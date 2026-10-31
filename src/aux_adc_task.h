#ifndef EEVBLOG_AUX_ADC_TASK_H
#define EEVBLOG_AUX_ADC_TASK_H

#include <stdbool.h>
#include <stdint.h>

#define AUX_ADC_TASK_PERIOD_MS 100u

typedef struct {
    bool pending;
    uint8_t last_channel;
    uint16_t last_raw;
    uint32_t poll_count;
    uint32_t sample_count;
    uint32_t start_count;
    uint32_t start_busy_count;
    uint32_t overwrite_count;
} aux_adc_task_diag_t;

void aux_adc_task_init(uint32_t now_ms);
void aux_adc_task_poll(uint32_t now_ms);
bool aux_adc_task_take_sample(uint8_t *channel, uint16_t *raw);
void aux_adc_task_get_diag(aux_adc_task_diag_t *diag);

#endif
