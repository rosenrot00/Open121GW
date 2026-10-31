#include "aux_adc_task.h"

#include "analog.h"

#define AUX_ADC_TEMP_CHANNEL 4u
#define AUX_ADC_BATTERY_CHANNEL 5u

static bool g_aux_adc_pending;
static bool g_aux_adc_next_battery;
static uint8_t g_aux_adc_channel;
static uint16_t g_aux_adc_raw;
static aux_adc_task_diag_t g_aux_adc_diag;

void aux_adc_task_init(uint32_t now_ms) {
    (void)now_ms;
    g_aux_adc_pending = false;
    g_aux_adc_next_battery = false;
    g_aux_adc_channel = 0;
    g_aux_adc_raw = 0;
    g_aux_adc_diag.pending = false;
    g_aux_adc_diag.last_channel = 0;
    g_aux_adc_diag.last_raw = 0;
    g_aux_adc_diag.poll_count = 0;
    g_aux_adc_diag.sample_count = 0;
    g_aux_adc_diag.start_count = 0;
    g_aux_adc_diag.start_busy_count = 0;
    g_aux_adc_diag.overwrite_count = 0;
}

void aux_adc_task_poll(uint32_t now_ms) {
    (void)now_ms;
    ++g_aux_adc_diag.poll_count;

    uint8_t channel;
    uint16_t raw;
    if (analog_take_irq_sample(&channel, &raw)) {
        if (g_aux_adc_pending) {
            ++g_aux_adc_diag.overwrite_count;
        }
        g_aux_adc_channel = channel;
        g_aux_adc_raw = raw;
        g_aux_adc_pending = true;
        g_aux_adc_diag.pending = true;
        g_aux_adc_diag.last_channel = channel;
        g_aux_adc_diag.last_raw = raw;
        ++g_aux_adc_diag.sample_count;
    }

    uint8_t next_channel = g_aux_adc_next_battery ? AUX_ADC_BATTERY_CHANNEL : AUX_ADC_TEMP_CHANNEL;
    if (analog_start_adc_channel_irq(next_channel)) {
        g_aux_adc_next_battery = !g_aux_adc_next_battery;
        ++g_aux_adc_diag.start_count;
    } else {
        ++g_aux_adc_diag.start_busy_count;
    }
}

bool aux_adc_task_take_sample(uint8_t *channel, uint16_t *raw) {
    if (!g_aux_adc_pending || channel == 0 || raw == 0) {
        return false;
    }

    *channel = g_aux_adc_channel;
    *raw = g_aux_adc_raw;
    g_aux_adc_pending = false;
    g_aux_adc_diag.pending = false;
    return true;
}

void aux_adc_task_get_diag(aux_adc_task_diag_t *diag) {
    if (diag == 0) {
        return;
    }
    *diag = g_aux_adc_diag;
    diag->pending = g_aux_adc_pending;
}
