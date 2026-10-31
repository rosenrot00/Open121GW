#ifndef EEVBLOG_UART_UPDATE_H
#define EEVBLOG_UART_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

void uart_update_init(void);
bool uart_update_rx_byte(uint8_t byte);
bool uart_update_active(void);
bool uart_update_debug_allowed(void);
bool uart_update_bt_stream_enabled(void);
bool uart_update_bt_stream_allowed(void);
void uart_update_set_bt_stream_enabled(bool enabled);
bool uart_update_temperature_override_active(void);
int16_t uart_update_temperature_override_tenths_c(void);
bool uart_update_temperature_unit_override_active(void);
bool uart_update_temperature_unit_override_fahrenheit(void);
bool uart_update_battery_override_active(void);
uint8_t uart_update_battery_override_percent(void);
bool uart_update_battery_override_low(void);
bool uart_update_lcdtest_active(void);
uint16_t uart_update_lcdtest_index(void);
uint16_t uart_update_lcdtest_period_ms(void);
void uart_update_lcdtest_advance(void);
bool uart_update_p10_override_active(void);
uint8_t uart_update_p10_override_mode(void);
uint8_t uart_update_p10_override_range(void);
uint8_t uart_update_p10_override_cookie(void);
bool uart_update_hardfault_recover(uint32_t *stacked_regs);

#endif
