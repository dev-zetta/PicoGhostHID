// BIGTREETECH HBB / HBB Fe V1.0.
// Pin and flash configuration: https://github.com/bigtreetech/HBB
// This header is also consumed by the assembler and Pico SDK CMake parser.
#ifndef _BOARDS_BTT_HBB_H
#define _BOARDS_BTT_HBB_H

pico_board_cmake_set(PICO_PLATFORM, rp2040)

#define BIGTREETECH_HBB 1

// K1 is the lower-right key in the manufacturer's front-view pin diagram.
// Do not define PICO_DEFAULT_LED_PIN: GPIO25 is a switch, not a GPIO LED.
#define GHOST_PAUSE_BUTTON_PIN 25
#define GHOST_BOOTSEL_PAUSE_ENABLED 0
#define GHOST_START_PAUSED 1

#define PICO_DEFAULT_WS2812_PIN 20
#define GHOST_WS2812_LED_COUNT 7

// The manufacturer's Klipper build instructions specify W25Q080, CLKDIV 2.
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
#define PICO_FLASH_SPI_CLKDIV 2
pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (1 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (1 * 1024 * 1024)
#endif

#endif
