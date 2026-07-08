cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)

project(PicoGhostHID C CXX ASM)

pico_sdk_init()

set(GHOST_USB_SERIAL "fedcba9876543210" CACHE STRING "USB serial number string exposed by the HID device")

add_executable(PicoGhostHID
    src/main.c
    src/usb_descriptors.c
)

target_compile_definitions(PicoGhostHID PRIVATE
    GHOST_USB_SERIAL="${GHOST_USB_SERIAL}"
)

target_include_directories(PicoGhostHID PRIVATE
    include
)

target_link_libraries(PicoGhostHID
    pico_stdlib
    tinyusb_device
    tinyusb_board
)

pico_enable_stdio_usb(PicoGhostHID 0)
pico_enable_stdio_uart(PicoGhostHID 0)

pico_add_extra_outputs(PicoGhostHID)
