#include "temperature.h"

#include "analog.h"
#include "calibration.h"
#include "uart_update.h"

#define TEMP_CAL_OFFSET_ADDR 0x02E0u
#define TEMP_OPEN_TENTHS 10000
#define TEMP_OPEN_CENTIDEG 32767

static const uint16_t k_temp_adc_thresholds[] = {
    0x0EAEu, 0x0E58u, 0x0DF2u, 0x0D7Bu, 0x0CF3u, 0x0C5Bu,
    0x0BB5u, 0x0B03u, 0x0A47u, 0x0985u, 0x08C2u, 0x0800u,
    0x0743u, 0x068Eu, 0x05E4u, 0x0545u, 0x04B3u, 0x042Eu,
    0x03B6u, 0x034Au, 0x02EAu, 0x0295u, 0x0249u, 0x0206u,
};

static uint16_t g_temp_raw_samples[10];
static uint8_t g_temp_raw_count;

static int16_t temperature_cal_offset_hundredths(void) {
    int16_t offset = (int16_t)calibration_read_u16_be(TEMP_CAL_OFFSET_ADDR);
    if (offset > 1000 || offset < -1000) {
        offset = 0;
    }
    return offset;
}

static uint16_t temperature_filter_raw(uint16_t raw) {
    uint32_t sum = 0;

    if (g_temp_raw_count >= 10u) {
        for (uint8_t i = 9u; i > 0u; --i) {
            g_temp_raw_samples[i] = g_temp_raw_samples[i - 1u];
        }
        g_temp_raw_samples[0] = raw;
        for (uint8_t i = 0; i < 10u; ++i) {
            sum += g_temp_raw_samples[i];
        }
        return (uint16_t)(sum / 10u);
    }

    g_temp_raw_samples[g_temp_raw_count++] = raw;
    for (uint8_t i = 0; i < g_temp_raw_count; ++i) {
        sum += g_temp_raw_samples[i];
    }
    return (uint16_t)(sum / g_temp_raw_count);
}

static bool ntc_adc_to_c_hundredths(uint16_t raw, int16_t *out_hundredths) {
    if (raw < 0x0206u || raw >= 0x0E59u) {
        return false;
    }

    uint8_t index = 0;
    uint8_t last = (uint8_t)(sizeof(k_temp_adc_thresholds) / sizeof(k_temp_adc_thresholds[0]) - 1u);
    while (index < last && raw < k_temp_adc_thresholds[index + 1u]) {
        ++index;
    }

    uint16_t high = k_temp_adc_thresholds[index];
    uint16_t low = k_temp_adc_thresholds[index + 1u];
    uint16_t span = (uint16_t)(high - low);
    uint16_t offset = (uint16_t)(high - raw);
    int32_t value = -3000 + (int32_t)index * 500;
    if (span != 0u) {
        value += ((int32_t)offset * 500) / span;
    }
    value += temperature_cal_offset_hundredths();
    *out_hundredths = (int16_t)value;
    return true;
}

bool temperature_update_from_raw_centideg(uint16_t raw, int16_t *out_centideg_c) {
    int16_t hundredths;
    if (out_centideg_c == 0 || raw < 0x0033u) {
        if (out_centideg_c != 0) {
            *out_centideg_c = TEMP_OPEN_CENTIDEG;
        }
        return false;
    }

    if (uart_update_temperature_override_active()) {
        *out_centideg_c = (int16_t)(uart_update_temperature_override_tenths_c() * 10);
        return true;
    }

    raw = temperature_filter_raw(raw);
    if (!ntc_adc_to_c_hundredths(raw, &hundredths)) {
        *out_centideg_c = TEMP_OPEN_CENTIDEG;
        return false;
    }

    *out_centideg_c = hundredths;
    return true;
}

bool temperature_update_from_raw(uint16_t raw, int16_t *out_tenths_c) {
    int16_t centideg;
    if (out_tenths_c == 0) {
        return false;
    }
    if (!temperature_update_from_raw_centideg(raw, &centideg)) {
        *out_tenths_c = TEMP_OPEN_TENTHS;
        return false;
    }

    int16_t tenths = (int16_t)(centideg / 10);
    if ((tenths % 10) >= 5) {
        ++tenths;
    }
    *out_tenths_c = tenths;
    return true;
}

bool temperature_read_c_centideg(int16_t *out_centideg_c) {
    uint16_t raw;
    if (out_centideg_c != 0 && uart_update_temperature_override_active()) {
        *out_centideg_c = (int16_t)(uart_update_temperature_override_tenths_c() * 10);
        return true;
    }
    bool read_ok = out_centideg_c != 0 && analog_read_adc_channel(4u, &raw);
    analog_power_down();
    if (!read_ok) {
        if (out_centideg_c != 0) {
            *out_centideg_c = TEMP_OPEN_CENTIDEG;
        }
        return false;
    }
    return temperature_update_from_raw_centideg(raw, out_centideg_c);
}

bool temperature_read_c_tenths(int16_t *out_tenths_c) {
    uint16_t raw;
    if (out_tenths_c != 0 && uart_update_temperature_override_active()) {
        *out_tenths_c = uart_update_temperature_override_tenths_c();
        return true;
    }
    bool read_ok = out_tenths_c != 0 && analog_read_adc_channel(4u, &raw);
    analog_power_down();
    if (!read_ok) {
        if (out_tenths_c != 0) {
            *out_tenths_c = TEMP_OPEN_TENTHS;
        }
        return false;
    }
    return temperature_update_from_raw(raw, out_tenths_c);
}
