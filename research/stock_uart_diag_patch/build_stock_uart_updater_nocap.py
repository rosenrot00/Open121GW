#!/usr/bin/env python3
import build_stock_uart_diag as diag


diag.OUT = diag.IDA_PRO / "EEVBlog2_05_stock_uart_updater_nocap.bin"
diag.HOOKS = {
    0x08024830: "hook_usart_read_data",
}


if __name__ == "__main__":
    raise SystemExit(diag.main())
