#ifndef EEVBLOG_BUZZER_H
#define EEVBLOG_BUZZER_H

#include <stdbool.h>
#include <stdint.h>

void buzzer_init(void);
void buzzer_set_enabled(bool enabled);
bool buzzer_enabled(void);
bool buzzer_active(void);
void buzzer_on_ms(uint32_t ms);
void buzzer_force_on_ms(uint32_t ms);
void buzzer_beep_blocking_ms(uint32_t ms);
void buzzer_off(void);
void buzzer_poll(void);

#endif
