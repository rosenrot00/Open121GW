#ifndef EEVBLOG_TASK_RUNTIME_H
#define EEVBLOG_TASK_RUNTIME_H

#include <stdint.h>

void task_runtime_init(uint32_t now_ms);
void task_runtime_poll(uint32_t now_ms);
uint32_t task_runtime_heartbeat_count(void);

#endif
