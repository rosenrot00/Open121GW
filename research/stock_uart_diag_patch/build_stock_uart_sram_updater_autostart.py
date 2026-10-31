#!/usr/bin/env python3
import build_stock_uart_diag as diag


diag.OUT = diag.IDA_PRO / "EEVBlog2_05_stock_uart_sram_updater_autostart.bin"
diag.WORD_HOOKS = {}
diag.HOOKS = {
    0x0800C528: "hook_bt_init_stock_ble_low",
    0x0800C52E: "hook_bt_disable_keep_uart_ble_low",
    0x0801C608: "hook_bt_disable_keep_uart_ble_low",
    0x08024830: "hook_usart_read_data",
    0x08024B4E: "hook_board_init_uart_ready_ble_low",
    0x08024D56: "hook_bt_process_rx_start_updater",
}


if __name__ == "__main__":
    raise SystemExit(diag.main())
