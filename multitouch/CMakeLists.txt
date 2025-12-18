add_library( multitouch
    multitouch.h
    multitouch.c
)

target_link_libraries(multitouch pico_stdlib)

target_include_directories(multitouch PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" ${PICO_INCLUDE_DIRS})
