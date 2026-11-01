/*
 * stm32f405.ld --> linker script for the CAN PoC on the STM32F405RGT6
 * tells the linker where flash and RAM are and in what order to put things.
 * the F405RG has 1 MiB flash at 0x08000000 and 112 KiB + 16 KiB of SRAM
 * back to back at 0x20000000 (RM0090 section 2.3)
 */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* stack starts at the top of RAM */

SECTIONS
{
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH
  .text : { . = ALIGN(4); *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH

  _sidata = LOADADDR(.data);
  .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  .bss (NOLOAD) : { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM

  /DISCARD/ : { *(.ARM.attributes) *(.comment) *(.note.gnu.build-id) }
}
