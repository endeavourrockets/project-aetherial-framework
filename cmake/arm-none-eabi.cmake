set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Set this per machine in CMakeUserPresets.json, or put Arm GCC on PATH.
set(ARM_TOOLCHAIN_BIN "$ENV{ARM_TOOLCHAIN_BIN}" CACHE PATH "Arm GCC bin directory")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_TOOLCHAIN_BIN)
find_program(CMAKE_C_COMPILER arm-none-eabi-gcc HINTS "${ARM_TOOLCHAIN_BIN}" REQUIRED)
find_program(CMAKE_CXX_COMPILER arm-none-eabi-g++ HINTS "${ARM_TOOLCHAIN_BIN}" REQUIRED)
set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}")
find_program(CMAKE_SIZE arm-none-eabi-size HINTS "${ARM_TOOLCHAIN_BIN}" REQUIRED)

# Apply the same Cortex-M4 ABI to application, HAL, and startup sources.
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT "${MCU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${MCU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_FLAGS}")
