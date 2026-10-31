#ifndef EEVBLOG_DISPLAY_MODEL_H
#define EEVBLOG_DISPLAY_MODEL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t value;
    bool have_value;
    bool overrange;

    bool hold_active;
    int32_t hold_value;
    bool hold_has_value;
    bool hold_overrange;

    bool peak_active;
    int32_t peak_value;
    bool peak_has_value;
    bool peak_overrange;

    uint8_t minmax_state;
    int32_t minmax_max;
    int32_t minmax_min;
    int32_t minmax_avg;
    uint32_t minmax_count;
    bool minmax_max_overrange;
    bool minmax_min_overrange;

    bool relative_active;
    bool relative_capture_pending;
    int32_t relative_baseline;
} display_model_input_t;

typedef struct {
    int32_t value;
    bool have_value;
    bool overrange;
} display_model_output_t;

display_model_output_t display_model_select(const display_model_input_t *input);

#endif
