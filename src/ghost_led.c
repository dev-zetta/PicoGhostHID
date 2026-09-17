#include "ghost_led.h"

#include "bsp/board.h"

#if defined(WAVESHARE_RP2040_ZERO)

// --- WS2812 backend (Waveshare RP2040-Zero onboard RGB LED, GPIO16) -------

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "ws2812.pio.h"

#ifndef GHOST_WS2812_PIN
#define GHOST_WS2812_PIN PICO_DEFAULT_WS2812_PIN
#endif

// Keep it dim -- this sits on a desk, not a stage.
#ifndef GHOST_LED_BRIGHTNESS
#define GHOST_LED_BRIGHTNESS 32u
#endif

static PIO ghost_led_pio = pio0;
static uint ghost_led_sm;

static inline void put_pixel(uint32_t grb) {
    // WS2812 shift register is left-justified in the 32-bit FIFO word.
    pio_sm_put_blocking(ghost_led_pio, ghost_led_sm, grb << 8u);
}

static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16u) | ((uint32_t)r << 8u) | (uint32_t)b;
}

void ghost_led_init(void) {
    uint offset = pio_add_program(ghost_led_pio, &ws2812_program);
    ghost_led_sm = pio_claim_unused_sm(ghost_led_pio, true);
    ws2812_program_init(ghost_led_pio, ghost_led_sm, offset, GHOST_WS2812_PIN, 800000.0f, false);
    put_pixel(0);
}

void ghost_led_write(ghost_led_state_t state) {
    switch (state) {
        case GHOST_LED_ACTIVITY:
            put_pixel(rgb_to_grb(0, GHOST_LED_BRIGHTNESS, 0));  // green
            break;
        case GHOST_LED_PAUSED:
            put_pixel(rgb_to_grb(GHOST_LED_BRIGHTNESS, 0, 0));  // red
            break;
        case GHOST_LED_OFF:
        default:
            put_pixel(0);
            break;
    }
}

#else

// --- Plain on/off backend (Raspberry Pi Pico / Pico 2) --------------------

void ghost_led_init(void) {
    // board_init() already configured PICO_DEFAULT_LED_PIN via the
    // Pico SDK's TinyUSB board support; nothing else to do here.
}

void ghost_led_write(ghost_led_state_t state) {
    board_led_write(state != GHOST_LED_OFF);
}

#endif
