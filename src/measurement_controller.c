#include "measurement_controller.h"

void measurement_controller_init(measurement_controller_t *controller, uint8_t mode,
                                 uint8_t range) {
    controller->mode = mode;
    controller->range = range;
    controller->display_range = range;
    measurement_controller_invalidate_configuration(controller);
}

void measurement_controller_invalidate_configuration(measurement_controller_t *controller) {
    controller->configured_mode = MEASUREMENT_CONTROLLER_UNCONFIGURED;
    controller->configured_range = MEASUREMENT_CONTROLLER_UNCONFIGURED;
}

void measurement_controller_invalidate_range_configuration(
    measurement_controller_t *controller) {
    controller->configured_range = MEASUREMENT_CONTROLLER_UNCONFIGURED;
}

void measurement_controller_set_range(measurement_controller_t *controller, uint8_t range,
                                      bool update_display_range) {
    controller->range = range;
    if (update_display_range) {
        controller->display_range = range;
    }
}

void measurement_controller_set_display_range(measurement_controller_t *controller,
                                              uint8_t range) {
    controller->display_range = range;
}

bool measurement_controller_needs_configuration(const measurement_controller_t *controller) {
    return controller->configured_mode != controller->mode ||
           controller->configured_range != controller->range;
}

bool measurement_controller_is_same_mode_reconfigure(
    const measurement_controller_t *controller) {
    return controller->configured_mode == controller->mode &&
           controller->configured_range != controller->range;
}

void measurement_controller_mark_configured(measurement_controller_t *controller) {
    controller->configured_mode = controller->mode;
    controller->configured_range = controller->range;
}

void measurement_controller_accept_range_change(measurement_controller_t *controller,
                                                uint8_t range,
                                                bool deferred_reconfigure,
                                                bool update_display_range) {
    measurement_controller_set_range(controller, range, update_display_range);
    controller->configured_mode = controller->mode;
    controller->configured_range =
        deferred_reconfigure ? MEASUREMENT_CONTROLLER_UNCONFIGURED : range;
}

void measurement_controller_accept_display_sample(measurement_controller_t *controller,
                                                  uint8_t range) {
    controller->range = range;
    controller->display_range = range;
    controller->configured_range = range;
}
