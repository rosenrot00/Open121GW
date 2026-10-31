#ifndef EEVBLOG_FRONTEND_H
#define EEVBLOG_FRONTEND_H

#include <stdbool.h>
#include <stdint.h>

#define FRONTEND_STATUS_SIZE 52u

void frontend_init_pins(void);
void frontend_power_down(void);
bool frontend_ready(void);
void frontend_read_registers(uint8_t reg, uint8_t *dst, uint8_t len);
void frontend_write_registers(uint8_t reg, const uint8_t *src, uint8_t len);
void frontend_configure_diode(uint16_t timing_word);
void frontend_configure_p10_lowz(bool alt_path, uint16_t timing_word);
void frontend_configure_p10_voltage(uint8_t mode, uint8_t range, bool alt_bank,
                                    bool alt_primary_path, uint16_t timing_word);
void frontend_configure_p10_mode(uint8_t mode, uint8_t range, uint16_t timing_word,
                                 bool fast_ohms_search);
void frontend_configure_p10_current_dual_bank(uint8_t mode, uint8_t range, bool bank1,
                                              uint8_t alt_range0, uint8_t alt_range1,
                                              uint16_t timing_word);
void frontend_restart_p10_conversion(uint16_t timing_word);
void frontend_set_diode_voltage_15v(bool enabled);
bool frontend_cap_option_table_enabled(void);
uint8_t frontend_configured_reg20(void);
void frontend_restore_configured_reg20_shadow(void);
void frontend_read_status(uint8_t *dst52);
bool frontend_status_has_sample(const uint8_t *status52);
int32_t frontend_decode_signed24_sample(const uint8_t *status52);
uint32_t frontend_decode_u24_le_at(const uint8_t *status52, uint8_t offset);

#endif
