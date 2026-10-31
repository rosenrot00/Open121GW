#!/usr/bin/env python3
import build_stock_uart_diag as diag


diag.OUT = diag.IDA_PRO / "EEVBlog2_05_stock_uart_stocktx_blelow.bin"
diag.WORD_HOOKS = {}
diag.HOOKS = {
    0x0800C528: "hook_bt_init_stock_ble_low",
    0x0800C52E: "hook_bt_disable_keep_uart_ble_low",
    0x0801C608: "hook_bt_disable_keep_uart_ble_low",
    0x08024C0C: "hook_bt_build_frame_safe_diag",
}


if __name__ == "__main__":
    raise SystemExit(diag.main())
