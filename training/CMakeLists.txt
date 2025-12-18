add_library( training
    training.h
    training.c
)

pico_generate_pio_header(training ${CMAKE_CURRENT_LIST_DIR}/clocked_input.pio)

target_link_libraries(training pico_stdlib hardware_pio hardware_dma hardware_flash hardware_spi hardware_sync)

target_include_directories(training PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" ${PICO_INCLUDE_DIRS})
