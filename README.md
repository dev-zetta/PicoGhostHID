# PicoGhostHID

PicoGhostHID is native Raspberry Pi Pico firmware that makes a Pico appear as a
USB HID keyboard and mouse. It can periodically move the mouse, send small
random keyboard actions, follow an active-hours schedule, and pause/resume from
the Pico `BOOTSEL` button.

It runs directly on Pico hardware through TinyUSB. No Linux USB gadget mode,
Python runtime, SSH setup, or host-side service is required after flashing.

## Download Firmware

Ready-built UF2 firmware files for all three boards are published in [release v0.1.1](https://github.com/dev-zetta/PicoGhostHID/releases/tag/v0.1.1). Download the file for your board, put the board into `BOOTSEL` mode, and copy the UF2 onto the mounted boot drive.

- `PicoGhostHID-pico1.uf2` for Raspberry Pi Pico / RP2040
- `PicoGhostHID-pico2.uf2` for Raspberry Pi Pico 2 / RP2350
- `PicoGhostHID-rp2040zero.uf2` for Waveshare RP2040-Zero

## Features

- USB HID composite keyboard and mouse device
- Pico 1, Pico 2, and Waveshare RP2040-Zero firmware builds
- Logitech-style VID/PID and product strings
- Smooth Bezier-style mouse movement with jitter/nudge actions
- Independent randomized keyboard action emitter using Shift+F13 and Shift+F14
- Status LED: plain on/off on Pico/Pico2, green activity pulse / solid red paused on the RP2040-Zero's onboard WS2812
- Active-hours and lunch-break schedule configured over HID feature reports
- `BOOTSEL` button pause/resume with LED status
- GitHub Actions builds and tagged release artifacts

The Pico has no battery-backed clock by default. Time and schedule settings are
volatile and must be synced again after reset or unplug.

## Build

Use `scripts/build.sh` for both native and Docker builds. It accepts `all` (the default), `pico1`, `pico2`, or `rp2040zero`; the Pico SDK names `pico` and `waveshare_rp2040_zero` are also accepted. The existing board-specific build scripts remain compatibility wrappers around this command.

### Docker

Install Docker with BuildKit, then build all three boards without installing the ARM toolchain or Pico SDK on the host:

```sh
./scripts/build.sh --docker all
```

Build only one board or choose an export directory:

```sh
./scripts/build.sh --docker pico2 --output dist/pico2
```

The Dockerfile installs packages through the shared dependency installer and fetches the SDK in cached layers. Docker's [local exporter](https://docs.docker.com/build/building/export/) writes the firmware directly to the chosen directory. Local SDK checkouts, build caches and private configuration are excluded from the build context.

The equivalent direct Docker command is:

```sh
docker build --build-arg BOARD=all --output dist .
```

### Native

On Ubuntu/Debian, install the build tools once using the same package manifest consumed by Docker:

```sh
sudo ./scripts/install_build_deps.sh
./scripts/build.sh all
```

`scripts/build-deps.txt` is the single list of required system packages, including the ARM compiler and the dependencies needed to build `picotool`. CI builds through Docker using this same manifest. To change a package requirement, edit that file.

The build command fetches the official [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) when no local checkout exists. `scripts/fetch_sdk.sh` defines the default SDK release, currently `2.3.0`. To select another SDK release, fetch it explicitly before a native build:

```sh
PICO_SDK_VERSION=master ./scripts/fetch_sdk.sh
./scripts/build.sh pico2
```

Or point at an existing SDK checkout:

```sh
PICO_SDK_PATH=/path/to/pico-sdk ./scripts/build.sh pico2
```

### Outputs and options

Both build modes export release-ready files and license notices into `dist/` by default:

```text
dist/PicoGhostHID-pico1.uf2
dist/PicoGhostHID-pico1.elf
dist/PicoGhostHID-pico2.uf2
dist/PicoGhostHID-pico2.elf
dist/PicoGhostHID-rp2040zero.uf2
dist/PicoGhostHID-rp2040zero.elf
dist/LICENSE.txt
dist/BSD-3-Clause-ws2812.txt
```

A single-board build exports that board's UF2/ELF and both license notices. Existing files for other boards in an export directory are preserved; use `--output` with a separate directory when you need an isolated set. Native builds also retain `build/<board>/PicoGhostHID.uf2` for the upload wrappers. Docker builds export only the release-ready files.

`GHOST_USB_SERIAL` and `CMAKE_BUILD_PARALLEL_LEVEL` work in either mode. Docker also accepts `PICO_SDK_VERSION` and `PICO_SDK_REPO` for its SDK checkout:

```sh
GHOST_USB_SERIAL=0123456789abcdef CMAKE_BUILD_PARALLEL_LEVEL=4 ./scripts/build.sh --docker pico2
PICO_SDK_VERSION=2.3.0 ./scripts/build.sh --docker all
```

If a native build directory was configured with a different compiler or checkout path, regenerate that board's directory before rebuilding:

```sh
cmake -E rm -rf build/pico2
./scripts/build.sh pico2
```

To flash either build mode, hold the board's `BOOTSEL` button while plugging it in and copy the matching UF2 to the mounted boot drive. Native builds can also use the upload wrappers:

```sh
./scripts/upload_pico1.sh
./scripts/upload_pico2.sh
./scripts/upload_rp2040zero.sh
```

## GitHub Actions

The repository includes a firmware workflow that builds all three boards on pushes, pull requests, and manual runs. Each run uploads UF2 and ELF artifacts together with the license notices:

- `PicoGhostHID-pico1.uf2`
- `PicoGhostHID-pico1.elf`
- `PicoGhostHID-pico2.uf2`
- `PicoGhostHID-pico2.elf`
- `PicoGhostHID-rp2040zero.uf2`
- `PicoGhostHID-rp2040zero.elf`

Pushing a tag that starts with `v` creates an automated GitHub release and
attaches those firmware files:

```sh
git tag v0.1.1
git push origin v0.1.1
```

## Configuration

`CFG_TUD_HID_EP_BUFSIZE` in `include/tusb_config.h` must hold `GHOST_CONFIG_FEATURE_SIZE` (63 payload bytes) plus the report ID: at least 64 bytes in total. TinyUSB uses this buffer for control transfers and removes the report ID before calling `tud_hid_get_report_cb()`/`tud_hid_set_report_cb()`. A smaller buffer breaks the HID feature reports used by `tools/picoghost_config.py` for `status`/`configure`; a compile-time assertion prevents this configuration.

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

The default profile runs mouse movement and keyboard activity as separate random emitters. Keyboard activity randomly sends Shift+F13 or Shift+F14. These avoid typing characters or switching windows with the previous defaults, but applications or global shortcuts can still bind them. Key presses go to whichever host window is focused.

With `GHOST_BOOTSEL_PAUSE_ENABLED` set, pressing the board's `BOOTSEL` button while the firmware is running toggles pause/resume. Paused mode suppresses HID activity and holds the onboard LED on (solid red on the RP2040-Zero); activity pulses green on the RP2040-Zero. This uses the Pico SDK/TinyUSB flash-chip-select sampling trick rather than a normal GPIO input, so it is best treated as a temporary convenience. A dedicated GPIO button is the more robust long-term physical pause control.

The default USB serial is `fedcba9876543210`, matching the original GhostHID
fallback value. You can override it at configure time:

```sh
cmake -E rm -rf build
GHOST_USB_SERIAL=0123456789abcdef ./scripts/build.sh pico2
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
    ghost_led.h
    ghost_runtime.h
    tusb_config.h
    usb_descriptors.h
  src/
    main.c
    usb_descriptors.c
    ghost_led.c
    ws2812.pio
  tools/
    picoghost_config.py
```

## License

PicoGhostHID is licensed under the GNU General Public License version 3 or later. See `LICENSE` for the full license text. The adapted WS2812 PIO routine retains its BSD 3-Clause notice in [LICENSES/BSD-3-Clause-ws2812.txt](LICENSES/BSD-3-Clause-ws2812.txt).

## Credits

- [Trotter73](https://github.com/Trotter73) contributed Waveshare RP2040-Zero support with its WS2812 status LED and build/upload scripts in [commit f282685](https://github.com/Trotter73/PicoGhostHID/commit/f2826851331850527f0187c503fe0b3a70143a87), reported and fixed the HID configuration buffer issue in [issue #1](https://github.com/dev-zetta/PicoGhostHID/issues/1), and changed the default keyboard actions to Shift+F13/F14. The integration preserves the original commit's authorship.
- Raspberry Pi (Trading) Ltd. provided the [Pico SDK](https://github.com/raspberrypi/pico-sdk) and the [WS2812 PIO example](https://github.com/raspberrypi/pico-examples/tree/master/pio/ws2812) adapted for the onboard RGB LED.
- [TinyUSB](https://github.com/hathach/tinyusb) provides the USB HID stack.

## Next Steps

- Add a physical enable/disable button.
- Add flash-backed configuration.
- Add persistent schedule configuration.
- Add optional external RTC support for timekeeping across unplug/reset.
- Split keyboard text generation into a separate, opt-in behavior module.
