/*
 * stm32g431kb.ld --> linker script for the G431 PoC programs
 * copy of the payload's firmware/Payload/platform/renode/stm32g431kb.ld
 * (only the comments changed) so this folder builds on its own
 * STM32G431KB --> 128 KiB flash at 0x08000000, SRAM1 + SRAM2 + CCM alias give
 * 32 KiB of RAM in one block at 0x20000000
 */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 128K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 32K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_min_stack = 0x1000;

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text*)
    *(.rodata*)
    *(.glue_7) *(.glue_7t)
    . = ALIGN(4);
  } > FLASH

  .ARM.exidx :
  {
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
  } > FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /* survives resets, Reset_Handler doesn't clear it (the payload's persist.c,
   * the PoC doesn't use it) */
  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    *(.noinit*)
    . = ALIGN(4);
  } > RAM

  ._stack_check (NOLOAD) :
  {
    . = ALIGN(8);
    . = . + _min_stack;
  } > RAM

  /DISCARD/ : { *(.ARM.attributes) *(.comment) *(.note.gnu.build-id) }
}
