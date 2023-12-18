set(PROJECT picoADC_UDP)
cmake_minimum_required(VERSION 3.12)
set(PICO_BOARD pico_w)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
add_executable(${PROJECT} ${PROJECT}.c)


project(${PROJECT} C CXX)
pico_sdk_init()


target_sources(${PROJECT} PRIVATE
        ${PROJECT}.c
        )

target_compile_definitions(picoADC_UDP PRIVATE
            WIFI_SSID=\"${WIFI_SSID}\"
            WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
            )


target_include_directories(${PROJECT} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${PROJECT}
     pico_cyw43_arch_lwip_threadsafe_background
     pico_stdlib
     pico_multicore
     hardware_adc
     hardware_dma
)

pico_add_extra_outputs(${PROJECT})
pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 0)


