#ifndef GHOST_LED_H_
#define GHOST_LED_H_

// Small LED abstraction so main.c doesn't need to care whether the board
// has a plain on/off LED (Pico, Pico 2 -> board_led_write) or an
// addressable WS2812 (Waveshare RP2040-Zero -> PIO driver below).
typedef enum {
    GHOST_LED_OFF = 0,
    GHOST_LED_ACTIVITY,   // brief pulse on mouse/keyboard activity
    GHOST_LED_PAUSED,     // solid while BOOTSEL-paused
} ghost_led_state_t;

// Call once after board_init()/tusb_init(). No-op on boards with a plain
// GPIO LED; claims a PIO state machine on WS2812 boards.
void ghost_led_init(void);

// Set the current LED state/colour.
void ghost_led_write(ghost_led_state_t state);

#endif
