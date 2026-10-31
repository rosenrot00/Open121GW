ENTRY(Reset_Handler)

MEMORY
{
  /*
   * The stock app image is based at 0x08006000. 0x08000000..0x08005fff is
   * left to the factory boot/update code.
   */
  FLASH (rx)  : ORIGIN = 0x08006000, LENGTH = 0x5A000

  /*
   * The stock app uses 0x20001240 as initial MSP, but that is a stock layout
   * choice, not the device RAM limit. Keep our app in low SRAM and leave a
   * conservative stack margin while allowing debug/test state to grow.
   */
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x4000
}

SECTIONS
{
  .isr_vector ORIGIN(FLASH) :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text.Reset_Handler)
    *(.text*)
    *(.rodata*)
  } > FLASH

  /DISCARD/ :
  {
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.comment*)
  }

  .data :
  {
    __data_load = LOADADDR(.data);
    __data_start = .;
    *(.data*)
    __data_end = .;
  } > RAM AT > FLASH

  .ramfunc :
  {
    . = ALIGN(4);
    __ramfunc_load = LOADADDR(.ramfunc);
    __ramfunc_start = .;
    *(.ramfunc*)
    . = ALIGN(4);
    __ramfunc_end = .;
  } > RAM AT > FLASH

  .bss (NOLOAD) :
  {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    __bss_end = .;
  } > RAM

  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    __noinit_start = .;
    *(.noinit*)
    . = ALIGN(4);
    __noinit_end = .;
  } > RAM

  . = ALIGN(8);
  __stack_top = ORIGIN(RAM) + LENGTH(RAM);

  ASSERT(__stack_top - __noinit_end >= 0x800, "not enough runtime stack margin")
}
