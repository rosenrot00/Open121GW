#include <assert.h>

#include "display_model.h"

static display_model_input_t base_input(void) {
    display_model_input_t input = {0};
    input.value = 100;
    input.have_value = true;
    return input;
}

static void test_base_and_relative(void) {
    display_model_input_t input = base_input();
    display_model_output_t output = display_model_select(&input);
    assert(output.value == 100);
    assert(output.have_value);
    assert(!output.overrange);

    input.relative_active = true;
    input.relative_baseline = 40;
    output = display_model_select(&input);
    assert(output.value == 60);

    input.relative_capture_pending = true;
    output = display_model_select(&input);
    assert(output.value == 100);
}

static void test_hold_has_highest_priority(void) {
    display_model_input_t input = base_input();
    input.hold_active = true;
    input.hold_value = 20;
    input.hold_has_value = true;
    input.peak_active = true;
    input.peak_has_value = true;
    input.peak_value = 30;
    input.minmax_state = 1u;
    input.minmax_count = 1u;
    input.minmax_max = 40;
    input.relative_active = true;
    input.relative_baseline = 10;

    display_model_output_t output = display_model_select(&input);
    assert(output.value == 20);
    assert(output.have_value);
}

static void test_peak_and_minmax_states(void) {
    display_model_input_t input = base_input();
    input.peak_active = true;
    input.peak_overrange = true;
    display_model_output_t output = display_model_select(&input);
    assert(!output.have_value);
    assert(output.overrange);

    input.peak_active = false;
    input.minmax_state = 1u;
    input.minmax_count = 2u;
    input.minmax_max = 300;
    output = display_model_select(&input);
    assert(output.value == 300);

    input.minmax_state = 2u;
    input.minmax_min = -20;
    output = display_model_select(&input);
    assert(output.value == -20);

    input.minmax_state = 3u;
    input.minmax_avg = 90;
    output = display_model_select(&input);
    assert(output.value == 90);
}

int main(void) {
    test_base_and_relative();
    test_hold_has_highest_priority();
    test_peak_and_minmax_states();
    return 0;
}
