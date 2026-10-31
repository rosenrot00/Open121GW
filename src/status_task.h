#ifndef EEVBLOG_STATUS_TASK_H
#define EEVBLOG_STATUS_TASK_H

#include <stdbool.h>
#include <stdint.h>

void status_task_init(void);
void status_task_poll(uint32_t now_ms);
void status_task_show_text(const char text[5], uint32_t now_ms, uint32_t duration_ms);
bool status_task_active(void);
bool status_task_take_expired(void);
const char *status_task_text(void);

#endif
