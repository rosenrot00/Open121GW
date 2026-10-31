ENTRY(hook_cap_compute)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x080294E8, LENGTH = 0x36B18
  RAM   (rwx) : ORIGIN = 0x20002000, LENGTH = 0x2000
}

SECTIONS
{
  .text ORIGIN(FLASH) :
  {
    KEEP(*(.text.hook_reset_direct_uart))
    KEEP(*(.text.hook_reset_direct_uart_pf15_high))
    KEEP(*(.text.hook_bt_disable_direct_uart))
    KEEP(*(.text.hook_bt_init_stock_ble_low))
    KEEP(*(.text.hook_bt_disable_keep_uart_ble_low))
    KEEP(*(.text.hook_bt_init_stock_pf15_high))
    KEEP(*(.text.hook_bt_disable_stock_pf15_low))
    KEEP(*(.text.hook_bt_disable_stock_pf15_keep_high))
    KEEP(*(.text.hook_board_init_uart_ready_ble_low))
    KEEP(*(.text.hook_bt_build_frame_safe_diag))
    KEEP(*(.text.hook_bt_process_rx_start_updater))
    KEEP(*(.text.hook_cap_compute))
    KEEP(*(.text.hook_cap_timeout))
    KEEP(*(.text.hook_cap_timeout_uart_only))
    KEEP(*(.text.hook_measurement_dispatch_uart_hello))
    KEEP(*(.text.hook_usart_read_data))
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH

  .ramfunc :
  {
    . = ALIGN(4);
    __ramfunc_load = LOADADDR(.ramfunc);
    __ramfunc_start = .;
    *(.ramfunc*)
    . = ALIGN(4);
    __ramfunc_end = .;
  } > RAM AT > FLASH

  /DISCARD/ :
  {
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.comment*)
  }
}
