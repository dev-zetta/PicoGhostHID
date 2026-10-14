# Changelog

## [0.2.0] - 2026-10-14

### Added

- BIGTREETECH HBB / HBB Fe V1.0 support with a dedicated Pico SDK board definition, native/Docker build target, upload wrapper and CI/release artifacts.
- HBB starts paused after every reset. K1 (the lower-right key, GPIO25) toggles pause/resume; all seven RGB LEDs show red while paused, turn off while idle and pulse green during activity. K2-K7 remain unassigned, and the rear BOOT button is reserved for firmware updates.
- A multi-stage Docker build that caches dependency installation and SDK fetching, then exports release-ready firmware and license notices directly to the host.
- A shared Ubuntu/Debian package manifest and installer used by native and Docker builds, including the ARM C++ standard library required by clean build environments.

### Changed

- Unified native and Docker builds under `scripts/build.sh`, with `all`, board aliases and `--output` support. Both modes export consistently named UF2/ELF files and license notices to `dist/`; existing board wrappers forward options to the shared implementation.
- GitHub Actions now builds all four boards through Docker using the shared dependency manifest and artifact packaging.
- The WS2812 driver supports board-defined chain lengths, preserves a reset/latch interval between frames and suppresses repeated writes of an unchanged LED state. RP2040-Zero retains its single-pixel status behavior.

## [0.1.1] - 2026-10-14

### Fixed

- Corrected HID status and configuration reports by increasing TinyUSB's buffer to 64 bytes, with a compile-time check requiring space for the 63-byte payload and report ID. Fixes [issue #1](https://github.com/dev-zetta/PicoGhostHID/issues/1).

### Added

- Waveshare RP2040-Zero support with a WS2812 status LED, build/upload scripts and CI/release artifacts, contributed by [Trotter73](https://github.com/Trotter73). The original commit authorship and README credits are preserved.
- Firmware version metadata and GPL/BSD license notices in release downloads.

### Changed

- Default randomized keyboard activity now uses Shift+F13 and Shift+F14. Applications and global shortcuts can still bind these keys.

## [0.1.0] - 2026-07-09

- Initial release for Raspberry Pi Pico and Pico 2, with composite USB keyboard/mouse activity, volatile time/schedule configuration, BOOTSEL pause/resume and automated UF2/ELF release builds.

[0.2.0]: https://github.com/dev-zetta/PicoGhostHID/compare/v0.1.1...v0.2.0
[0.1.1]: https://github.com/dev-zetta/PicoGhostHID/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/dev-zetta/PicoGhostHID/releases/tag/v0.1.0
