# My thingy

small programs that try CAN between the planned boards in Renode, before any
PCB exists. clocks are 16 MHz HSI like the team firmware.ioc, run everything
from `poc/can-sim`

| Program | Chip | What eet does :=0 |
|---|---|---|
| `f4_dual` | STM32F405 | CAN1 sends FLIGHT_PHASE (0x100) at 10 Hz, CAN2 receives and prints it |
| `obc` | STM32F405 | fake OBC: FLIGHT_PHASE 10 Hz + TIME_SYNC 1 Hz on bus A (CAN1) and bus B (CAN2) |
| `experiment` | STM32F405 | listens on both buses, uses the first copy of each message, ignores the second |
| `g431_loopback` | STM32G431 | FDCAN1 internal loopback --> first bench test for the NUCLEO-G431KB |
| `g431_ext_loopback` | STM32G431 | same program in external loopback --> the frames also come out on PA12 for a logic analyser |

```bash
make            # build/*.elf (needs arm-none-eabi-gcc on PATH, or CROSS=/path/to/arm-none-eabi-)
make test       # runs every Renode demo headless and checks the output (needs renode on PATH, or RENODE=...)
```

code layout: `common/` has the drivers (`bxcan.c` for the F405, `fdcan_poc.c`
for the G431), board start up, console printing, the linker scripts and the
shared message definitions (`can_ids.h`). each program is one `main.c` in its
own folder. everything is register level, with comments pointing at the
reference manual (RM0090 for the F405, RM0440 for the G431) and at the
matching HAL function. not run on hardware yet
