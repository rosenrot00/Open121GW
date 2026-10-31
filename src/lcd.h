#ifndef EEVBLOG_LCD_H
#define EEVBLOG_LCD_H

#include <stdint.h>
#include <stdbool.h>

void lcd_init_blank(uint8_t contrast_level);
void lcd_set_contrast(uint8_t contrast_level);
void lcd_blank(void);
void lcd_power_down(void);
void lcd_show_stock_unused_segments(void);
void lcd_show_startup_version(void);
void lcd_show_raw_segment_test(uint16_t index);
void lcd_clear_shadow(void);
void lcd_commit(void);
void lcd_draw_char_at_position(uint8_t ch, uint8_t pos);
void lcd_write_5chars(bool upper, const char text[5]);
void lcd_set_secondary_decimal(uint8_t pos);
void lcd_clear_main_numeric_marks(void);
void lcd_write_lower_blank_with_decimal(uint8_t decimal_pos);
void lcd_write_lower_numeric(int32_t value, uint8_t decimal_pos);
void lcd_write_lower_temperature_c_tenths(int32_t value, uint8_t decimal_pos);
void lcd_write_lower_diode_open(uint8_t decimal_pos);
void lcd_write_lower_ohms_open(uint8_t decimal_pos);
void lcd_write_lower_continuity_open(uint8_t decimal_pos);
void lcd_write_lower_overflow(uint8_t decimal_pos);
void lcd_write_upper_signed_numeric(int32_t value, uint8_t decimal_pos);
void lcd_write_upper_b_scaled(int32_t value, uint8_t scale_mode, bool overrange);
void lcd_set_secondary_sign(bool negative);
void lcd_write_upper_temperature_c_tenths(int16_t value);
void lcd_write_upper_temperature_tenths(int16_t value, bool fahrenheit);
void lcd_write_upper_battery_decivolts(uint16_t decivolts);
void lcd_write_upper_battery_percent(uint8_t percent);
void lcd_set_p10_mode_annunciators(uint8_t mode, uint8_t range);
void lcd_set_temperature_unit_annunciator(bool temp_mode, bool fahrenheit);
void lcd_set_auto_annunciator(bool enabled);
void lcd_set_apo_annunciator(bool enabled);
void lcd_set_low_battery_annunciator(bool enabled);
void lcd_set_threshold_annunciator(bool enabled);
void lcd_set_rel_hold_annunciator(uint8_t state);
void lcd_set_rel_annunciator(bool enabled);
void lcd_set_relative_delta_annunciator(bool enabled);
void lcd_set_minmax_annunciator(uint8_t state);
void lcd_set_mem_annunciator(bool enabled);
void lcd_set_bt_annunciator(bool enabled);
void lcd_set_secondary_voltage_annunciator(bool enabled);
void lcd_set_secondary_frequency_annunciator(uint8_t range);
void lcd_set_dbm_annunciator(bool enabled);
void lcd_set_bargraph(int32_t value, bool overrange, uint8_t mode);
void lcd_set_bargraph_limited(int32_t value, bool overrange, uint8_t mode,
                              uint8_t range, uint8_t max_count);
void lcd_clear_bargraph(void);
uint32_t lcd_debug_shadow_word(uint8_t index);

#endif
