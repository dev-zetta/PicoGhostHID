#ifndef GHOST_CONFIG_H_
#define GHOST_CONFIG_H_

#include <stdint.h>

// Pico has no battery-backed real-time clock by default, so this firmware uses
// elapsed time since boot rather than wall-clock shift/lunch scheduling.
#define GHOST_STARTUP_DELAY_MS     (10000u)
#define GHOST_MIN_IDLE_MS          (15000u)
#define GHOST_MAX_IDLE_MS          (60000u)
#define GHOST_KEY_MIN_IDLE_MS      (45000u)
#define GHOST_KEY_MAX_IDLE_MS      (180000u)

#define GHOST_MOVE_MIN_DELTA       (-100)
#define GHOST_MOVE_MAX_DELTA       (100)
#define GHOST_MOVE_STEPS           (50u)
#define GHOST_STEP_MIN_DELAY_MS    (8u)
#define GHOST_STEP_MAX_DELAY_MS    (15u)
#define GHOST_MOVE_CONTROL_JITTER  (20)
#define GHOST_MOVE_WOBBLE_MAX      (3)

#define GHOST_JITTER_STEPS         (5u)
#define GHOST_JITTER_DELAY_MS      (100u)

#define GHOST_ACTIVITY_LED_PULSE_MS (80u)
#define GHOST_KEY_MIN_HOLD_MS      (40u)
#define GHOST_KEY_MAX_HOLD_MS      (120u)

#ifndef GHOST_START_PAUSED
#define GHOST_START_PAUSED          (0u)
#endif

#ifndef GHOST_BOOTSEL_PAUSE_ENABLED
#define GHOST_BOOTSEL_PAUSE_ENABLED (1u)
#endif
#define GHOST_BOOTSEL_POLL_MS       (20u)
#define GHOST_BOOTSEL_DEBOUNCE_MS   (300u)

typedef enum {
    GHOST_PROFILE_MOUSE_ONLY = 0,
    GHOST_PROFILE_MOUSE_AND_KEYBOARD = 1,
} ghost_profile_t;

// Keyboard mode runs a separate random key-action emitter from mouse movement.
#define GHOST_DEFAULT_PROFILE GHOST_PROFILE_MOUSE_AND_KEYBOARD

#endif
