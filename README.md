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

### Prerequisites

On Ubuntu/Debian, install the Pico build tools and ARM embedded compiler
system-wide:

```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  git \
  python3 \
  gcc-arm-none-eabi \
  binutils-arm-none-eabi \
  libnewlib-arm-none-eabi \
  libnewlib-dev
```

If CMake needs to build `picotool` from source on your machine, these packages
may also be required:

```sh
sudo apt-get install -y pkg-config libusb-1.0-0-dev
```

This project uses the official Raspberry Pi Pico C/C++ SDK:

```text
https://github.com/raspberrypi/pico-sdk
```

Fetch a local SDK checkout, then build:

```sh
./scripts/fetch_sdk.sh
./scripts/build_pico1.sh
./scripts/build_pico2.sh
```

By default, `fetch_sdk.sh` checks out SDK release `2.3.0` into
`deps/pico-sdk`. To use another release or branch:

```sh
PICO_SDK_VERSION=master ./scripts/fetch_sdk.sh
```

You can also skip `deps/pico-sdk` and point at an existing SDK checkout:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
./scripts/build_pico2.sh
```

The firmware will be produced as one of:

```text
build/pico1/PicoGhostHID.uf2
build/pico2/PicoGhostHID.uf2
```

If CMake was previously configured with a different compiler or toolchain path,
regenerate the relevant build directory before building again:

```sh
cmake -E rm -rf build/pico2
./scripts/build_pico2.sh
```

To flash it, hold the Pico `BOOTSEL` button while plugging it in, then copy the
UF2 file to the mounted `RPI-RP2` drive.

Or use the upload wrappers:

```sh
./scripts/upload_pico1.sh
./scripts/upload_pico2.sh
```

## Configuration

Edit `include/ghost_config.h` before building.

Useful knobs:

- `GHOST_STARTUP_DELAY_MS`
- `GHOST_MIN_IDLE_MS`
- `GHOST_MAX_IDLE_MS`
- `GHOST_MOVE_CONTROL_JITTER`
- `GHOST_MOVE_WOBBLE_MAX`
- `GHOST_DEFAULT_PROFILE`
- `GHOST_USB_SERIAL`, passed to CMake as the HID USB serial string
- `GHOST_ACTIVITY_LED_PULSE_MS`

The default profile sends mouse activity and a brief Space key press after each
mouse activity burst. Keep in mind the key press goes to whichever host window
is focused.

The default USB serial is `fedcba9876543210`, matching the original GhostHID
fallback value. You can override it at configure time:

```sh
cmake -E rm -rf build
GHOST_USB_SERIAL=0123456789abcdef ./scripts/build_pico2.sh
```

## USB Time And Schedule Config

The firmware accepts volatile time/schedule configuration through a
vendor-defined HID feature report on the existing keyboard/mouse HID interface.
It does not add a USB serial port.

Sync local host time and enable a 09:00-17:00 window with a 12:00 lunch break:

```sh
tools/picoghost_config.py configure --start 09:00 --end 17:00 --lunch 12:00 --lunch-duration 60
```

Read current status:

```sh
tools/picoghost_config.py status
```

Disable the schedule and return to always-active behavior while mounted:

```sh
tools/picoghost_config.py set-schedule --no-enable
```

The Pico has no battery-backed clock by default, so time and schedule state are
lost after reset or unplug.

## Project Layout

```text
PicoGhostHID/
  CMakeLists.txt
  pico_sdk_import.cmake
  include/
    ghost_config.h
    ghost_runtime.h
    tusb_config.h
    usb_descriptors.h
  src/
    main.c
    usb_descriptors.c
  tools/
    picoghost_config.py
```

## Next Steps

- Add a physical enable/disable button.
- Add flash-backed configuration.
- Add persistent schedule configuration.
- Add optional external RTC support for timekeeping across unplug/reset.
- Split keyboard text generation into a separate, opt-in behavior module.
