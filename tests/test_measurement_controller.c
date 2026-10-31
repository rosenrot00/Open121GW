#include "measurement_controller.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

int main(void) {
    measurement_controller_t controller;
    measurement_controller_init(&controller, 9u, 4u);
    assert(controller.mode == 9u);
    assert(controller.range == 4u);
    assert(controller.display_range == 4u);
    assert(measurement_controller_needs_configuration(&controller));

    measurement_controller_mark_configured(&controller);
    assert(!measurement_controller_needs_configuration(&controller));

    measurement_controller_invalidate_range_configuration(&controller);
    assert(measurement_controller_needs_configuration(&controller));
    measurement_controller_mark_configured(&controller);

    measurement_controller_set_range(&controller, 3u, true);
    assert(controller.display_range == 3u);
    assert(measurement_controller_is_same_mode_reconfigure(&controller));

    measurement_controller_mark_configured(&controller);
    measurement_controller_accept_range_change(&controller, 2u, true, true);
    assert(controller.range == 2u);
    assert(controller.display_range == 2u);
    assert(controller.configured_mode == controller.mode);
    assert(controller.configured_range == MEASUREMENT_CONTROLLER_UNCONFIGURED);
    assert(measurement_controller_needs_configuration(&controller));

    measurement_controller_mark_configured(&controller);
    measurement_controller_accept_range_change(&controller, 5u, false, false);
    assert(controller.range == 5u);
    assert(controller.display_range == 2u);
    assert(!measurement_controller_needs_configuration(&controller));

    measurement_controller_set_display_range(&controller, 4u);
    assert(controller.display_range == 4u);

    measurement_controller_accept_display_sample(&controller, 5u);
    assert(controller.range == 5u);
    assert(controller.display_range == 5u);
    assert(controller.configured_range == 5u);
    return 0;
}
