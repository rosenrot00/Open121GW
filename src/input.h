#ifndef EEVBLOG_INPUT_H
#define EEVBLOG_INPUT_H

#include <stdbool.h>
#include <stdint.h>

void input_init(void);
uint8_t input_rotary_sample_active_low(void);
uint8_t input_jack_sample_active_low(void);
uint8_t input_button_sample_active_low(void);
void input_debug_override_set(bool enabled, uint8_t rotary, uint8_t jack, uint8_t buttons);
void input_debug_override_clear(void);
void input_debug_override_set_jack(bool enabled, uint8_t jack);
void input_debug_override_set_buttons(bool enabled, uint8_t buttons);
bool input_debug_override_enabled(void);
bool input_debug_override_rotary_enabled(void);
bool input_debug_override_jack_enabled(void);
bool input_debug_override_buttons_enabled(void);
uint8_t input_debug_override_rotary(void);
uint8_t input_debug_override_jack(void);
uint8_t input_debug_override_buttons(void);
uint8_t input_debug_physical_rotary(void);
uint8_t input_debug_physical_jack(void);

#endif
