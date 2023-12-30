set(PROJECT picoADC_SDCARD)
cmake_minimum_required(VERSION 3.12)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico_w)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
add_executable(${PROJECT} ${PROJECT}.c)


project(${PROJECT} C CXX ASM)
pico_sdk_init()

add_subdirectory(../FatFs_SPI build)

target_sources(${PROJECT} PRIVATE
        ${PROJECT}.c
        fifoBlock.c
        pico_ntp.c
        ds3231.c
        hw_config.c
        )

target_compile_definitions(picoADC_SDCARD PRIVATE
            WIFI_SSID=\"labtest\"
            WIFI_PASSWORD=\"ltjix68233\"
            )


target_include_directories(${PROJECT} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(${PROJECT}
     pico_cyw43_arch_lwip_threadsafe_background
     pico_stdlib
     pico_multicore
     hardware_i2c
     hardware_rtc
     hardware_adc
     hardware_dma
     FatFs_SPI
)

pico_add_extra_outputs(${PROJECT})
pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 0)


