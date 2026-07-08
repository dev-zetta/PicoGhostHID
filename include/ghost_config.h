#ifndef GHOST_CONFIG_H_
#define GHOST_CONFIG_H_

#include <stdint.h>

// Pico has no battery-backed real-time clock by default, so this firmware uses
// elapsed time since boot rather than wall-clock shift/lunch scheduling.
#define GHOST_STARTUP_DELAY_MS     (10000u)
#define GHOST_MIN_IDLE_MS          (30000u)
#define GHOST_MAX_IDLE_MS          (120000u)

#define GHOST_MOVE_MIN_DELTA       (-100)
#define GHOST_MOVE_MAX_DELTA       (100)
#define GHOST_MOVE_STEPS           (50u)
#define GHOST_STEP_MIN_DELAY_MS    (8u)
#define GHOST_STEP_MAX_DELAY_MS    (15u)

#define GHOST_JITTER_STEPS         (5u)
#define GHOST_JITTER_DELAY_MS      (100u)

typedef enum {
    GHOST_PROFILE_MOUSE_ONLY = 0,
    GHOST_PROFILE_MOUSE_AND_KEYBOARD = 1,
} ghost_profile_t;

// Keyboard reports are available, but the default behavior stays mouse-only to
// avoid typing into whichever host window happens to be focused.
#define GHOST_DEFAULT_PROFILE GHOST_PROFILE_MOUSE_ONLY

#endif
