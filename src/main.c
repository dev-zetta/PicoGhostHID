#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board.h"
#include "hardware/structs/rosc.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "ghost_config.h"
#include "usb_descriptors.h"

typedef enum {
    ACTION_MOVE,
    ACTION_JITTER,
    ACTION_NUDGE
} ghost_action_t;

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

static bool send_mouse_report(int8_t x, int8_t y, int8_t wheel) {
    if (!tud_mounted() || !tud_hid_ready()) {
        return false;
    }

    tud_hid_mouse_report(REPORT_ID_MOUSE, 0, x, y, wheel, 0);
    return true;
}

static bool send_keyboard_key(uint8_t modifier, uint8_t keycode) {
    if (!tud_mounted() || !tud_hid_ready()) {
        return false;
    }

    uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycodes);
    return true;
}

static void release_keyboard(void) {
    if (tud_mounted() && tud_hid_ready()) {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
    }
}

static void usb_sleep_ms(uint32_t duration_ms) {
    absolute_time_t deadline = make_timeout_time_ms(duration_ms);
    while (!time_reached(deadline)) {
        tud_task();
        sleep_ms(1);
    }
}

static void move_mouse_humanly(int dest_x, int dest_y) {
    int control_x = dest_x / 2 + random_range(-20, 20);
    int control_y = dest_y / 2 + random_range(-20, 20);
    int last_x = 0;
    int last_y = 0;

    for (uint32_t step = 1; step <= GHOST_MOVE_STEPS; step++) {
        float t = (float)step / (float)GHOST_MOVE_STEPS;
        float inv_t = 1.0f - t;
        int target_x = (int)((inv_t * inv_t * 0.0f) +
                             (2.0f * inv_t * t * (float)control_x) +
                             (t * t * (float)dest_x));
        int target_y = (int)((inv_t * inv_t * 0.0f) +
                             (2.0f * inv_t * t * (float)control_y) +
                             (t * t * (float)dest_y));

        send_mouse_report(clamp_i8(target_x - last_x), clamp_i8(target_y - last_y), 0);
        last_x = target_x;
        last_y = target_y;
        usb_sleep_ms(clamp_delay(GHOST_STEP_MIN_DELAY_MS, GHOST_STEP_MAX_DELAY_MS));
    }
}

static void jitter_mouse(void) {
    for (uint32_t i = 0; i < GHOST_JITTER_STEPS; i++) {
        send_mouse_report((int8_t)random_range(-1, 1), (int8_t)random_range(-1, 1), 0);
        usb_sleep_ms(GHOST_JITTER_DELAY_MS);
    }
}

static void nudge_mouse(void) {
    send_mouse_report(0, 1, 0);
    usb_sleep_ms(200);
    send_mouse_report(0, -1, 0);
}

static void simulate_activity(void) {
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

    if (GHOST_DEFAULT_PROFILE == GHOST_PROFILE_MOUSE_AND_KEYBOARD) {
        send_keyboard_key(0, HID_KEY_SPACE);
        usb_sleep_ms(clamp_delay(40, 120));
        release_keyboard();
    }
}

int main(void) {
    board_init();
    tusb_init();

    usb_sleep_ms(GHOST_STARTUP_DELAY_MS);

    while (true) {
        tud_task();

        if (tud_mounted()) {
            simulate_activity();
            usb_sleep_ms(clamp_delay(GHOST_MIN_IDLE_MS, GHOST_MAX_IDLE_MS));
        } else {
            sleep_ms(10);
        }
    }
}
