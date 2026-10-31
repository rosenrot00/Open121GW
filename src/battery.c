#include "battery.h"

#include "analog.h"
#include "uart_update.h"

static const uint16_t k_battery_adc_thresholds[] = {
    3120u, 2980u, 2840u, 2700u, 2560u, 2420u, 2280u, 2140u,
    2000u, 1860u, 1720u, 1580u, 1440u, 1300u, 1160u, 1020u,
};

static uint16_t g_battery_samples[20];
static uint8_t g_battery_sample_count;
static bool g_battery_low_latched;
static uint8_t g_battery_low_debounce;

static uint16_t battery_adc_to_centivolts(uint16_t raw) {
    if (raw < 1020u) {
        raw = 1020u;
    } else if (raw > 3120u) {
        raw = 3120u;
    }

    uint8_t index = 0;
    uint8_t last = (uint8_t)(sizeof(k_battery_adc_thresholds) / sizeof(k_battery_adc_thresholds[0]) - 1u);
    while (index < last && raw < k_battery_adc_thresholds[index + 1u]) {
        ++index;
    }

    uint16_t high = k_battery_adc_thresholds[index];
    uint16_t low = k_battery_adc_thresholds[index + 1u];
    uint16_t span = (uint16_t)(high - low);
    uint16_t offset = (uint16_t)(high - raw);
    uint16_t centivolts = (uint16_t)(1050u - (uint16_t)index * 50u);
    if (span != 0u) {
        centivolts = (uint16_t)(centivolts - (uint16_t)(((uint32_t)offset * 50u) / span));
    }
    return centivolts;
}

static uint16_t battery_filter_centivolts(uint16_t centivolts) {
    uint32_t sum = 0;

    if (g_battery_sample_count >= 20u) {
        uint16_t sorted[20];
        for (uint8_t i = 19u; i > 0u; --i) {
            g_battery_samples[i] = g_battery_samples[i - 1u];
        }
        g_battery_samples[0] = centivolts;

        for (uint8_t i = 0; i < 20u; ++i) {
            sorted[i] = g_battery_samples[i];
        }

        for (uint8_t i = 0; i < 20u; ++i) {
            uint8_t max_index = i;
            for (uint8_t j = (uint8_t)(i + 1u); j < 20u; ++j) {
                if (sorted[j] > sorted[max_index]) {
                    max_index = j;
                }
            }
            uint16_t tmp = sorted[i];
            sorted[i] = sorted[max_index];
            sorted[max_index] = tmp;
        }

        for (uint8_t i = 5u; i < 15u; ++i) {
            sum += sorted[i];
        }
        return (uint16_t)(sum / 10u);
    }

    g_battery_samples[g_battery_sample_count++] = centivolts;
    for (uint8_t i = 0; i < g_battery_sample_count; ++i) {
        sum += g_battery_samples[i];
    }
    return (uint16_t)(sum / g_battery_sample_count);
}

bool battery_read_centivolts(uint16_t *out_centivolts) {
    uint16_t raw;
    if (out_centivolts != 0 && uart_update_battery_override_active()) {
        *out_centivolts = (uint16_t)uart_update_battery_override_percent() * 10u;
        return true;
    }
    bool read_ok = out_centivolts != 0 && analog_read_adc_channel(5u, &raw);
    analog_power_down();
    if (!read_ok) {
        return false;
    }
    *out_centivolts = battery_adc_to_centivolts(raw);
    return true;
}

bool battery_sample_stock(uint16_t *out_decivolts, bool *out_low) {
    uint16_t raw;
    if (out_decivolts != 0 && out_low != 0 && uart_update_battery_override_active()) {
        *out_decivolts = uart_update_battery_override_percent();
        *out_low = uart_update_battery_override_low();
        return true;
    }
    bool read_ok = out_decivolts != 0 && out_low != 0 && analog_read_adc_channel(5u, &raw);
    analog_power_down();
    if (!read_ok) {
        return false;
    }
    return battery_sample_raw_stock(raw, out_decivolts, out_low);
}

bool battery_sample_raw_stock(uint16_t raw, uint16_t *out_decivolts, bool *out_low) {
    if (out_decivolts == 0 || out_low == 0) {
        return false;
    }
    if (uart_update_battery_override_active()) {
        *out_decivolts = uart_update_battery_override_percent();
        *out_low = uart_update_battery_override_low();
        return true;
    }
    uint16_t filtered_centivolts = battery_filter_centivolts(battery_adc_to_centivolts(raw));
    uint16_t decivolts = (uint16_t)(filtered_centivolts / 10u);
    bool low_candidate = decivolts < 41u;

    if (low_candidate == g_battery_low_latched) {
        g_battery_low_debounce = 0;
    } else if (++g_battery_low_debounce >= 11u) {
        g_battery_low_latched = low_candidate;
        g_battery_low_debounce = 0;
    }

    *out_decivolts = decivolts;
    *out_low = g_battery_low_latched;
    return true;
}
