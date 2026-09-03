cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)
set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

include(pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)

project (paul_uq_clock C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(paul_uq_clock)

target_sources(paul_uq_clock PRIVATE
    main.c
    clock.cpp
    wifi.cpp
    ntp.cpp
    aux_led.cpp
    cJSON.c
    cpu_load.cpp)

target_include_directories(paul_uq_clock PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    /home/david/Pico_Projects/LED_Driver)

target_link_libraries(paul_uq_clock
    pico_stdlib
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_mqtt
    hardware_pio
    hardware_dma
    hardware_rtc
    FreeRTOS-Kernel-Heap4
    /home/david/Pico_Projects/LED_Driver/build/libaddressable_led.a)

# enable usb output, disable uart output
    pico_enable_stdio_usb(paul_uq_clock 1)
# enable serial output, disable uart output (UART0 TX, pin 1)
    pico_enable_stdio_uart(paul_uq_clock 0)

pico_add_extra_outputs(paul_uq_clock)
