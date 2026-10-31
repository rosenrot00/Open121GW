#ifndef EEVBLOG_BATTERY_H
#define EEVBLOG_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

bool battery_read_centivolts(uint16_t *out_centivolts);
bool battery_sample_stock(uint16_t *out_decivolts, bool *out_low);
bool battery_sample_raw_stock(uint16_t raw, uint16_t *out_decivolts, bool *out_low);

#endif
