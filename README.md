# PicoGhostHID

PicoGhostHID is a Raspberry Pi Pico rewrite of the original GhostHID idea. It
does not use Linux USB gadget mode. The Pico firmware presents itself directly
as a USB HID keyboard/mouse device through TinyUSB.

## Why This Is a Rewrite

The original GhostHID project targets Raspberry Pi Zero Linux:

- `setup_hid.sh` creates a ConfigFS USB gadget under `/sys/kernel/config`.
- `ghost_sim.py` writes HID reports to `/dev/hidg0` and `/dev/hidg1`.
- The installer deploys over SSH to a Linux Pi.

The Pico has no Linux kernel, shell, SSH service, Python runtime, or `/dev/hidg`
devices. This project replaces those pieces with native firmware.

## Current Status

- USB HID composite descriptor: keyboard and mouse
- Logitech-style VID/PID and product strings
- Mouse movement using the original Bezier-style behavior
- Random jitter/nudge actions
- Keyboard report helper present, but disabled by default
- UF2 firmware build through Pico SDK

Scheduling from the original `config.ini` is intentionally not copied yet. A
plain Pico has no battery-backed real-time clock, so wall-clock shift and lunch
logic needs either host-provided time, persisted configuration, or an external
RTC module.

## Build

Install or clone the Raspberry Pi Pico SDK, then configure with `PICO_SDK_PATH`:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
./scripts/build.sh
```

The firmware will be produced as:

```text
build/PicoGhostHID.uf2
```

To flash it, hold the Pico `BOOTSEL` button while plugging it in, then copy the
UF2 file to the mounted `RPI-RP2` drive.

## Configuration

Edit `include/ghost_config.h` before building.

Useful knobs:

- `GHOST_STARTUP_DELAY_MS`
- `GHOST_MIN_IDLE_MS`
- `GHOST_MAX_IDLE_MS`
- `GHOST_DEFAULT_PROFILE`

The default profile is mouse-only so the firmware does not type into whichever
host window is focused.

## Project Layout

```text
PicoGhostHID/
  CMakeLists.txt
  pico_sdk_import.cmake
  include/
    ghost_config.h
    tusb_config.h
    usb_descriptors.h
  src/
    main.c
    usb_descriptors.c
```

## Next Steps

- Add a physical enable/disable button.
- Add flash-backed configuration.
- Add host-side config tooling that writes settings over USB vendor or CDC.
- Add optional RTC-backed shift/lunch scheduling.
- Split keyboard text generation into a separate, opt-in behavior module.
