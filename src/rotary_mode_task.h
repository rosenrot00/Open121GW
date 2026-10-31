#ifndef EEVBLOG_ROTARY_MODE_TASK_H
#define EEVBLOG_ROTARY_MODE_TASK_H

#include <stdbool.h>
#include <stdint.h>

uint8_t rotary_mode_task_default(uint8_t rotary);
uint8_t rotary_mode_task_cycle(uint8_t mode);
bool rotary_mode_task_persist(uint8_t rotary, uint8_t mode);
bool rotary_mode_task_is_mode_for_rotary(uint8_t rotary, uint8_t mode);

#endif
