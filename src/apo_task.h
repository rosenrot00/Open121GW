#ifndef EEVBLOG_APO_TASK_H
#define EEVBLOG_APO_TASK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APO_TASK_EVENT_NONE = 0,
    APO_TASK_EVENT_WARNING_BEEP,
    APO_TASK_EVENT_TIMEOUT,
} apo_task_event_t;

void apo_task_init(uint32_t now_ms);
void apo_task_set_enabled(bool enabled, uint32_t now_ms);
void apo_task_set_suspended(bool suspended, uint32_t now_ms);
void apo_task_reset(bool minmax_active, uint32_t now_ms);
void apo_task_poll(uint32_t now_ms);
bool apo_task_take_event(apo_task_event_t *event);
uint16_t apo_task_seconds_remaining(void);

#endif
