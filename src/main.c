#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/rosc.h"
#include "hardware/structs/sio.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "ghost_config.h"
#include "ghost_led.h"
#include "ghost_runtime.h"
#include "usb_descriptors.h"

typedef enum {
    ACTION_MOVE,
    ACTION_JITTER,
    ACTION_NUDGE
} ghost_action_t;

typedef struct {
    uint8_t modifier;
    uint8_t keycode;
} ghost_key_action_t;

static const ghost_key_action_t key_actions[] = {
    {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_F13},  // harmless in almost every app
    {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_F14},
};

static absolute_time_t activity_led_off_at;
static bool activity_led_active = false;
static bool ghost_paused = false;
static bool bootsel_was_pressed = false;
static uint32_t bootsel_last_poll_ms = 0;
static uint32_t bootsel_last_toggle_ms = 0;
static uint32_t synced_seconds_of_day = 0;
static uint64_t synced_at_us = 0;

typedef struct {
    bool time_valid;
    bool schedule_enabled;
    uint16_t active_start_minute;
    uint16_t active_end_minute;
    uint16_t lunch_start_minute;
    uint16_t lunch_duration_minutes;
} ghost_runtime_config_t;

static ghost_runtime_config_t runtime_config = {
    .time_valid = false,
    .schedule_enabled = false,
    .active_start_minute = 9u * 60u,
    .active_end_minute = 17u * 60u,
    .lunch_start_minute = 12u * 60u,
    .lunch_duration_minutes = 60u,
};

enum {
    CONFIG_CMD_SET_TIME = 1,
    CONFIG_CMD_SET_SCHEDULE = 2,
    CONFIG_CMD_SET_TIME_AND_SCHEDULE = 3,
    CONFIG_STATUS_RESPONSE = 0x80,
};

static uint16_t read_le16(uint8_t const *buffer) {
    return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8u);
}

static void write_le16(uint8_t *buffer, uint16_t value) {
    buffer[0] = (uint8_t)(value & 0xffu);
    buffer[1] = (uint8_t)(value >> 8u);
}

static bool valid_minute(uint16_t minute) {
    return minute < (24u * 60u);
}

static bool minute_in_window(uint16_t minute, uint16_t start, uint16_t end) {
    if (start <= end) {
        return minute >= start && minute <= end;
    }
    return minute >= start || minute <= end;
}

static bool minute_in_span(uint16_t minute, uint16_t start, uint16_t duration) {
    if (duration == 0u) {
        return false;
    }
    if (duration >= 24u * 60u) {
        return true;
    }

    uint16_t end = (uint16_t)((start + duration) % (24u * 60u));
    if (start < end) {
        return minute >= start && minute < end;
    }
    return minute >= start || minute < end;
}

static uint32_t current_seconds_of_day(void) {
    if (!runtime_config.time_valid) {
        return 0;
    }

    uint64_t elapsed_seconds = (time_us_64() - synced_at_us) / 1000000u;
    return (uint32_t)((synced_seconds_of_day + elapsed_seconds) % (24u * 60u * 60u));
}

static bool set_time_from_report(uint8_t const *buffer, uint16_t bufsize) {
    if (bufsize < 13u) {
        return false;
    }

    uint16_t year = read_le16(&buffer[6]);
    uint8_t month = buffer[8];
    uint8_t day = buffer[9];
    uint8_t hour = buffer[10];
    uint8_t minute = buffer[11];
    uint8_t second = buffer[12];

    if (year < 2024u || year > 2099u || month < 1u || month > 12u ||
        day < 1u || day > 31u || hour > 23u || minute > 59u || second > 59u) {
        return false;
    }

    synced_seconds_of_day = (uint32_t)hour * 3600u + (uint32_t)minute * 60u + second;
    synced_at_us = time_us_64();
    runtime_config.time_valid = true;
    return true;
}

static bool set_schedule_from_report(uint8_t const *buffer, uint16_t offset, uint16_t bufsize) {
    if (bufsize < offset + 9u) {
        return false;
    }

    bool enabled = buffer[offset] != 0u;
    uint16_t active_start = read_le16(&buffer[offset + 1u]);
    uint16_t active_end = read_le16(&buffer[offset + 3u]);
    uint16_t lunch_start = read_le16(&buffer[offset + 5u]);
    uint16_t lunch_duration = read_le16(&buffer[offset + 7u]);

    if (!valid_minute(active_start) || !valid_minute(active_end) ||
        !valid_minute(lunch_start) || lunch_duration > 24u * 60u) {
        return false;
    }

    runtime_config.schedule_enabled = enabled;
    runtime_config.active_start_minute = active_start;
    runtime_config.active_end_minute = active_end;
    runtime_config.lunch_start_minute = lunch_start;
    runtime_config.lunch_duration_minutes = lunch_duration;
    return true;
}

static bool schedule_allows_activity(void) {
    if (ghost_paused) {
        return false;
    }
    if (!runtime_config.schedule_enabled) {
        return true;
    }
    if (!runtime_config.time_valid) {
        return false;
    }

    uint16_t minute = (uint16_t)(current_seconds_of_day() / 60u);
    if (!minute_in_window(minute, runtime_config.active_start_minute,
                          runtime_config.active_end_minute)) {
        return false;
    }
    if (minute_in_span(minute, runtime_config.lunch_start_minute,
                       runtime_config.lunch_duration_minutes)) {
        return false;
    }

    return true;
}

static bool __no_inline_not_in_flash_func(bootsel_button_pressed)(void) {
#if GHOST_BOOTSEL_PAUSE_ENABLED
    const uint cs_pin_index = 1u;
    uint32_t flags = save_and_disable_interrupts();

    hw_write_masked(&ioqspi_hw->io[cs_pin_index].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    for (volatile int i = 0; i < 1000; ++i) {
    }

#ifdef __ARM_ARCH_6M__
    const uint32_t cs_bit = 1u << 1u;
#else
    const uint32_t cs_bit = SIO_GPIO_HI_IN_QSPI_CSN_BITS;
#endif
    bool cs_high = (sio_hw->gpio_hi_in & cs_bit) != 0u;

    hw_write_masked(&ioqspi_hw->io[cs_pin_index].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);

    return !cs_high;
#else
    return false;
#endif
}

static void pause_button_task(void) {
#if GHOST_BOOTSEL_PAUSE_ENABLED
    uint32_t now_ms = time_us_32() / 1000u;
    if ((uint32_t)(now_ms - bootsel_last_poll_ms) < GHOST_BOOTSEL_POLL_MS) {
        return;
    }
    bootsel_last_poll_ms = now_ms;

    bool pressed = bootsel_button_pressed();
    if (pressed && !bootsel_was_pressed &&
        (uint32_t)(now_ms - bootsel_last_toggle_ms) >= GHOST_BOOTSEL_DEBOUNCE_MS) {
        ghost_paused = !ghost_paused;
        bootsel_last_toggle_ms = now_ms;
        ghost_led_write(ghost_paused ? GHOST_LED_PAUSED : GHOST_LED_OFF);
        activity_led_active = false;
    }
    bootsel_was_pressed = pressed;
#endif
}

void ghost_handle_feature_report(uint8_t const *buffer, uint16_t bufsize) {
    if (bufsize < 6u || memcmp(buffer, "PGHC", 4u) != 0 || buffer[4] != 1u) {
        return;
    }

    switch (buffer[5]) {
        case CONFIG_CMD_SET_TIME:
            (void)set_time_from_report(buffer, bufsize);
            break;
        case CONFIG_CMD_SET_SCHEDULE:
            (void)set_schedule_from_report(buffer, 6u, bufsize);
            break;
        case CONFIG_CMD_SET_TIME_AND_SCHEDULE:
            if (set_time_from_report(buffer, bufsize)) {
                (void)set_schedule_from_report(buffer, 13u, bufsize);
            }
            break;
        default:
            break;
    }
}

uint16_t ghost_fill_feature_report(uint8_t *buffer, uint16_t reqlen) {
    if (reqlen < GHOST_CONFIG_FEATURE_SIZE) {
        return 0;
    }

    memset(buffer, 0, GHOST_CONFIG_FEATURE_SIZE);
    memcpy(buffer, "PGHC", 4u);
    buffer[4] = 1u;
    buffer[5] = CONFIG_STATUS_RESPONSE;

    bool active = schedule_allows_activity();
    buffer[6] = (runtime_config.time_valid ? 0x01u : 0u) |
                (runtime_config.schedule_enabled ? 0x02u : 0u) |
                (active ? 0x04u : 0u) |
                (ghost_paused ? 0x08u : 0u);

    if (runtime_config.time_valid) {
        uint32_t seconds = current_seconds_of_day();
        buffer[7] = (uint8_t)(seconds / 3600u);
        buffer[8] = (uint8_t)((seconds / 60u) % 60u);
        buffer[9] = (uint8_t)(seconds % 60u);
    } else {
        buffer[7] = 0xffu;
        buffer[8] = 0xffu;
        buffer[9] = 0xffu;
    }

    write_le16(&buffer[10], runtime_config.active_start_minute);
    write_le16(&buffer[12], runtime_config.active_end_minute);
    write_le16(&buffer[14], runtime_config.lunch_start_minute);
    write_le16(&buffer[16], runtime_config.lunch_duration_minutes);

    return GHOST_CONFIG_FEATURE_SIZE;
}

static void activity_led_pulse(void) {
    ghost_led_write(GHOST_LED_ACTIVITY);
    activity_led_active = true;
    activity_led_off_at = make_timeout_time_ms(GHOST_ACTIVITY_LED_PULSE_MS);
}

static void activity_led_task(void) {
    if (ghost_paused) {
        ghost_led_write(GHOST_LED_PAUSED);
        return;
    }
    if (activity_led_active && time_reached(activity_led_off_at)) {
        ghost_led_write(GHOST_LED_OFF);
        activity_led_active = false;
    }
}

static uint32_t ghost_rand_32(void) {
    uint32_t value = 0;
    for (uint32_t i = 0; i < 32; i++) {
        value = (value << 1u) | (rosc_hw->randombit & 1u);
    }
    return value ^ time_us_32();
}

static uint32_t clamp_delay(uint32_t minimum_ms, uint32_t maximum_ms) {
    if (maximum_ms <= minimum_ms) {
        return minimum_ms;
    }

    return minimum_ms + (uint32_t)(ghost_rand_32() % (maximum_ms - minimum_ms + 1u));
}

static absolute_time_t random_timeout(uint32_t minimum_ms, uint32_t maximum_ms) {
    return make_timeout_time_ms(clamp_delay(minimum_ms, maximum_ms));
}

static int random_range(int minimum, int maximum) {
    if (maximum <= minimum) {
        return minimum;
    }

    uint32_t span = (uint32_t)(maximum - minimum + 1);
    return minimum + (int)(ghost_rand_32() % span);
}

static int8_t clamp_i8(int value) {
    if (value > 127) {
        return 127;
    }
    if (value < -127) {
        return -127;
    }
    return (int8_t)value;
}

static int round_to_i32(float value) {
    return (int)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

static float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static bool send_mouse_report(int8_t x, int8_t y, int8_t wheel) {
    if (!tud_mounted() || !tud_hid_ready()) {
        return false;
    }

    tud_hid_mouse_report(REPORT_ID_MOUSE, 0, x, y, wheel, 0);
    activity_led_pulse();
    return true;
}

static bool send_keyboard_action(ghost_key_action_t action) {
    if (!tud_mounted() || !tud_hid_ready()) {
        return false;
    }

    uint8_t keycodes[6] = {action.keycode, 0, 0, 0, 0, 0};
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, action.modifier, keycodes);
    activity_led_pulse();
    return true;
}

static void release_keyboard(void) {
    if (tud_mounted() && tud_hid_ready()) {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        activity_led_pulse();
    }
}

static void usb_sleep_ms(uint32_t duration_ms) {
    absolute_time_t deadline = make_timeout_time_ms(duration_ms);
    while (!time_reached(deadline)) {
        tud_task();
        pause_button_task();
        activity_led_task();
        sleep_ms(1);
    }
}

static void send_random_keyboard_action(void) {
    size_t action_count = sizeof(key_actions) / sizeof(key_actions[0]);
    ghost_key_action_t action = key_actions[ghost_rand_32() % action_count];

    if (send_keyboard_action(action)) {
        usb_sleep_ms(clamp_delay(GHOST_KEY_MIN_HOLD_MS, GHOST_KEY_MAX_HOLD_MS));
        release_keyboard();
    }
}

static void move_mouse_humanly(int dest_x, int dest_y) {
    float control_x = (float)dest_x * 0.5f +
                      (float)random_range(-GHOST_MOVE_CONTROL_JITTER, GHOST_MOVE_CONTROL_JITTER);
    float control_y = (float)dest_y * 0.5f +
                      (float)random_range(-GHOST_MOVE_CONTROL_JITTER, GHOST_MOVE_CONTROL_JITTER);
    int sent_x = 0;
    int sent_y = 0;

    int wobble = random_range(-GHOST_MOVE_WOBBLE_MAX, GHOST_MOVE_WOBBLE_MAX);
    bool wobble_on_x = abs(dest_y) >= abs(dest_x);
    float wobble_x = wobble_on_x ? (float)wobble : 0.0f;
    float wobble_y = wobble_on_x ? 0.0f : (float)wobble;

    for (uint32_t step = 1; step <= GHOST_MOVE_STEPS; step++) {
        if (!schedule_allows_activity()) {
            break;
        }

        float raw_t = (float)step / (float)GHOST_MOVE_STEPS;
        float t = smoothstep(raw_t);
        float inv_t = 1.0f - t;

        float target_x = (2.0f * inv_t * t * control_x) + (t * t * (float)dest_x);
        float target_y = (2.0f * inv_t * t * control_y) + (t * t * (float)dest_y);

        float wobble_envelope = raw_t * (1.0f - raw_t) * 4.0f;
        target_x += wobble_x * wobble_envelope;
        target_y += wobble_y * wobble_envelope;

        int rounded_x = round_to_i32(target_x);
        int rounded_y = round_to_i32(target_y);
        int delta_x = rounded_x - sent_x;
        int delta_y = rounded_y - sent_y;

        if (delta_x != 0 || delta_y != 0) {
            int8_t report_x = clamp_i8(delta_x);
            int8_t report_y = clamp_i8(delta_y);
            send_mouse_report(report_x, report_y, 0);
            sent_x += report_x;
            sent_y += report_y;
        }
        usb_sleep_ms(clamp_delay(GHOST_STEP_MIN_DELAY_MS, GHOST_STEP_MAX_DELAY_MS));
    }
}

static void jitter_mouse(void) {
    for (uint32_t i = 0; i < GHOST_JITTER_STEPS; i++) {
        if (!schedule_allows_activity()) {
            break;
        }
        send_mouse_report((int8_t)random_range(-1, 1), (int8_t)random_range(-1, 1), 0);
        usb_sleep_ms(GHOST_JITTER_DELAY_MS);
    }
}

static void nudge_mouse(void) {
    if (!schedule_allows_activity()) {
        return;
    }
    send_mouse_report(0, 1, 0);
    usb_sleep_ms(200);
    if (!schedule_allows_activity()) {
        return;
    }
    send_mouse_report(0, -1, 0);
}

static void simulate_mouse_activity(void) {
    if (!schedule_allows_activity()) {
        return;
    }

    ghost_action_t action = (ghost_action_t)random_range(ACTION_MOVE, ACTION_NUDGE);

    switch (action) {
        case ACTION_MOVE:
            move_mouse_humanly(random_range(GHOST_MOVE_MIN_DELTA, GHOST_MOVE_MAX_DELTA),
                               random_range(GHOST_MOVE_MIN_DELTA, GHOST_MOVE_MAX_DELTA));
            break;
        case ACTION_JITTER:
            jitter_mouse();
            break;
        case ACTION_NUDGE:
        default:
            nudge_mouse();
            break;
    }
}

int main(void) {
    board_init();
    tusb_init();
    ghost_led_init();

    usb_sleep_ms(GHOST_STARTUP_DELAY_MS);

    absolute_time_t next_mouse_at = random_timeout(GHOST_MIN_IDLE_MS, GHOST_MAX_IDLE_MS);
    absolute_time_t next_keyboard_at = random_timeout(GHOST_KEY_MIN_IDLE_MS, GHOST_KEY_MAX_IDLE_MS);

    while (true) {
        tud_task();
        pause_button_task();
        activity_led_task();

        if (tud_mounted() && schedule_allows_activity()) {
            if (time_reached(next_mouse_at)) {
                simulate_mouse_activity();
                next_mouse_at = random_timeout(GHOST_MIN_IDLE_MS, GHOST_MAX_IDLE_MS);
            }

            if (GHOST_DEFAULT_PROFILE == GHOST_PROFILE_MOUSE_AND_KEYBOARD &&
                schedule_allows_activity() && time_reached(next_keyboard_at)) {
                send_random_keyboard_action();
                next_keyboard_at = random_timeout(GHOST_KEY_MIN_IDLE_MS, GHOST_KEY_MAX_IDLE_MS);
            }
        } else {
            next_mouse_at = random_timeout(GHOST_MIN_IDLE_MS, GHOST_MAX_IDLE_MS);
            next_keyboard_at = random_timeout(GHOST_KEY_MIN_IDLE_MS, GHOST_KEY_MAX_IDLE_MS);
        }

        sleep_ms(10);
    }
}
