#ifndef EEVBLOG_MEM_LOGGING_TASK_H
#define EEVBLOG_MEM_LOGGING_TASK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MEM_LOGGING_EVENT_NONE = 0,
    MEM_LOGGING_EVENT_SAMPLE_DUE,
} mem_logging_event_t;

void mem_logging_task_init(uint32_t now_ms);
void mem_logging_task_poll(uint32_t now_ms);
void mem_logging_task_start(const char name83[11], uint32_t now_ms);
void mem_logging_task_stop(uint32_t now_ms);
void mem_logging_task_clear_icon_gate(void);
void mem_logging_task_finish_sample(bool queued, uint16_t interval_s, uint32_t now_ms);
bool mem_logging_task_take_event(mem_logging_event_t *event);
bool mem_logging_task_active(void);
bool mem_logging_task_icon_gate(void);
uint32_t mem_logging_task_sample(void);
const char *mem_logging_task_name83(void);

#endif
