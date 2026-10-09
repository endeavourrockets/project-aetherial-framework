# Project Aetherial Framework

Initial STM32G431 firmware structure.

- `firmware/`: CubeMX configuration, generated HAL/CMSIS sources, startup,
  linker script, and application code in `App/`.
- `include/`, `docs/`, `poc/`, `tests/`: placeholders for future work;
  they are not included in the main firmware build.
- `cmake/arm-none-eabi.cmake`: selects the Arm GCC toolchain.
- `CMakePresets.json`: shared firmware build settings.
- `CMakeUserPresets.json`: ignored machine-specific compiler/Ninja paths.

## Build

From the repository root on this machine:

```sh
cmake --preset stm32-windows
cmake --build --preset stm32-windows
```

Output flashable binary: `build/stm32-windows/firmware.elf`.

In VS Code with CMake Tools, select the `stm32-windows` configure preset,
then run `CMake: Configure` and `CMake: Build`.

On another machine, either put Arm GCC and Ninja on PATH and use the shared
`stm32-debug` preset, or create an ignored `CMakeUserPresets.json` with a
preset inheriting `stm32-debug`. Set `ARM_TOOLsCHAIN_BIN` to the compiler's
bin directory and `CMAKE_MAKE_PROGRAM` to Ninja's executable path.

The current application is a skeleton: it initializes the peripherals and
enters the main loop. Add application sources explicitly in
`firmware/CMakeLists.txt`; C11 and C++17 are enabled. The generated CubeMX
source list lives in `firmware/cmake/stm32cubemx/CMakeLists.txt`.

After CubeMX regeneration, review changes to the firmware wrapper and
source lists. The firmware project creates the `firmware` target expected by
this generated CubeMX configuration. PoCs remain separate firmware projects.

Build from the repository root, which owns the shared presets and toolchain.
The empty application currently sets up HAL and the 16 MHz HSI clock; no
GPIO or communication peripherals are configured. App sources and headers
are listed explicitly in firmware/CMakeLists.txt. Include directories alone
do not build library source files; add those when implementing a library.
