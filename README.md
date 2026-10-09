# Project Aetherial Framework

Endeavour's dedicated payload team cubesat software repository, running on a NUCLEO-G431KB board. 
The build is designed to be shared; everyone can work from the same source and settings while using their preferred IDE on any OS. It has currently been verified on Windows/Linux.

## Setup

For a standard installation, download the STM32CubeIDE and all related tools/drivers/extensions listed in the **STM32 guide** on the Drive. This doesn't force you to use the IDE, it just stops you from making errors with versions.

For a manual installation, install:
- CMake >= 3.22
- Ninja
- An Arm GNU toolchain

Put the toolchain's bin directory and Ninja on your PATH, then run these
commands from the repository root:

```sh
cmake --preset stm32-debug
cmake --build --preset stm32-debug
```

The debug output is `build/stm32-debug/firmware.elf`. This is the file to flash onto the board. Build settings live in the various `CMakeLists.txt`, `CMakePresets.json` and `cmake/arm-none-eabi.cmake`. The `CMakeLists.txt` files in `root/` and `firmware/` shouldn't get regenerated. The one in `firmware/cmake/stm32cubemx/` will.

If you prefer to leave PATH alone, create a local `CMakeUserPresets.json` with a preset that inherits `stm32-debug`. Set `ARM_TOOLCHAIN_BIN` to your compiler's bin directory and `CMAKE_MAKE_PROGRAM` to your Ninja executable.

## IDEs and flashing

In VS Code with the CMake extension, select the shared `stm32-debug` configure preset (or your local variant) then configure and build. The STM32 extension can launch an ST-LINK debug session to program the board and run the firmware. When you run it, the debugger will wait at `main()` before starting execution.

Follow the **Software Setup Guide** on the Drive if anything breaks.

The STM32CubeProgrammer can also flash the .elf directly; this can come as an extension. You will need a working USB (not charging-only) and ST-LINK drivers (these usually come with the CubeIDE).

The STM32CubeIDE can work with CMake projects too. You simply need to import the CMake build config, setting `firmware/` as the project and the repo root as root.

## Current Suggested Structure

| Directory | Purpose |
| --- | --- |
| `firmware/Core/`, `firmware/Drivers/` | CubeMX initialization, interrupt handlers and ST libraries |
| `firmware/App/` | Software code owned by the team |
| `include/` | Headers for external libraries. In root in case `poc/` projects need them |
| `cmake/` | Shared compiler setup |
| `poc/` | Separate buildable experiments with some notes about what is tested and achieved |
| `docs/` | Design decisions, contributing guide, and hardware/build notes |
| `tests/` | Placeholder for code tests |

Add App sources explicitly in `firmware/CMakeLists.txt`.
C11 and C++17 are enabled. Code conventions and a more concrete high-level structure will be decided soon. Also, keep the main firmware and each PoC separate, please.

The firmware currently doensn't have any scheduling or RTOS. This will be discussed in the coming weeks. 

## CubeMX config changes

Open `firmware/firmware.ioc` in CubeMX. Make sure to load it as a project without starting a new one. Keep the project name `firmware` and the CMake/GCC generation settings. Make sure **Keep User Code when re-generating**, **Generate peripheral initialisation**, and **Delete previously generated files** are all toggled on before hitting **GENERATE CODE**. Any CubeMX config changes should be discussed and announced to the team before committing. 

Each time you regenerate, review the source lists and check that everything builds and flashes correctly. You'll be surprised how often issues have risen up in the past.

Commit the .ioc and the related generated changes together. Keep tool installation paths, build output and local workspace files out of Git (the .gitignore should already account for this).
