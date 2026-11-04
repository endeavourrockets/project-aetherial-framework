cmake_minimum_required(VERSION 3.22)

# OBC firmware for the G431.
set(CMAKE_PROJECT_NAME firmware)
add_executable(firmware)
add_subdirectory(cmake/stm32cubemx)
target_sources(firmware PRIVATE App/pressure.c)

# Add hand-written application sources here as they are split out of main.c.
target_include_directories(firmware PRIVATE
    App
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE App/rtos)
set(FREERTOS_PORT GCC_ARM_CM4F CACHE STRING "")
set(FREERTOS_HEAP 4 CACHE STRING "")
add_subdirectory(../../third_party/FreeRTOS-Kernel ${CMAKE_BINARY_DIR}/freertos)
target_link_libraries(firmware stm32cubemx freertos_kernel m)

# Keep the existing ELF path for the debugger and Renode.
set_target_properties(firmware PROPERTIES
    SUFFIX .elf
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(linker_script "${CMAKE_CURRENT_SOURCE_DIR}/STM32G431xx_FLASH.ld")
target_link_options(firmware PRIVATE
    "-T${linker_script}"
    --specs=nano.specs --specs=nosys.specs
    "-Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map"
    -Wl,--gc-sections -Wl,--print-memory-usage)
set_property(TARGET firmware APPEND PROPERTY LINK_DEPENDS "${linker_script}")
set_property(TARGET firmware APPEND PROPERTY ADDITIONAL_CLEAN_FILES
    "${CMAKE_BINARY_DIR}/firmware.map")
add_custom_command(TARGET firmware POST_BUILD
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:firmware>"
    VERBATIM)
