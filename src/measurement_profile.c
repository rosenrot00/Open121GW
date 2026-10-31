#include "measurement_profile.h"

#include "dmm.h"

#define PROFILE_MAX_RANGES 7u
#define PROFILE_MODE_COUNT 26u

typedef struct {
    uint8_t manual_range_count;
    uint8_t autorange_count;
    uint8_t decimal_count;
    uint8_t settle_count;
    uint8_t average_count;
    uint8_t decimal[PROFILE_MAX_RANGES];
    uint16_t smoothing_deadband[PROFILE_MAX_RANGES];
    uint8_t smoothing_count[PROFILE_MAX_RANGES];
} measurement_profile_t;

#define PROFILE(ranges, decimals, settle, average, ...) \
    {ranges, ranges, decimals, settle, average, {__VA_ARGS__}, {0u}, {0u}}

static const measurement_profile_t k_profiles[PROFILE_MODE_COUNT] = {
    [DMM_MODE_LOZ] = PROFILE(1u, 1u, 3u, 16u, 1u),
    [DMM_MODE_V_DC] = PROFILE(4u, 4u, 2u, 16u, 4u, 3u, 2u, 1u),
    [DMM_MODE_V_AC] = PROFILE(4u, 4u, 3u, 16u, 4u, 3u, 2u, 1u),
    [DMM_MODE_MV_DC] = PROFILE(2u, 2u, 0u, 16u, 3u, 2u),
    [DMM_MODE_MV_AC] = PROFILE(2u, 2u, 0u, 16u, 3u, 2u),
    [DMM_MODE_TEMP] = PROFILE(1u, 1u, 0u, 16u, 1u),
    [DMM_MODE_FREQ] = PROFILE(5u, 5u, 0u, 1u, 3u, 2u, 4u, 3u, 2u),
    [DMM_MODE_PERIOD] = PROFILE(3u, 3u, 0u, 1u, 4u, 3u, 2u),
    [DMM_MODE_DUTY] = PROFILE(1u, 1u, 0u, 1u, 1u),
    [DMM_MODE_P10_OHMS] = {
        7u, 6u, 7u, 1u, 8u,
        {3u, 2u, 4u, 3u, 2u, 4u, 3u},
        {100u, 20u, 10u, 20u, 20u, 10u, 100u},
        {10u, 10u, 10u, 10u, 10u, 5u, 20u},
    },
    [DMM_MODE_P10_CONT] = {
        1u, 1u, 1u, 2u, 16u,
        {2u},
        {100u},
        {10u},
    },
    [DMM_MODE_P10_DIODE] = {
        1u, 1u, 1u, 0u, 16u,
        {4u},
        {2u},
        {2u},
    },
    [DMM_MODE_P10_CAP] = PROFILE(6u, 6u, 0u, 1u, 3u, 2u, 4u, 3u, 2u, 0u),
    [DMM_MODE_MA_AUX] = PROFILE(4u, 4u, 3u, 16u, 2u, 1u, 1u, 0u),
    [DMM_MODE_LEGACY_CURRENT_AC] = PROFILE(4u, 4u, 3u, 16u, 3u, 2u, 2u, 1u),
    [DMM_MODE_MVA_AC] = PROFILE(4u, 4u, 3u, 16u, 1u, 0u, 3u, 2u),
    [DMM_MODE_MA_AC] = PROFILE(2u, 7u, 3u, 16u, 3u, 2u, 4u, 3u, 2u, 4u, 3u),
    [DMM_MODE_MA_DC] = PROFILE(2u, 7u, 3u, 16u, 3u, 2u, 4u, 3u, 2u, 4u, 3u),
    [DMM_MODE_A_AC] = PROFILE(2u, 5u, 3u, 16u, 4u, 3u, 2u, 1u, 2u),
    [DMM_MODE_A_DC] = PROFILE(3u, 5u, 3u, 16u, 4u, 3u, 2u, 1u, 2u),
    [DMM_MODE_A_DC_PERSIST] = PROFILE(3u, 3u, 3u, 16u, 2u, 4u, 3u),
    [DMM_MODE_A_AC_PERSIST] = PROFILE(3u, 3u, 3u, 16u, 2u, 4u, 3u),
    [DMM_MODE_MA_BURDEN] = PROFILE(4u, 4u, 3u, 16u, 2u, 1u, 1u, 0u),
    [DMM_MODE_MA_ALT_AC] = PROFILE(4u, 4u, 3u, 16u, 3u, 2u, 2u, 1u),
    [DMM_MODE_MVA_DC] = PROFILE(4u, 4u, 3u, 16u, 1u, 0u, 3u, 2u),
    [DMM_MODE_V_ACDC] = PROFILE(1u, 1u, 0u, 16u, 0u),
};

#undef PROFILE

static const measurement_profile_t k_default_profile = {
    1u, 1u, 1u, 0u, 16u,
    {0u},
    {0u},
    {0u},
};

static const measurement_profile_t *measurement_profile_get(uint8_t mode) {
    if (mode >= PROFILE_MODE_COUNT || k_profiles[mode].manual_range_count == 0u) {
        return &k_default_profile;
    }
    return &k_profiles[mode];
}

static uint8_t measurement_profile_clamp_range(const measurement_profile_t *profile,
                                               uint8_t range) {
    uint8_t count = profile->manual_range_count;
    if (count > PROFILE_MAX_RANGES) {
        count = PROFILE_MAX_RANGES;
    }
    return range < count ? range : (uint8_t)(count - 1u);
}

uint8_t measurement_profile_manual_range_count(uint8_t mode) {
    return measurement_profile_get(mode)->manual_range_count;
}

uint8_t measurement_profile_autorange_count(uint8_t mode) {
    return measurement_profile_get(mode)->autorange_count;
}

uint8_t measurement_profile_decimal_position(uint8_t mode, uint8_t range) {
    const measurement_profile_t *profile = measurement_profile_get(mode);
    uint8_t count = profile->decimal_count;
    if (count > PROFILE_MAX_RANGES) {
        count = PROFILE_MAX_RANGES;
    }
    uint8_t index = range < count ? range : (uint8_t)(count - 1u);
    return profile->decimal[index];
}

uint8_t measurement_profile_settle_count(uint8_t mode, uint8_t range) {
    const measurement_profile_t *profile = measurement_profile_get(mode);
    if (mode == DMM_MODE_P10_OHMS && range >= 5u) {
        return 2u;
    }
    return profile->settle_count;
}

uint8_t measurement_profile_average_count(uint8_t mode, uint8_t range) {
    const measurement_profile_t *profile = measurement_profile_get(mode);
    if (mode == DMM_MODE_P10_OHMS && range == 5u) {
        return 1u;
    }
    return profile->average_count;
}

void measurement_profile_smoothing(uint8_t mode, uint8_t range,
                                   uint16_t *deadband, uint8_t *sample_count) {
    const measurement_profile_t *profile = measurement_profile_get(mode);
    uint8_t index = measurement_profile_clamp_range(profile, range);
    if (deadband != 0) {
        *deadband = profile->smoothing_deadband[index];
    }
    if (sample_count != 0) {
        *sample_count = profile->smoothing_count[index];
    }
}
