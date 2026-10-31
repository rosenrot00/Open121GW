#include "display_model.h"

display_model_output_t display_model_select(const display_model_input_t *input) {
    display_model_output_t output = {0, false, false};
    if (input == 0) {
        return output;
    }

    output.value = input->value;
    output.have_value = input->have_value;
    output.overrange = input->overrange;

    if (input->hold_active) {
        output.value = input->hold_value;
        output.have_value = input->hold_has_value;
        output.overrange = input->hold_overrange;
    } else if (input->peak_active &&
               (input->peak_has_value || input->peak_overrange)) {
        output.value = input->peak_value;
        output.have_value = input->peak_has_value;
        output.overrange = input->peak_overrange;
    } else if (input->minmax_state == 1u &&
               (input->minmax_count != 0u || input->minmax_max_overrange)) {
        output.value = input->minmax_max;
        output.have_value = true;
        output.overrange = input->minmax_max_overrange;
    } else if (input->minmax_state == 2u &&
               (input->minmax_count != 0u || input->minmax_min_overrange)) {
        output.value = input->minmax_min;
        output.have_value = true;
        output.overrange = input->minmax_min_overrange;
    } else if (input->minmax_state == 3u && input->minmax_count != 0u) {
        output.value = input->minmax_avg;
        output.have_value = true;
        output.overrange = false;
    } else if (input->relative_active && !input->relative_capture_pending &&
               input->have_value && !input->overrange) {
        output.value -= input->relative_baseline;
    }

    return output;
}
