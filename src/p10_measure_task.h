#ifndef EEVBLOG_P10_MEASURE_TASK_H
#define EEVBLOG_P10_MEASURE_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "dmm.h"

typedef struct {
    bool active;
    bool pending;
    uint32_t poll_count;
    uint32_t event_count;
    uint32_t sample_count;
    uint32_t range_change_count;
    uint32_t overwrite_count;
    uint8_t last_mode;
    uint8_t last_range;
} p10_measure_task_diag_t;

void p10_measure_task_init(void);
void p10_measure_task_set_active(bool active);
bool p10_measure_task_active(void);
void p10_measure_task_flush(void);
void p10_measure_task_poll(uint32_t now_ms);
bool p10_measure_task_take_reading(dmm_reading_t *reading);
void p10_measure_task_get_diag(p10_measure_task_diag_t *diag);

#endif
