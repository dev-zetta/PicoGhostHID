# PicoGhostHID

PicoGhostHID is native Raspberry Pi Pico firmware that makes a Pico appear as a
USB HID keyboard and mouse. It can periodically move the mouse, send small
random keyboard actions, follow an active-hours schedule, and pause/resume from
the Pico `BOOTSEL` button.

It runs directly on Pico hardware through TinyUSB. No Linux USB gadget mode,
Python runtime, SSH setup, or host-side service is required after flashing.

## Download Firmware

Ready-built UF2 firmware files are published in the GitHub Releases section.
Download the file for your board, put the Pico into `BOOTSEL` mode, and copy
the UF2 onto the mounted boot drive:

- `PicoGhostHID-pico1.uf2` for Raspberry Pi Pico / RP2040
- `PicoGhostHID-pico2.uf2` for Raspberry Pi Pico 2 / RP2350

## Features

- USB HID composite keyboard and mouse device
- Pico 1 and Pico 2 firmware builds
- Logitech-style VID/PID and product strings
- Smooth Bezier-style mouse movement with jitter/nudge actions
- Independent randomized keyboard action emitter
- Active-hours and lunch-break schedule configured over HID feature reports
- `BOOTSEL` button pause/resume with LED status
- GitHub Actions builds and tagged release artifacts

The Pico has no battery-backed clock by default. Time and schedule settings are
volatile and must be synced again after reset or unplug.

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

## GitHub Actions

The repository includes a firmware workflow that builds both Pico targets on
pushes, pull requests, and manual runs. Each run uploads UF2 and ELF artifacts:

- `PicoGhostHID-pico1.uf2`
- `PicoGhostHID-pico1.elf`
- `PicoGhostHID-pico2.uf2`
- `PicoGhostHID-pico2.elf`

Pushing a tag that starts with `v` creates an automated GitHub release and
attaches those firmware files:

```sh
git tag v0.1.0
git push origin v0.1.0
```

## Configuration

Edit `include/ghost_config.h` before building.

Useful knobs:

- `GHOST_STARTUP_DELAY_MS`
- `GHOST_MIN_IDLE_MS`
- `GHOST_MAX_IDLE_MS`
- `GHOST_KEY_MIN_IDLE_MS`
- `GHOST_KEY_MAX_IDLE_MS`
- `GHOST_MOVE_CONTROL_JITTER`
- `GHOST_MOVE_WOBBLE_MAX`
- `GHOST_DEFAULT_PROFILE`
- `GHOST_USB_SERIAL`, passed to CMake as the HID USB serial string
- `GHOST_ACTIVITY_LED_PULSE_MS`
- `GHOST_BOOTSEL_PAUSE_ENABLED`

The default profile runs mouse movement and keyboard activity as separate
random emitters. The current key action set is Space, Escape, Tab, Alt+Tab,
Windows/Super, Shift+Tab, and Ctrl+Esc. Keep in mind the key press goes to
whichever host window is focused.

With `GHOST_BOOTSEL_PAUSE_ENABLED` set, pressing the Pico `BOOTSEL` button
while the firmware is running toggles pause/resume. Paused mode suppresses HID
activity and holds the onboard LED on. This uses the Pico SDK/TinyUSB
flash-chip-select sampling trick rather than a normal GPIO input, so it is best
treated as a temporary convenience. A dedicated GPIO button is the more robust
long-term physical pause control.

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
  LICENSE
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

## License

PicoGhostHID is licensed under the GNU General Public License version 3 or
later. See `LICENSE` for the full license text.

## Next Steps

- Add a physical enable/disable button.
- Add flash-backed configuration.
- Add persistent schedule configuration.
- Add optional external RTC support for timekeeping across unplug/reset.
- Split keyboard text generation into a separate, opt-in behavior module.
