cmake_minimum_required(VERSION 3.13)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(BT_Shutter C CXX ASM)
set(PICO_BOARD pico_w)
pico_sdk_init() 


add_executable(BT_Shutter main.c)

# Compiles shutter.gatt into shutter.h automatically
pico_btstack_make_gatt_header(BT_Shutter PRIVATE "${CMAKE_CURRENT_LIST_DIR}/shutter.gatt")

target_include_directories(BT_Shutter PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(BT_Shutter
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_ble
    pico_btstack_cyw43
)
pico_enable_stdio_usb(BT_Shutter  0)
pico_enable_stdio_uart(BT_Shutter 1)
target_compile_definitions(BT_Shutter PUBLIC PICO_DEFAULT_UART=0)

pico_add_extra_outputs(BT_Shutter)

   
