#ifndef EEVBLOG_TEMPERATURE_H
#define EEVBLOG_TEMPERATURE_H

#include <stdbool.h>
#include <stdint.h>

bool temperature_read_c_tenths(int16_t *out_tenths_c);
bool temperature_update_from_raw(uint16_t raw, int16_t *out_tenths_c);
bool temperature_read_c_centideg(int16_t *out_centideg_c);
bool temperature_update_from_raw_centideg(uint16_t raw, int16_t *out_centideg_c);

#endif
