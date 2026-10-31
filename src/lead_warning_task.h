#ifndef EEVBLOG_LEAD_WARNING_TASK_H
#define EEVBLOG_LEAD_WARNING_TASK_H

#include <stdbool.h>
#include <stdint.h>

void lead_warning_task_init(uint32_t now_ms);
void lead_warning_task_set_active(bool active, uint32_t now_ms);
void lead_warning_task_poll(uint32_t now_ms);

#endif
