add_library(storage_driver INTERFACE)
target_sources(storage_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/sdmmc/spi_sdmmc.c
    ${CMAKE_CURRENT_LIST_DIR}/msc_device_disk.c
)

target_include_directories(storage_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/sdmmc
    ${CMAKE_CURRENT_LIST_DIR}/flash
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(storage_driver INTERFACE
        hardware_spi
        hardware_dma
        hardware_rtc
        pico_stdlib
)
