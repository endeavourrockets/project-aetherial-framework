cmake_minimum_required(VERSION 3.22)
project(aetherial LANGUAGES C CXX ASM)

if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Select an Arm toolchain preset (stm32-debug, or your local preset).")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(--specs=nano.specs)
add_subdirectory(firmware)
