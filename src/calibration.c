#include "calibration.h"

#include "hw.h"

#define FLASH_PECR_PELOCK BIT(0)
#define FLASH_SR_BSY      BIT(0)
#define FLASH_SR_EOP      BIT(1)
#define FLASH_SR_WRPERR   BIT(8)
#define FLASH_SR_PGAERR   BIT(9)
#define FLASH_SR_SIZERR   BIT(10)
#define FLASH_SR_OPTVERR  BIT(11)

#define STOCK_MODE_LOZ      0u
#define STOCK_MODE_V_DC     1u
#define STOCK_MODE_V_AC     2u
#define STOCK_MODE_MV_AC    4u
#define STOCK_MODE_MA_AC    16u
#define STOCK_MODE_A_AC     18u
#define STOCK_CAL_VOLTAGE_ALT_BLOCK 0x02D0u

static const uint16_t k_cal_mode_base_offsets[CAL_MODE_COUNT] = {
    0x0030u, 0x0070u, 0x0030u, 0x00B0u, 0x00D0u,
    0x00F0u, 0x0100u, 0x0100u, 0x0100u, 0x0110u,
    0x0120u, 0x0180u, 0x0190u, 0x0300u, 0x0340u,
    0x0380u, 0x01F0u, 0x0210u, 0x0230u, 0x0250u,
    0x0270u, 0x02A0u, 0x03C0u, 0x0400u, 0x0440u,
};

uint8_t calibration_read_u8(uint32_t offset) {
    return REG8(CAL_FLASH_BASE + offset);
}

uint16_t calibration_read_u16_le(uint32_t offset) {
    uint16_t lo = calibration_read_u8(offset);
    uint16_t hi = calibration_read_u8(offset + 1u);
    return (uint16_t)(lo | (hi << 8));
}

uint32_t calibration_read_u32_le(uint32_t offset) {
    uint32_t b0 = calibration_read_u8(offset);
    uint32_t b1 = calibration_read_u8(offset + 1u);
    uint32_t b2 = calibration_read_u8(offset + 2u);
    uint32_t b3 = calibration_read_u8(offset + 3u);
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

uint16_t calibration_read_u16_be(uint32_t offset) {
    uint16_t hi = calibration_read_u8(offset);
    uint16_t lo = calibration_read_u8(offset + 1u);
    return (uint16_t)((hi << 8) | lo);
}

uint32_t calibration_read_u32_be(uint32_t offset) {
    uint32_t b0 = calibration_read_u8(offset);
    uint32_t b1 = calibration_read_u8(offset + 1u);
    uint32_t b2 = calibration_read_u8(offset + 2u);
    uint32_t b3 = calibration_read_u8(offset + 3u);
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

uint16_t calibration_mode_base(uint8_t mode) {
    if (mode >= CAL_MODE_COUNT) {
        return 0;
    }
    return k_cal_mode_base_offsets[mode];
}

uint32_t calibration_range_offset(uint8_t mode, uint8_t range) {
    return (uint32_t)calibration_mode_base(mode) + (uint32_t)range * CAL_RANGE_BLOCK_SIZE;
}

uint32_t calibration_stock_value_offset(uint8_t mode, uint8_t range, uint8_t field,
                                        bool measure_alt_path,
                                        bool voltage_alt_bank,
                                        bool voltage_alt_subpath,
                                        bool lowz_alt_path) {
    if (field > 3u) {
        return 0xFFFFFFFFu;
    }

    if ((mode == STOCK_MODE_V_AC || mode == STOCK_MODE_MV_AC) &&
        measure_alt_path && field < 2u) {
        return STOCK_CAL_VOLTAGE_ALT_BLOCK + (uint32_t)field * 4u;
    }

    uint32_t base;
    if (mode == STOCK_MODE_LOZ) {
        uint8_t cal_mode = lowz_alt_path ? STOCK_MODE_V_AC : STOCK_MODE_V_DC;
        base = (uint32_t)calibration_mode_base(cal_mode) + 0x30u;
    } else if (mode == STOCK_MODE_V_AC && voltage_alt_bank && voltage_alt_subpath) {
        base = calibration_range_offset(STOCK_MODE_V_DC, range);
    } else {
        base = calibration_range_offset(mode, range);
    }

    return base + (uint32_t)field * 4u;
}

uint32_t calibration_stock_threshold_offset(uint8_t mode, uint8_t range,
                                            bool lowz_alt_path) {
    (void)lowz_alt_path;
    if (mode == STOCK_MODE_LOZ) {
        mode = STOCK_MODE_V_AC;
        range = 3u;
    } else if (mode == STOCK_MODE_MA_AC || mode == STOCK_MODE_A_AC) {
        range = 1u;
    }
    return (uint32_t)calibration_mode_base(mode) + 8u +
           (uint32_t)range * CAL_RANGE_BLOCK_SIZE;
}

uint32_t calibration_checksum_export_area(void) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < CAL_EXPORT_SIZE; ++i) {
        hash ^= calibration_read_u8(i);
        hash *= 16777619u;
    }
    return hash;
}

bool calibration_export_area_present(void) {
    uint32_t all_ff = 0;
    uint32_t all_00 = 0;
    for (uint32_t i = 0; i < CAL_EXPORT_SIZE; ++i) {
        uint8_t b = calibration_read_u8(i);
        all_ff += b == 0xFFu ? 1u : 0u;
        all_00 += b == 0x00u ? 1u : 0u;
    }
    return all_ff != CAL_EXPORT_SIZE && all_00 != CAL_EXPORT_SIZE;
}

static void calibration_eeprom_wait_ready(void) {
    while ((REG32(FLASH_SR) & FLASH_SR_BSY) != 0u) {
    }
}

static bool calibration_eeprom_unlock(void) {
    calibration_eeprom_wait_ready();
    if ((REG32(FLASH_PECR) & FLASH_PECR_PELOCK) == 0u) {
        return true;
    }

    REG32(FLASH_PEKEYR) = 0x89ABCDEFu;
    REG32(FLASH_PEKEYR) = 0x02030405u;
    calibration_eeprom_wait_ready();
    return (REG32(FLASH_PECR) & FLASH_PECR_PELOCK) == 0u;
}

bool calibration_write_u8(uint32_t offset, uint8_t value) {
    if (!calibration_eeprom_unlock()) {
        return false;
    }

    REG32(FLASH_SR) = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR |
                      FLASH_SR_SIZERR | FLASH_SR_OPTVERR;
    REG8(CAL_FLASH_BASE + offset) = value;
    calibration_eeprom_wait_ready();

    uint32_t errors = FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_OPTVERR;
    return (REG32(FLASH_SR) & errors) == 0u && calibration_read_u8(offset) == value;
}

bool calibration_write_u16_be(uint32_t offset, uint16_t value) {
    return calibration_write_u8(offset, (uint8_t)(value >> 8)) &&
           calibration_write_u8(offset + 1u, (uint8_t)value);
}

bool calibration_write_u32_be(uint32_t offset, uint32_t value) {
    return calibration_write_u8(offset, (uint8_t)(value >> 24)) &&
           calibration_write_u8(offset + 1u, (uint8_t)(value >> 16)) &&
           calibration_write_u8(offset + 2u, (uint8_t)(value >> 8)) &&
           calibration_write_u8(offset + 3u, (uint8_t)value);
}
