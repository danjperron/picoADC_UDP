 add_library(pico_storage_drv INTERFACE)
target_sources(pico_storage_drv INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/glue.c
    ${CMAKE_CURRENT_LIST_DIR}/FatFs/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/FatFs/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/FatFs/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/sdmmc/spi_sdmmc.c
    ${CMAKE_CURRENT_LIST_DIR}/flash/W25Q.c
    ${CMAKE_CURRENT_LIST_DIR}/usb_msc/usb_msc.c
)

target_include_directories(pico_storage_drv INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/FatFs
    ${CMAKE_CURRENT_LIST_DIR}/flash
    ${CMAKE_CURRENT_LIST_DIR}/sdmmc
    ${CMAKE_CURRENT_LIST_DIR}/usb_msc
)

target_link_libraries(pico_storage_drv INTERFACE
        hardware_spi
        hardware_dma
        hardware_rtc
        pico_stdlib
        tinyusb_board
        tinyusb_host
)
