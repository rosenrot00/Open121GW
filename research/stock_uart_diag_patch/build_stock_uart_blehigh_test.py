#!/usr/bin/env python3
import build_stock_uart_diag as diag


diag.OUT = diag.IDA_PRO / "EEVBlog2_05_stock_uart_blehigh_test.bin"
diag.WORD_HOOKS = {
    0x08006004: "hook_reset_direct_uart",
}
diag.HOOKS = {
    0x08024830: "hook_usart_read_data",
    0x08024D56: "hook_bt_process_rx_start_updater",
}


if __name__ == "__main__":
    raise SystemExit(diag.main())
