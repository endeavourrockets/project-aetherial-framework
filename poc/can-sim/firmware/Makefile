# Makefile --> builds the CAN proof of concept (bare metal, no HAL), see README.md
#
#   make build every program into build/*.elf
#   make test build, then run every Renode CAN demo and check the output
#   make clean
#   make CROSS=/path/to/arm-none-eabi-   use another toolchain (default: the one on PATH)
#
# programs:
#   f4_dual STM32F405: CAN1 sends FLIGHT_PHASE, CAN2 receives it (step 2)
#   obc STM32F405 fake OBC: FLIGHT_PHASE + TIME_SYNC on bus A and bus B
#   experiment STM32F405 Experiment board: listens on both buses, drops duplicates
#   g431_loopback STM32G431: FDCAN1 sends to itself (internal loopback)
#   g431_ext_loopback  same, external loopback --> frames also come out on PA12 (FDCAN1_TX)

CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
SIZE    := $(CROSS)size

BUILD   := build
ROOT    := ..

# cortex-m4 with soft float --> these programs only use integers
ARCH := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
CFLAGS := $(ARCH) -std=c11 -O2 -g3 -ffreestanding -fno-builtin \
           -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections \
           -Wall -Wextra -Wshadow -Icommon -MMD -MP
LDFLAGS := $(ARCH) -nostdlib -Wl,--build-id=none -Wl,--gc-sections

F4_COMMON := common/startup_cm4.c common/board_f4.c common/bxcan.c common/print.c
G4_COMMON := common/startup_cm4.c common/board_g431.c common/fdcan_poc.c common/print.c
F4_LD := common/stm32f405.ld
# copy of the payload's G431 linker script (firmware/Payload/platform/renode/),
# kept here so this folder builds on its own
G4_LD := common/stm32g431kb.ld

PROGS := f4_dual obc experiment g431_loopback g431_ext_loopback

all: $(addprefix $(BUILD)/,$(addsuffix .elf,$(PROGS)))
	@$(SIZE) $^

# objects go in build/obj/<same path as the .c file>.o
objs = $(patsubst %.c,$(BUILD)/obj/%.o,$(1)) $(BUILD)/obj/libc_min.o

$(BUILD)/f4_dual.elf: $(call objs,f4_dual/main.c $(F4_COMMON)) $(F4_LD)
$(BUILD)/obc.elf: $(call objs,obc/main.c $(F4_COMMON)) $(F4_LD)
$(BUILD)/experiment.elf: $(call objs,experiment/main.c $(F4_COMMON)) $(F4_LD)
$(BUILD)/g431_loopback.elf: $(call objs,g431_loopback/main.c $(G4_COMMON)) $(G4_LD)
# same main.c compiled a second time with external loopback on
$(BUILD)/g431_ext_loopback.elf: $(BUILD)/obj/g431_ext_loopback/main.o \
                                $(call objs,$(G4_COMMON)) $(G4_LD)

# link --> the linker script is whichever .ld is in the prerequisites
$(BUILD)/%.elf:
	$(CC) $(LDFLAGS) -T $(filter %.ld,$^) -Wl,-Map=$(BUILD)/$*.map $(filter %.o,$^) -o $@

$(BUILD)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/obj/g431_ext_loopback/main.o: g431_loopback/main.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DLOOPBACK_EXTERNAL=1 -c $< -o $@

# memcpy/memset the compiler may call by itself, copy of the payload's libc_min.c
$(BUILD)/obj/libc_min.o: common/libc_min.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: all
	$(ROOT)/renode/run_can_tests.sh

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
