#ifndef EEVBLOG_DUAL_SECONDARY_TASK_H
#define EEVBLOG_DUAL_SECONDARY_TASK_H

#include <stdbool.h>
#include <stdint.h>

void dual_secondary_task_init(uint32_t now_ms);
void dual_secondary_task_set_active(bool active, uint32_t now_ms);
void dual_secondary_task_poll(uint32_t now_ms);
bool dual_secondary_task_bank1(void);
bool dual_secondary_task_take_changed(void);

#endif
