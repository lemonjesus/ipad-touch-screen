add_library( power
    power.h
    power.c
)

target_link_libraries(power pico_stdlib hardware_i2c hardware_gpio)

target_include_directories(power PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" ${PICO_INCLUDE_DIRS})
