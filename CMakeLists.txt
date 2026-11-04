cmake_minimum_required(VERSION 3.22)
project(aetherial LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(AETHERIAL_BUILD_TESTS "Build PC unit tests instead of firmware" OFF)
if(AETHERIAL_BUILD_TESTS)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Unit tests need a PC compiler, not the Arm toolchain.")
    endif()
    enable_testing()
    add_subdirectory(tests/host)
    return()
endif()

if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Select an Arm toolchain preset (stm32-debug, or your local preset).")
endif()

enable_language(ASM)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(--specs=nano.specs)
set(AETHERIAL_BOARD "obc" CACHE STRING "Board firmware to build")
set_property(CACHE AETHERIAL_BOARD PROPERTY STRINGS obc eps aerosol fieldmill)
set(board_names obc eps aerosol fieldmill)
if(NOT AETHERIAL_BOARD IN_LIST board_names)
    message(FATAL_ERROR "Unknown board: ${AETHERIAL_BOARD}")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/${AETHERIAL_BOARD}/CMakeLists.txt")
    message(FATAL_ERROR "${AETHERIAL_BOARD} is a placeholder; its firmware is not set up yet.")
endif()

# Build one board at a time so CubeMX target names stay local to that build.
add_subdirectory(firmware/${AETHERIAL_BOARD})
