#ifndef EEVBLOG_MEASUREMENT_CONTROLLER_H
#define EEVBLOG_MEASUREMENT_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#define MEASUREMENT_CONTROLLER_UNCONFIGURED 0xFFu

typedef struct {
    uint8_t mode;
    uint8_t range;
    uint8_t display_range;
    uint8_t configured_mode;
    uint8_t configured_range;
} measurement_controller_t;

void measurement_controller_init(measurement_controller_t *controller, uint8_t mode,
                                 uint8_t range);
void measurement_controller_invalidate_configuration(measurement_controller_t *controller);
void measurement_controller_invalidate_range_configuration(
    measurement_controller_t *controller);
void measurement_controller_set_range(measurement_controller_t *controller, uint8_t range,
                                      bool update_display_range);
void measurement_controller_set_display_range(measurement_controller_t *controller,
                                              uint8_t range);
bool measurement_controller_needs_configuration(const measurement_controller_t *controller);
bool measurement_controller_is_same_mode_reconfigure(
    const measurement_controller_t *controller);
void measurement_controller_mark_configured(measurement_controller_t *controller);
void measurement_controller_accept_range_change(measurement_controller_t *controller,
                                                uint8_t range,
                                                bool deferred_reconfigure,
                                                bool update_display_range);
void measurement_controller_accept_display_sample(measurement_controller_t *controller,
                                                  uint8_t range);

#endif
