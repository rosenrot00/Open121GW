#ifndef EEVBLOG_CALIBRATION_H
#define EEVBLOG_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#define CAL_EXPORT_SIZE 0x04B0u
#define CAL_MODE_COUNT 25u
#define CAL_RANGE_BLOCK_SIZE 16u

uint8_t calibration_read_u8(uint32_t offset);
uint16_t calibration_read_u16_le(uint32_t offset);
uint32_t calibration_read_u32_le(uint32_t offset);
uint16_t calibration_read_u16_be(uint32_t offset);
uint32_t calibration_read_u32_be(uint32_t offset);
uint16_t calibration_mode_base(uint8_t mode);
uint32_t calibration_range_offset(uint8_t mode, uint8_t range);
uint32_t calibration_stock_value_offset(uint8_t mode, uint8_t range, uint8_t field,
                                        bool measure_alt_path,
                                        bool voltage_alt_bank,
                                        bool voltage_alt_subpath,
                                        bool lowz_alt_path);
uint32_t calibration_stock_threshold_offset(uint8_t mode, uint8_t range,
                                            bool lowz_alt_path);
uint32_t calibration_checksum_export_area(void);
bool calibration_export_area_present(void);
bool calibration_write_u8(uint32_t offset, uint8_t value);
bool calibration_write_u16_be(uint32_t offset, uint16_t value);
bool calibration_write_u32_be(uint32_t offset, uint32_t value);

#endif
