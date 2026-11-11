# Project Aetherial

Software for Endeavour's 3U rocket payload. We are building a prototype to collect aerosol and electric-field measurements, with an STM32G431 as our current MCU. There is quite a bit of scaffolding here. The spacecraft gremlins are still on the payroll.

## Start here

- [Setup and everyday commands](docs/usage.md)
- [Contributing and a small PR template](docs/CONTRIBUTING.md)
- [Project notes and references](docs/README.md)
- [Tests and how to add one](tests/README.md)

## What is here

The OBC has a FreeRTOS demo with synthetic readings and task queues. Storage, radio, command execution and hardware-watchdog actions are still placeholders. The PC tests cover the demo pressure check; Renode provides a minimal OBC/LED smoke check. This is a starting point, not finished flight software.

| Folder | Purpose |
| --- | --- |
| `firmware/obc/` | OBC CubeMX project and application code in `App/` |
| `firmware/eps/`, `firmware/aerosol/`, `firmware/fieldmill/` | Other MCU projects, currently placeholders |
| `shared/` | Space for common drivers and the board-to-board protocol |
| `tests/` | PC tests and space for integration tests |
| `renode/` | Emulator platform and run scripts |
| `poc/` | Separate experiments with their own builds and notes |
| `third_party/` | FreeRTOS submodule and vendored Unity |
| `cmake/`, `.vscode/` | Shared build and editor setup |

Build one board per build directory. Only `obc` builds so far. The firmware uses C11 and assembly; keep machine-specific tool paths in the ignored `CMakeUserPresets.json`.

The four-board refactor is ongoing. Each board will have its own `.ioc`, generated source/header files and ELF. See [the remaining build, VS Code and Renode work](docs/usage.md#finishing-the-four-board-workflow).
