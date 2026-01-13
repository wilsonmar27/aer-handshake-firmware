# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico_aer_rx C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add common aer library
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../common common_build)

# Add executable. Default name is the project name, version 0.1
add_executable(pico_aer_rx
    pico_aer_rx.c
    aer_rx_poll.c
    usb_stream.c
    aer_event_sink.c
    hal/hal_gpio.c
    hal/hal_stdio.c
    hal/hal_time.c
)

pico_set_program_name(pico_aer_rx "pico_aer_rx")
pico_set_program_version(pico_aer_rx "0.1")

# Generate PIO header (ADD LATER)
# pico_generate_pio_header(pico_aer_rx ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico_aer_rx 0)
pico_enable_stdio_usb(pico_aer_rx 1)

# Add the standard library to the build
target_link_libraries(pico_aer_rx
        pico_stdlib
)

# Add the standard include files to the build
target_include_directories(pico_aer_rx PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/hal
)

# Add any user requested libraries
target_link_libraries(pico_aer_rx 
        aer_common
        hardware_dma
        hardware_pio
        hardware_timer
        )

pico_add_extra_outputs(pico_aer_rx)

