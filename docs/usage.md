# Setup and everyday commands

Run these from the repository root. You need Git, CMake 3.22 or newer, Ninja and Arm GNU GCC for firmware. PC tests also need a native C compiler; Arm GCC cannot build a test executable for your laptop. Install Renode if you want the emulator. The team's Software Setup Guide covers the STM32 tools and drivers.

## Get the code

```sh
git clone --recurse-submodules https://github.com/endeavourrockets/project-aetherial-framework.git
cd project-aetherial-framework
git switch dev
git submodule update --init --recursive
```

If you already cloned it, the last command gets the FreeRTOS revision selected by your current branch. Unity is included directly. These instructions describe the four-MCU/tests branch; until it is merged into `dev`, switch to that feature branch to use the new layout.

## Build the OBC

With Arm GCC and Ninja on your PATH:

```sh
cmake --preset stm32-debug
cmake --build --preset stm32-debug
```

The result is `build/stm32-debug/firmware.elf`. To rebuild from scratch without removing the configure settings:

```sh
cmake --build --preset stm32-debug --clean-first
```

The board names are `obc`, `eps`, `aerosol` and `fieldmill`. Only OBC has firmware yet; the others deliberately stop with a placeholder message. Give each board its own build directory when adding its project.

## Tool paths and preset names

The shared commands work on Windows, Linux and macOS with CMake, Ninja and Arm GCC on PATH. PC tests need a native compiler, such as GCC or Clang. VS Code uses the same CMake build underneath.

| Name | What it actually does |
| --- | --- |
| `stm32-debug` | Shared firmware configure/build preset. Currently selects OBC and writes `build/stm32-debug/firmware.elf`. |
| `host-tests` | Shared configure/build/test preset for native PC tests, separate from the Arm build. |

Put the directories containing `cmake`, `ninja` and `arm-none-eabi-gcc` on PATH, along with your native compiler. On Windows, add these directories under your user environment variables; on Linux/macOS, use your shell environment or package manager setup. Restart VS Code after changing PATH so its terminals and extensions inherit the change. Check with `cmake --version`, `ninja --version` and `arm-none-eabi-gcc --version`.

Use `cmake --list-presets` to see what is available on your machine. If tools are not on PATH, put a local preset in the ignored `CMakeUserPresets.json`, inheriting the relevant shared preset. Set `ARM_TOOLCHAIN_BIN` for firmware, `CMAKE_C_COMPILER` for native tests, and `CMAKE_MAKE_PROGRAM` for Ninja as needed. These paths belong to your machine, regardless of OS. Keep native and Arm builds in separate directories.

## Run the PC tests

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

Add `--verbose` to the CTest command to see individual Unity checks. The [test guide](../tests/README.md) explains how this applies to each board.

## VS Code and flashing

Open the repository root with CMake Tools and the STM32 extensions installed. Run **CMake: Select Configure Preset**, choose your firmware preset, then **CMake: Configure** and **CMake: Build**.

The shared `.vscode/settings.json` uses `cube-cmake` and ST's bundled clangd. If you use standalone CMake, override `cmake.cmakePath` and `cmake.configureArgs` in your local editor configuration. Keep personal installation paths out of the shared files. The CMake compile commands describe the actual build settings.

Connect the NUCLEO with a data-capable USB cable and working ST-LINK drivers. Select the ST-LINK launch configuration in **Run and Debug**. Check it points at the intended firmware ELF before launching; it programs the board and stops at `main()`. STM32CubeProgrammer can also program the ELF via ST-LINK: connect, choose the ELF under **Erasing & Programming**, then start programming.

## Run Renode

Build the firmware first, then:

```sh
renode renode/aetherial.resc
```

Or run the VS Code task **Renode: Run firmware** and enter `stm32-debug` for the build folder. The script loads the ELF and starts the minimal G431 platform. In the Renode Monitor, `pause`, `start` and `quit` control the session.

The current sim isn't finished. There are a bunch of unmapped-peripheral and interrupt-priority warnings need interpreting, not treating as a clean hardware test. It does not establish that sensors, CAN, radio or storage work. Each CAN POC has its own setup.

## CubeMX changes

Each board needs its own CubeMX project, generated sources and headers, and application code. The intended layout is:

```text
firmware/obc/obc.ioc
firmware/eps/eps.ioc
firmware/aerosol/aerosol.ioc
firmware/fieldmill/fieldmill.ioc
```

The refactor is not finished: today the working project is still `firmware/obc/firmware.ioc`, and the other board folders are placeholders. Renaming it to `obc.ioc` needs the CubeMX project settings and generated build checked together, not just a file rename.

For other boards, follow the same setup with the individual peripheral configuration for the given board. Keep generated `Core/`, `Drivers/` and board-specific startup/linker files inside that board's folder, and handwritten code in its `App/`. Share code deliberately through `shared/`, rather than sharing generated peripheral setup.

When generating, check the output directory and preserve `USER CODE` blocks. CubeMX owns the board's `cmake/stm32cubemx/CMakeLists.txt`; preserve our root and board-level build files. Review the diff, rebuild, and commit the `.ioc` with its generated changes. The old setup guide's regeneration check predates this refactor.

## Finishing the four-board workflow

The intended setup is one shared CMake preset and build directory per board, with a separate ELF for OBC, EPS, aerosol and fieldmill. VS Code should select a board for build/flash/debug and provide a build-all task that runs the four builds. Building everything should not flash everything.

This is still to implement once the board projects exist. The current ST-LINK launch entry asks the STM32 extension for its selected binary. It is not an explicit four-board selector yet. Check both the ELF and the connected ST-LINK target before flashing, particularly with several boards plugged in.

Likewise, the combined Renode scenario will need four machines, each loading its own board ELF, with the agreed bus connections between them. The current root script only loads the OBC. Four copies of the OBC image would not test the four-board software.

## Git stuff

Start with a clean working tree, or commit your current work first:

```sh
git status
git switch dev
git pull --ff-only origin dev
git submodule update --init --recursive
git switch -c feat/your-change
```

After editing and checking the change:

```sh
git diff
git add path/to/changed-file
git commit -m "feat: describe the change"
git push -u origin feat/your-change
```

Open a PR on GitHub with `dev` as the base. Use the [small PR template](CONTRIBUTING.md). If a pull cannot fast-forward, inspect the branch history together rather than force-pushing your way out of it.
