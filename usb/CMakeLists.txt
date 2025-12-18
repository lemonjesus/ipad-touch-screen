add_library(usb_hid INTERFACE)

target_sources(usb_hid INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/usb_hid.c
    ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
)

target_include_directories(usb_hid INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(usb_hid INTERFACE
    pico_stdlib
    multitouch
    tinyusb_device
    tinyusb_board
)
