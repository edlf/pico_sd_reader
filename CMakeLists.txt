# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# Generated Cmake Pico project file
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

set(PICO_USE_FASTEST_SUPPORTED_CLOCK 1)
set(PICO_COPY_TO_RAM 1)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(pico_usb_msc_device C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(pico_usb_msc_device main.c usb_descriptors.c)

pico_set_program_name(pico_usb_msc_device "pico_usb_msc_device")
pico_set_program_version(pico_usb_msc_device "1.0")

pico_enable_stdio_uart(pico_usb_msc_device 1)
pico_enable_stdio_usb(pico_usb_msc_device 0)

# Add the standard library to the build
target_link_libraries(pico_usb_msc_device pico_stdlib)

# Add the standard include files to the build
target_include_directories(pico_usb_msc_device PRIVATE ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(storage_driver)

target_link_libraries(pico_usb_msc_device tinyusb_device tinyusb_board storage_driver)

pico_add_extra_outputs(pico_usb_msc_device)
