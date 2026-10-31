#ifndef EEVBLOG_DISPLAY_TASK_H
#define EEVBLOG_DISPLAY_TASK_H

#include <stdbool.h>
#include <stdint.h>

void display_task_init(uint32_t now_ms);
bool display_task_update_due(uint32_t now_ms);
void display_task_request_now(void);
void display_task_schedule_after(uint32_t now_ms, uint32_t delay_ms);
bool display_task_bargraph_update_due(uint32_t now_ms);
void display_task_schedule_bargraph_after(uint32_t now_ms, uint32_t delay_ms);

#endif
