#ifndef EEVBLOG_BLUETOOTH_H
#define EEVBLOG_BLUETOOTH_H

#include <stdbool.h>
#include <stdint.h>

void bluetooth_init_19200(uint32_t pclk_hz);
void bluetooth_init_uart(uint32_t pclk_hz, uint32_t baud, bool enable_ble);
void bluetooth_power_down(void);
void bluetooth_set_enable(bool enable_ble);
void bluetooth_set_baud(uint32_t pclk_hz, uint32_t baud);
bool bluetooth_send_byte(uint8_t byte);
bool bluetooth_wait_tx_complete(uint32_t timeout);
bool bluetooth_send_bytes_irq(const uint8_t *data, uint8_t len);
bool bluetooth_tx_busy(void);
void bluetooth_send_frame(const uint8_t payload17[17]);
void bluetooth_send_heartbeat(uint32_t ticks, uint32_t cal_hash, bool cal_present);
void bluetooth_make_meter_frame(uint8_t out[19], uint32_t meter_id, uint8_t mode,
                                uint8_t range, bool overrange, int32_t main_value,
                                int16_t sub_value, bool sub_fahrenheit,
                                uint8_t unit_scale, uint8_t bargraph,
                                uint8_t icon0, uint8_t icon1, uint8_t icon2);
bool bluetooth_receive_byte(uint8_t *byte);
void bluetooth_poll_rx(void);
void bluetooth_irq_handler(void);

#endif
