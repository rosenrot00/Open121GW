#!/usr/bin/env python3
import build_stock_uart_diag as diag


diag.OUT = diag.IDA_PRO / "EEVBlog2_05_stock_uart_direct_start.bin"
diag.WORD_HOOKS = {
    0x08006004: "hook_reset_direct_uart",
}
diag.HOOKS = {
    0x0800C528: "hook_bt_disable_direct_uart",
    0x0800C52E: "hook_bt_disable_direct_uart",
    0x0801C608: "hook_bt_disable_direct_uart",
}


if __name__ == "__main__":
    raise SystemExit(diag.main())
