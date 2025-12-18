cmake_minimum_required(VERSION 3.13)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT on)

# pico_sdk_import.cmake is a single file copied from this SDK
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(ipad-screen-firmware)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(multitouch)
add_subdirectory(training)
add_subdirectory(power)
add_subdirectory(usb)

# rest of your project
add_executable(main
    main.c
    pins.c
)

target_link_libraries(main pico_stdlib pico_multicore hardware_spi multitouch training power usb_hid)

target_include_directories(training INTERFACE ${PICO_INCLUDE_DIRS})
target_include_directories(multitouch INTERFACE ${PICO_INCLUDE_DIRS})
target_include_directories(power INTERFACE ${PICO_INCLUDE_DIRS})

pico_enable_stdio_usb(main 0)
pico_enable_stdio_uart(main 1)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(main)
