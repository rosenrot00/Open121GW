#include <assert.h>
#include <stdint.h>

#include "dmm.h"
#include "measurement_profile.h"

static void test_range_counts(void) {
    assert(measurement_profile_manual_range_count(DMM_MODE_V_DC) == 4u);
    assert(measurement_profile_manual_range_count(DMM_MODE_P10_OHMS) == 7u);
    assert(measurement_profile_autorange_count(DMM_MODE_P10_OHMS) == 6u);
    assert(measurement_profile_manual_range_count(DMM_MODE_P10_CAP) == 6u);
    assert(measurement_profile_manual_range_count(DMM_MODE_MA_AC) == 2u);
    assert(measurement_profile_manual_range_count(DMM_MODE_A_DC) == 3u);
}

static void test_decimal_positions(void) {
    static const uint8_t ohms_expected[7] = {3u, 2u, 4u, 3u, 2u, 4u, 3u};
    static const uint8_t cap_expected[6] = {3u, 2u, 4u, 3u, 2u, 0u};
    for (uint8_t range = 0u; range < 7u; ++range) {
        assert(measurement_profile_decimal_position(DMM_MODE_P10_OHMS, range) ==
               ohms_expected[range]);
    }
    for (uint8_t range = 0u; range < 6u; ++range) {
        assert(measurement_profile_decimal_position(DMM_MODE_P10_CAP, range) ==
               cap_expected[range]);
    }
    assert(measurement_profile_decimal_position(DMM_MODE_V_DC, 0u) == 4u);
    assert(measurement_profile_decimal_position(DMM_MODE_V_DC, 3u) == 1u);
}

static void test_filter_and_cadence_profiles(void) {
    uint16_t deadband = 0u;
    uint8_t count = 0u;
    measurement_profile_smoothing(DMM_MODE_P10_OHMS, 5u, &deadband, &count);
    assert(deadband == 10u);
    assert(count == 5u);
    measurement_profile_smoothing(DMM_MODE_P10_DIODE, 0u, &deadband, &count);
    assert(deadband == 2u);
    assert(count == 2u);

    assert(measurement_profile_settle_count(DMM_MODE_P10_OHMS, 4u) == 1u);
    assert(measurement_profile_settle_count(DMM_MODE_P10_OHMS, 5u) == 2u);
    assert(measurement_profile_average_count(DMM_MODE_P10_OHMS, 4u) == 8u);
    assert(measurement_profile_average_count(DMM_MODE_P10_OHMS, 5u) == 1u);
    assert(measurement_profile_average_count(DMM_MODE_P10_CAP, 0u) == 1u);
    assert(measurement_profile_average_count(DMM_MODE_V_DC, 0u) == 16u);
}

int main(void) {
    test_range_counts();
    test_decimal_positions();
    test_filter_and_cadence_profiles();
    return 0;
}
