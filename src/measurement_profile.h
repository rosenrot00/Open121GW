#ifndef EEVBLOG_MEASUREMENT_PROFILE_H
#define EEVBLOG_MEASUREMENT_PROFILE_H

#include <stdint.h>

uint8_t measurement_profile_manual_range_count(uint8_t mode);
uint8_t measurement_profile_autorange_count(uint8_t mode);
uint8_t measurement_profile_decimal_position(uint8_t mode, uint8_t range);
uint8_t measurement_profile_settle_count(uint8_t mode, uint8_t range);
uint8_t measurement_profile_average_count(uint8_t mode, uint8_t range);
void measurement_profile_smoothing(uint8_t mode, uint8_t range,
                                   uint16_t *deadband, uint8_t *sample_count);

#endif
