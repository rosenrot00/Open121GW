#ifndef EEVBLOG_BLUETOOTH_METER_TASK_H
#define EEVBLOG_BLUETOOTH_METER_TASK_H

#include <stdbool.h>
#include <stdint.h>

void bluetooth_meter_task_init(uint32_t now_ms);
void bluetooth_meter_task_set_active(bool active, uint32_t now_ms);
void bluetooth_meter_task_poll(uint32_t now_ms);
bool bluetooth_meter_task_take_frame_due(void);
void bluetooth_meter_task_frame_sent(uint32_t now_ms);

#endif
