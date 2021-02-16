# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH /home/pi/pico/pico/pico-sdk)

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(dcf77 C CXX)

# Initialise the Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(dcf77 
        dcf77.cpp
        dcf77_decoder.cpp
        ssd1306.cpp
)

pico_set_program_name(dcf77 "dcf77")
pico_set_program_version(dcf77 "0.1")

# re-direct STDIO to USB
pico_enable_stdio_uart(dcf77 0)
pico_enable_stdio_usb(dcf77 1)

# Add the standard library to the build
target_link_libraries(dcf77 pico_stdlib pico_multicore)

# PIO programs
pico_generate_pio_header(dcf77 ${CMAKE_CURRENT_LIST_DIR}/dcf77.pio)

# Add any user reuqested libraries
target_link_libraries(dcf77
        hardware_pio
        hardware_pwm
        hardware_rtc
        hardware_i2c
        hardware_dma
        pico_runtime
        pico_printf
        )

pico_add_extra_outputs(dcf77)

