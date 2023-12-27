set(PROJECT picoADC_USB)
cmake_minimum_required(VERSION 3.12)
set(PICO_BOARD pico)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
add_executable(${PROJECT} ${PROJECT}.c)


project(${PROJECT} C CXX)
pico_sdk_init()


target_sources(${PROJECT} PRIVATE
        ${PROJECT}.c
        fifoBlock.c
        )

target_compile_definitions(picoADC_USB PRIVATE
            )

target_include_directories(${PROJECT} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${PROJECT}
     pico_stdlib
     pico_multicore
     hardware_adc
     hardware_dma
)

pico_add_extra_outputs(${PROJECT})
pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 0)


