#include "dmm.h"

#include <stdint.h>

#include "buzzer.h"
#include "calibration.h"
#include "clock.h"
#include "frontend.h"
#include "measurement_profile.h"
#include "rotary_mode_task.h"

#ifndef EEV_ENABLE_FAST_DIODE
#define EEV_ENABLE_FAST_DIODE 0
#endif

#ifndef EEV_TASK_FIRMWARE
#define EEV_TASK_FIRMWARE 0
#endif

#ifndef EEV_TASK_FAST_RANGE
#define EEV_TASK_FAST_RANGE EEV_TASK_FIRMWARE
#endif

#define P10_FAST_TOGGLE_TIMING_WORD 0x8000u
#define P10_STOCK_TIMING_WORD 0xE000u
#define P10_FAST_SEARCH_TIMING_WORD 0xF000u
#define DIODE_BEEP_MS 20u
#define DIODE_BEEP_MIN (-999)
#define DIODE_BEEP_MAX 1000
#define DIODE_OPEN_MAX_3V 30000u
#define DIODE_OPEN_MAX_15V 15000u
#define P10_DIODE_BEEP_MS 100u
#define P10_CONTINUITY_BEEP_MS 100u
#define P10_FAST_PREBEEP_MS 20u
#define P10_DISPLAY_AVG_SAMPLES 8u
#define CAL_GAIN_ONE 1000000u
#define CAL_GAIN_CAP 100000u
/*
 * Stock TIM2 uses PSC=260 and ARR=160 while SYSCLK/APB1 stay on MSI range 6
 * (4.194304 MHz), so the scheduler tick is about 10.018 ms.
 */
#define STOCK_TIM2_TICK_US 10017u
#define P10_FRONTEND_TIMING_TOGGLE_MS (((uint32_t)101u * STOCK_TIM2_TICK_US + 999u) / 1000u)
#define P10_FRONTEND_TIMING_LOZ_TOGGLE_MS (((uint32_t)2001u * STOCK_TIM2_TICK_US + 999u) / 1000u)
#define P10_FREQ_SECONDARY_REARM_MS 1000u
#define P10_OHMS_STOCK_GUARD_TICKS 100u
#define P10_OHMS_STOCK_SCHED_TICK_US STOCK_TIM2_TICK_US
#define P10_OHMS_STOCK_GUARD_MS \
    ((P10_OHMS_STOCK_GUARD_TICKS * P10_OHMS_STOCK_SCHED_TICK_US + 999u) / 1000u)
#define P10_RANGE_CONTROLLER_CONFIRM_SAMPLES 2u
#define P10_RANGE_CONTROLLER_TARGET_CONFIRM_SAMPLES 2u
#define P10_OHMS_OBVIOUS_DOWN_DIV 16u
#define P10_OHMS_PRECISION_SETTLE_EVENTS 1u

typedef enum {
    P10_OHMS_GUARD_STOCK = 0,
    P10_OHMS_GUARD_LEGACY = 1,
} p10_ohms_guard_policy_t;

static uint8_t g_frontend_status[FRONTEND_STATUS_SIZE];
static uint8_t g_p10_mode;
static uint8_t g_p10_range;
static int32_t g_p10_zero_offset;
static uint32_t g_p10_gain;
static uint32_t g_p10_gain_scale;
static int32_t g_p10_extra_comp;
static int32_t g_cal_threshold0_signed;
static int32_t g_cal_threshold1_signed;
static int32_t g_cal_threshold2_signed;
static int32_t g_cal_threshold3_signed;
static int32_t g_current_dual_zero[2];
static uint32_t g_current_dual_gain[2];
static int64_t g_p10_display_accum;
static uint8_t g_p10_display_count;
static int32_t g_p10_filter_samples[20];
static int32_t g_p10_filtered_value;
static uint8_t g_p10_filter_count;
static bool g_p10_autorange = true;
static bool g_p10_overrange_latched;
static bool g_p10_continuity_beeping;
static uint8_t g_p10_continuity_option;
static bool g_p10_diode_option_15;
static bool g_p10_diode_voltage_gpio_active;
static uint8_t g_p10_settle_samples;
static int8_t g_p10_autorange_direction;
static bool g_p10_cap_timeout_active;
static bool g_p10_cap_timeout_enabled = true;
static uint32_t g_p10_cap_timeout_at;
static uint32_t g_p10_cap_frontend_settle_until;
static bool g_p10_cap_force_low_range_after_timeout;
static bool g_p10_cap_frontend_sync_pending;
static uint8_t g_p10_autorange_holdoff;
static p10_ohms_guard_policy_t g_p10_ohms_guard_policy = P10_OHMS_GUARD_STOCK;
static bool g_p10_task_fast_range_enabled = EEV_TASK_FAST_RANGE != 0;
static int8_t g_p10_range_controller_candidate_dir;
static uint8_t g_p10_range_controller_candidate_count;
static uint8_t g_p10_range_controller_target_count;
static uint8_t g_p10_measure_phase = DMM_MEASURE_PHASE_PRECISION;
static uint8_t g_p10_phase_settle_remaining;
static uint16_t g_p10_phase_transition_count;
static uint16_t g_p10_phase_range_steps;
static uint32_t g_p10_phase_entered_at;
static uint32_t g_p10_mode_entered_at;
static uint32_t g_p10_first_precision_ms;
static uint32_t g_p10_phase_search_events;
static uint32_t g_p10_phase_settle_events;
static uint32_t g_p10_phase_precision_events;
static bool g_p10_ohms_stock_guard_state;
static uint32_t g_p10_ohms_stock_guard_until;
static bool g_p10_ohms_legacy_block_one_downshift;
static uint32_t g_p10_ohms_legacy_downshift_block_until;
static uint32_t g_p10_ohms_legacy_upshift_block_until;
static uint16_t g_p10_frontend_timing_word = P10_STOCK_TIMING_WORD;
static bool g_p10_frontend_timing_phase;
static uint16_t g_p10_frontend_timing_idle_counter;
static uint32_t g_p10_frontend_timing_toggle_at;
static bool g_frontend_timing_toggle_enabled = true;
static dmm_p10_diag_t g_p10_diag;
static bool g_current_single_aux_active;
static bool g_current_single_aux_enabled;
static uint8_t g_current_single_counter;
static int32_t g_current_single_aux_value;
static uint8_t g_current_single_jitter_count;
static int32_t g_current_single_jitter_sum;
static int32_t g_current_single_jitter_value;
static bool g_current_single_aux_over_threshold;
static int32_t g_current_single_extra_zero;
static bool g_current_dual_bank_active;
static uint8_t g_current_dual_bank;
static uint8_t g_current_dual_counter;
static uint8_t g_current_alt_range0;
static uint8_t g_current_alt_range1;
static int32_t g_current_bank0_value;
static int32_t g_current_bank1_value;
static int32_t g_current_bank_filter_samples[2][10];
static int32_t g_current_bank_filtered_value[2];
static uint8_t g_current_bank_filter_count[2];
static bool g_voltage_alt_bank_active;
static bool g_voltage_alt_primary_path;
static uint8_t g_voltage_alt_counter;
static int32_t g_voltage_alt_primary_value;
static int32_t g_voltage_alt_secondary_value;
static uint32_t g_frequency_secondary_raw;
static int32_t g_frequency_secondary_value;
static uint8_t g_frequency_secondary_range;
static bool g_frequency_secondary_valid;
static bool g_frequency_secondary_overrange;
static bool g_frequency_secondary_synced;
static uint32_t g_frequency_secondary_last_event_ms;
static uint32_t g_frequency_secondary_last_count;
static uint32_t g_frequency_secondary_last_ref;
static uint32_t g_frequency_secondary_last_wrap;
static bool g_lowz_alt_path;
static bool g_lowz_tight_threshold;
static uint8_t g_lowz_switch_countdown;
static uint8_t g_lowz_tight_countdown;
static uint32_t g_lowz_timing_toggle_at;
static int32_t g_temperature_cold_junction_emf;
static int32_t g_temperature_typek_input_emf;

typedef enum {
    P10_CAP_TIMING_OFF = 0,
    P10_CAP_TIMING_ARMED = 1,
    P10_CAP_TIMING_EVENT_RESTART = 2,
    P10_CAP_TIMING_TIMEOUT_PENDING = 3,
    P10_CAP_TIMING_RECONFIGURE = 4,
} p10_cap_timing_state_t;

static p10_cap_timing_state_t g_p10_cap_timing_state = P10_CAP_TIMING_OFF;
static uint32_t g_p10_cap_next_tick_at;
static uint16_t g_p10_cap_timeout_counter;
static bool g_p10_range_reconfigure_pending;

static void p10_configure_current_frontend(void);
static void p10_load_calibration(uint8_t mode, uint8_t range);
static bool p10_apply_autorange(int32_t value);
static bool p10_range_controller_fast_service(int32_t value, dmm_reading_t *reading);
static void p10_measure_phase_reset_for_mode(void);
static void p10_measure_phase_note_sample(void);
static void p10_measure_phase_note_display_ready(void);
static bool p10_current_single_aux_stock_mode(uint8_t mode);
static void p10_reset_current_single_aux_state(void);
static void p10_apply_current_mode_range_config(void);
static bool p10_mode_uses_frequency_secondary(void);
static uint8_t p10_frequency_secondary_decimal_for_range(uint8_t range);

static const uint16_t k_p10_mode9_thresholds[7] = {
    50000u, 50000u, 50000u, 50000u, 50000u, 50000u, 50000u,
};

static const uint16_t k_p10_mode12_thresholds[6] = {
    10000u, 10000u, 10000u, 10000u, 10000u, 10000u,
};

static const int16_t k_mode9_range6_x_kohm[13] = {
    0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60,
};

static const int16_t k_mode9_range6_y_percent[13] = {
    0, 37, 65, 85, 100, 100, 100, 87, 70, 40, 0, -37, -65,
};

static const int32_t k_corr_axis_9[9] = {
    50, 400, 1000, 10000, 30000, 50000, 70000, 90000, 100000
};

static const int32_t k_corr_axis_rel_low[9] = {
    50, 400, 1000, 3000, 5000, 7000, 10000, 15000, 20000
};

static const int32_t k_corr_axis_rel_high[9] = {
    20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000, 100000
};

static const int32_t k_corr_axis_7[7] = {
    50, 400, 1000, 3000, 5000, 7000, 10000
};

static const int32_t k_corr_mode0[18] = {
    0, 3, 0, -2, 1, 18, 47, 81, 100,
    0, -5, -1, 7, 40, 70, 90, 98, 100
};

static const int32_t k_corr_mode1[18] = {
    0, 1, -1, -2, -1, 15, 45, 82, 100,
    0, -2, -1, 3, 19, 40, 65, 88, 100
};

static const int32_t k_corr_mode4[14] = {
    0, 3, 10, 52, 77, 90, 100,
    0, 2, 9, 52, 77, 90, 100
};

static const int32_t k_corr_mode5[14] = {
    2, 9, 52, 77, 90, 100, 0,
    2, 9, 51, 77, 90, 100, 0
};

static const int32_t k_corr_mode6[14] = {
    2, 9, 51, 77, 90, 100, 0,
    -10, 3, 20, 35, 55, 100, 0
};

static const int32_t k_corr_mode7[14] = {
    -20, -8, 2, 16, 43, 100, 0,
    -10, 3, 20, 40, 55, 100, 0
};

static const int32_t k_corr_rel2_alt_low[18] = {
    0, 1, 18, 80, 95, 105, 109, 105, 100,
    0, -1, 14, 60, 83, 92, 97, 99, 100
};

static const int32_t k_corr_rel2_alt_high[18] = {
    0, 5, 13, 23, 36, 50, 66, 83, 100,
    0, 4, 10, 18, 29, 43, 60, 78, 100
};

static const int32_t k_corr_rel2_low[18] = {
    0, -10, 14, 80, 110, 120, 125, 115, 100,
    0, -3, 12, 60, 83, 92, 98, 100, 100
};

static const int32_t k_corr_rel2_high[18] = {
    0, 5, 13, 23, 36, 50, 66, 83, 100,
    0, 5, 12, 22, 33, 47, 63, 80, 100
};

static const int32_t k_corr_rel3_low[18] = {
    0, -5, 15, 90, 100, 110, 107, 105, 100,
    0, -5, 10, 57, 80, 89, 96, 99, 100
};

static const int32_t k_corr_rel3_high[18] = {
    0, 5, 13, 23, 36, 50, 66, 83, 100,
    0, 4, 8, 16, 27, 41, 58, 77, 100
};

static const int32_t k_current_alt_axis[17] = {
    50, 400, 1000, 2000, 3000, 5000, 7000, 10000, 20000,
    30000, 40000, 50000, 60000, 70000, 80000, 90000, 100000
};

static const int32_t k_current_alt_table[34] = {
    0, 7, 36, 92, 152, 171, 190, 206, 199, 199, 199, 195,
    231, 237, 237, 264, 269,
    0, 41, 294, 680, 1126, 1552, 1716, 1788, 1867, 1884,
    1912, 1931, 1949, 1962, 1970, 1981, 1979
};

#include "stock_voltage_alt_tables.inc"

static bool dmm_mode_is_current(uint8_t mode) {
    return mode == DMM_MODE_MA_AUX || mode == DMM_MODE_LEGACY_CURRENT_AC ||
           mode == DMM_MODE_MVA_AC || mode == DMM_MODE_MA_AC ||
           mode == DMM_MODE_MA_DC || mode == DMM_MODE_A_AC || mode == DMM_MODE_A_DC ||
           mode == DMM_MODE_A_DC_PERSIST || mode == DMM_MODE_A_AC_PERSIST ||
           mode == DMM_MODE_MA_BURDEN || mode == DMM_MODE_MA_ALT_AC ||
           mode == DMM_MODE_MVA_DC;
}

static bool dmm_mode_is_voltage(uint8_t mode) {
    return mode == DMM_MODE_LOZ || mode == DMM_MODE_V_DC || mode == DMM_MODE_V_AC ||
           mode == DMM_MODE_MV_DC || mode == DMM_MODE_MV_AC || mode == DMM_MODE_TEMP;
}

static bool dmm_mode_uses_voltage_frontend(uint8_t mode) {
    return mode == DMM_MODE_LOZ || mode == DMM_MODE_V_DC || mode == DMM_MODE_V_AC;
}

static bool dmm_mode_is_freq_group(uint8_t mode) {
    return mode >= DMM_MODE_FREQ && mode <= DMM_MODE_DUTY;
}

static bool dmm_mode_is_dual_current(uint8_t mode) {
    return mode == DMM_MODE_MA_AUX || mode == DMM_MODE_MA_BURDEN ||
           mode == DMM_MODE_LEGACY_CURRENT_AC || mode == DMM_MODE_MVA_AC ||
           mode == DMM_MODE_MA_ALT_AC || mode == DMM_MODE_MVA_DC;
}

static bool p10_ohms_stock_logic_enabled(void) {
    return g_p10_ohms_guard_policy == P10_OHMS_GUARD_STOCK;
}

static void p10_range_controller_reset(void) {
    g_p10_range_controller_candidate_dir = 0;
    g_p10_range_controller_candidate_count = 0;
    g_p10_range_controller_target_count = 0;
}

static void p10_measure_phase_enter(uint8_t phase) {
    if (g_p10_measure_phase != phase) {
        g_p10_measure_phase = phase;
        if (g_p10_phase_transition_count != 0xFFFFu) {
            ++g_p10_phase_transition_count;
        }
        g_p10_phase_entered_at = clock_millis();
    }
}

static bool p10_ohms_search_profile_active(void) {
    return g_p10_mode == DMM_MODE_P10_OHMS &&
           g_p10_autorange &&
           g_p10_task_fast_range_enabled &&
           p10_ohms_stock_logic_enabled() &&
           g_p10_measure_phase == DMM_MEASURE_PHASE_SEARCH;
}

static void p10_measure_phase_reset_for_mode(void) {
    uint32_t now = clock_millis();
    g_p10_mode_entered_at = now;
    g_p10_phase_entered_at = now;
    g_p10_first_precision_ms = 0xFFFFFFFFu;
    g_p10_phase_transition_count = 0u;
    g_p10_phase_range_steps = 0u;
    g_p10_phase_search_events = 0u;
    g_p10_phase_settle_events = 0u;
    g_p10_phase_precision_events = 0u;
    g_p10_phase_settle_remaining = 0u;

    if (g_p10_mode != DMM_MODE_P10_OHMS) {
        g_p10_measure_phase = DMM_MEASURE_PHASE_PRECISION;
        return;
    }
    if (g_p10_autorange && g_p10_task_fast_range_enabled &&
        p10_ohms_stock_logic_enabled()) {
        g_p10_measure_phase = DMM_MEASURE_PHASE_SEARCH;
        return;
    }

    g_p10_measure_phase = DMM_MEASURE_PHASE_SETTLE;
    g_p10_phase_settle_remaining = P10_OHMS_PRECISION_SETTLE_EVENTS;
    g_p10_settle_samples = g_p10_phase_settle_remaining;
}

static void p10_ohms_begin_search(void) {
    if (g_p10_mode != DMM_MODE_P10_OHMS) {
        return;
    }
    g_p10_phase_settle_remaining = 0u;
    g_p10_settle_samples = 0u;
    p10_measure_phase_enter(DMM_MEASURE_PHASE_SEARCH);
}

static void p10_ohms_begin_settle(void) {
    if (g_p10_mode != DMM_MODE_P10_OHMS) {
        return;
    }
    /*
     * SEARCH has already selected the range. A range-change holdoff left
     * here would discard the first complete eight-sample precision block.
     */
    g_p10_autorange_holdoff = 0u;
    p10_range_controller_reset();
    g_p10_phase_settle_remaining = P10_OHMS_PRECISION_SETTLE_EVENTS;
    g_p10_settle_samples = g_p10_phase_settle_remaining;
    p10_measure_phase_enter(DMM_MEASURE_PHASE_SETTLE);
}

static void p10_measure_phase_note_sample(void) {
    if (g_p10_mode != DMM_MODE_P10_OHMS) {
        return;
    }
    if (g_p10_measure_phase == DMM_MEASURE_PHASE_SEARCH) {
        ++g_p10_phase_search_events;
    } else if (g_p10_measure_phase == DMM_MEASURE_PHASE_SETTLE) {
        ++g_p10_phase_settle_events;
    } else if (g_p10_measure_phase == DMM_MEASURE_PHASE_PRECISION) {
        ++g_p10_phase_precision_events;
    }
}

static void p10_measure_phase_note_display_ready(void) {
    if (g_p10_mode == DMM_MODE_P10_OHMS &&
        g_p10_measure_phase == DMM_MEASURE_PHASE_PRECISION &&
        g_p10_first_precision_ms == 0xFFFFFFFFu) {
        g_p10_first_precision_ms = clock_millis() - g_p10_mode_entered_at;
    }
}

static void p10_reset_average(void);
static void p10_reset_filter(void);
static void p10_schedule_cap_timeout(void);
static void p10_configure_current_frontend(void);
static void p10_frontend_timing_idle_service(void);
static void p10_apply_current_mode_range_config(void);
static bool p10_mode_uses_frontend_timing_toggle(void);
static bool p10_current_single_main_stock_mode(uint8_t mode);

#include "stock_typek_table.inc"

static const uint32_t k_dual_current_temp_y_axis[3][8] = {
    {0u, 500u, 1000u, 5000u, 10000u, 30000u, 50000u, 60000u},
    {0u, 100u, 1000u, 5000u, 10000u, 30000u, 50000u, 60000u},
    {0u, 10u, 100u, 500u, 1000u, 5000u, 10000u, 12000u},
};

static const uint32_t k_dual_current_temp_x_axis[8] = {
    0u, 10u, 1000u, 5000u, 10000u, 30000u, 50000u, 60000u,
};

static const int16_t k_mode13_temp_comp_table[2][4][8][8] = {
    {
        {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, -1, 30, 28, 41, 177, 416, 607},
            {0, 0, 24, 26, 39, 173, 409, 594},
            {0, -26, 9, 8, 20, 146, 390, 540},
            {0, -77, -25, -13, -8, 122, 348, 519},
            {0, -405, -245, -187, -157, -27, 185, 317},
            {0, -773, -581, -460, -397, -180, 0, 145},
            {0, -975, -765, -646, -569, -305, -61, 60},
        },
        {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, -5, 6, 9, 24, 187, 457, 633},
            {0, -2, 0, 6, 24, 179, 452, 618},
            {0, -44, -17, -13, 5, 167, 413, 600},
            {0, -103, -51, -38, -23, 123, 384, 568},
            {0, -410, -276, -221, -193, -29, 200, 364},
            {0, -791, -622, -489, -421, -196, 0, 183},
            {0, -1046, -799, -689, -579, -309, -28, 105},
        },
        {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 1, 19, 20, 37, 198, 465, 628},
            {0, -3, 17, 23, 41, 202, 447, 603},
            {0, -55, 34, 31, 50, 205, 445, 589},
            {0, -143, 43, 39, 52, 202, 445, 555},
            {0, -395, -291, -217, -182, -23, 207, 356},
            {0, -822, -617, -485, -432, -188, 0, 180},
            {0, -1036, -804, -680, -589, -313, -34, 96},
        },
        {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 2, 8, 9, 21, 184, 435, 609},
            {0, -1, -2, 4, 15, 177, 428, 596},
            {0, -41, -18, -26, -3, 149, 417, 573},
            {0, -108, -62, -44, -31, 113, 378, 493},
            {0, -431, -310, -233, -203, -53, 166, 306},
            {0, -841, -635, -533, -440, -225, 0, 119},
            {0, -1107, -858, -694, -612, -321, -74, 41},
        },
    },
    {
        {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 2, 22, 24, 43, 223, 505, 686},
            {0, 0, 17, 21, 38, 212, 512, 676},
            {0, -38, -11, -2, 15, 181, 301, 657},
            {0, -75, -55, -31, -17, 144, 439, 564},
            {0, -444, -240, -196, -155, 15, 291, 433},
            {0, -914, -727, -597, -513, -252, 0, 133},
            {0, -1165, -960, -768, -661, -382, -91, 34},
        },
        {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, -4, 12, 16, 36, 225, 496, 663},
            {0, -3, 5, 13, 33, 222, 488, 685},
            {0, -51, -20, -5, 10, 183, 470, 654},
            {0, -122, -62, -41, -22, 139, 424, 594},
            {0, -463, -350, -283, -215, -44, 207, 361},
            {0, -894, -732, -575, -506, -246, 0, 136},
            {0, -1187, -905, -782, -697, -372, -86, 40},
        },
        {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, -1, 16, 18, 37, 334, 530, 682},
            {0, -2, 6, 14, 31, 218, 504, 650},
            {0, -51, -23, -13, 9, 188, 475, 634},
            {0, -123, -68, -43, -30, 150, 438, 584},
            {0, -469, -341, -271, -218, -48, 212, 370},
            {0, -943, -720, -597, -524, -254, 0, 138},
            {0, -1175, -960, -791, -699, -388, -89, 39},
        },
        {
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, -5, 11, 15, 40, 242, 557, 756},
            {0, -9, 2, 11, 37, 242, 550, 717},
            {0, -64, -31, -18, 8, 198, 505, 688},
            {0, -152, -79, -51, -29, 148, 447, 627},
            {0, -522, -369, -289, -239, -52, 210, 391},
            {0, -960, -762, -665, -543, -258, 0, 143},
            {0, -1246, -961, -822, -720, -400, -94, 41},
        },
    },
};

static const int16_t k_mode15_temp_comp_table8[2][4][64] = {
    {
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 20, 14, 24, 164, 399, 553,
            0, 0, 15, 13, 25, 155, 401, 528,
            0, -31, 3, 0, 10, 141, 364, 530,
            0, -85, -24, -19, -10, 111, 345, 475,
            0, -375, -220, -174, -152, -20, 188, 334,
            0, -781, -501, -413, -369, -165, 0, 188,
            0, -1020, -717, -583, -512, -234, 8, 123,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 11, 13, 28, 193, 465, 626,
            0, 0, 3, 107, 25, 176, 446, 614,
            0, -42, -17, -10, 6, 153, 425, 581,
            0, -105, -54, -38, -22, 123, 367, 527,
            0, -374, -280, -217, -180, -32, 201, 359,
            0, -805, -595, -490, -415, -192, 0, 309,
            0, -1007, -800, -653, -544, -296, -20, 110,
        },
        {0},
        {0},
    },
    {
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 21, 20, 122, 216, 494, 662,
            0, -3, 9, 15, 35, 211, 473, 664,
            0, -43, -13, -4, 12, 179, 468, 615,
            0, -113, -57, -36, -20, 143, 411, 573,
            0, -438, -335, -241, -221, -40, 192, 356,
            0, -926, -698, -582, -513, -242, 0, 132,
            0, -1174, -896, -783, -662, -378, -92, 42,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, 20, 23, 45, 236, 487, 713,
            0, -4, 9, 18, 36, 231, 502, 664,
            0, -48, -16, -4, 14, 198, 460, 660,
            0, -118, -64, -38, -20, 151, 412, 602,
            0, -471, -329, -258, -229, -37, 205, 356,
            0, -935, -708, -601, -520, -260, 0, 140,
            0, -1214, -944, -793, -679, -383, -98, 44,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 2, 2, -78, -158, -362, -455, -492,
            0, 3, 12, -75, -154, -354, -443, -471,
            0, -35, 54, -49, -132, -337, -446, -470,
            0, -100, 100, -22, -102, -306, -412, -438,
            0, -518, 421, 184, 97, -115, -246, -258,
            0, -1115, 446, 423, 387, 132, 0, -27,
            0, -1115, 456, 572, 499, 227, 67, 50,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 12, -18, -36, -2, 102, 214,
            0, -2, 12, -18, -36, -8, 133, 225,
            0, -4, 3, -22, -41, -23, 129, 221,
            0, -15, -3, -28, -48, -35, 125, 234,
            0, -131, -86, -93, -104, -68, 83, 146,
            0, -273, -200, -119, -126, -137, 0, 80,
            0, -273, -213, -105, -119, -117, -42, 35,
        },
    },
};

static const int16_t k_mode15_temp_comp_table7_nc[2][64] = {
    {
        0, 0, 0, 0, 0, 0, 0,
        0, -2, -91, -187, -475, -644, -706,
        0, 6, -85, -181, -468, -624, -707,
        0, 36, -52, -148, -434, -606, -694,
        0, 73, -13, -111, -397, -584, -644,
        0, 385, 291, 206, -105, -304, -342,
        0, 635, 675, 540, 226, 0, 15,
        0, 545, 777, 662, 308, 200, 139,
        0, -2, -92, -187, -473, -658, -741, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0,
        0, 6, -27, -54, -55, 45, 120,
        0, 6, -28, -51, -64, 31, 149,
        0, 1, -26, -51, -47, 56, 134,
        0, 0, -24, -50, -50, 48, 140,
        0, -22, -29, -54, -55, 41, 108,
        0, -47, -49, -91, -62, 0, 87,
        0, -107, -117, -121, -119, -27, 66,
        0, 6, -27, -54, -62, 48, 108, 0,
    },
};

static const int16_t k_mode24_temp_comp_table[3][4][64] = {
    {
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, -1, -5, -12, -37, -62, -76,
            0, 1, -1, -5, -11, -36, -62, -76,
            0, 9, 8, 0, -6, -31, -56, -70,
            0, 17, 17, 6, 1, -24, -50, -64,
            0, 54, 54, 31, 26, 1, -25, -38,
            0, 91, 91, 56, 51, 26, 0, -13,
            0, 108, 109, 68, 62, 38, 13, -2,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, -3, -7, -9,
            0, 0, 0, 0, 0, -4, -7, -9,
            0, 0, 1, 1, 0, -4, -7, -9,
            0, 1, 1, 1, 1, -3, -6, -8,
            0, 5, 4, 4, 3, -1, -5, -6,
            0, 9, 6, 7, 5, 1, -2, -3,
            0, 9, 7, 7, 7, 3, 0, -3,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 1, -16, -82, -165, -497, -830, -998,
            0, 9, -8, -75, -158, -490, -823, -990,
            0, 43, 27, -42, -126, -457, -791, -958,
            0, 85, 71, -2, -84, -411, -736, -905,
            0, 472, 458, 353, 268, -51, -388, -557,
            0, 953, 938, 781, 704, 354, 0, -151,
            0, 1092, 1065, 916, 835, 501, 164, -4,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, -2, -10, -21, -66, -112, -134,
            0, 1, -1, -9, -20, -66, -111, -133,
            0, 9, 4, -4, -15, -61, -106, -128,
            0, 17, 10, 3, -8, -55, -100, -122,
            0, 75, 54, 46, 34, -11, -58, -81,
            0, 156, 111, 101, 90, 45, -4, -25,
            0, 116, 107, 94, 85, 41, -4, -25,
        },
    },
    {
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, -3, -6, -20, -36, -45,
            0, 1, 1, -2, -6, -20, -35, -44,
            0, 6, 5, 1, -3, -17, -33, -41,
            0, 10, 11, 4, 1, -14, -29, -38,
            0, 35, 36, 17, 14, 0, -15, -25,
            0, 59, 60, 42, 28, 14, 0, -10,
            0, 69, 69, 38, 34, 20, 5, -4,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, -1, -5, -7, -8,
            0, 0, 0, 0, -2, -5, -7, -8,
            0, 0, 0, 0, -1, -4, -6, -8,
            0, 0, 0, 0, 0, -4, -6, -7,
            0, 2, 1, 2, 1, -3, -5, -6,
            0, 5, 3, 3, 2, 0, 0, -5,
            0, 6, 4, 4, 3, -1, -3, -4,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, -2, -12, -58, -115, -343, -573, -689,
            0, 3, -7, -52, -109, -338, -479, -683,
            0, 24, 15, -30, -87, -315, -546, -662,
            0, 52, 43, -2, -59, -287, -518, -633,
            0, 276, 268, 223, 166, -62, -292, -407,
            0, 566, 563, 522, 467, 242, 0, -95,
            0, 798, 796, 682, 658, 381, 107, 69,
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, -1, -5, -10, -34, -61, -73,
            0, 0, 0, -4, -11, -36, -61, -73,
            0, 1, 2, -3, -8, -32, -58, -71,
            0, 5, 5, 0, -5, -28, -54, -66,
            0, 25, 27, 23, 17, -6, -34, -45,
            0, 96, 66, 61, 55, 29, 0, -12,
            0, 77, 72, 68, 63, 40, 15, 2,
        },
    },
    {
        {0},
        {0},
        {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, -3, -16, -71, -140, -418, -697, -846,
            0, -9, -22, -77, -206, -424, -703, -844,
            0, -37, -49, -105, -174, -451, -731, -870,
            0, -70, -83, -138, -208, -484, -764, -903,
            0, -342, -352, -408, -477, -755, -1035, -1175,
            0, -684, -702, -761, -832, -1111, -1391, -1533,
            0, -884, -2, -961, -1032, -1311, -1591, -1733,
        },
        {0},
    },
};

bool dmm_fast_diode_enabled(void) {
    return EEV_ENABLE_FAST_DIODE != 0;
}

void dmm_init(void) {
    if (dmm_fast_diode_enabled()) {
        frontend_configure_diode(P10_STOCK_TIMING_WORD);
    }
}

void dmm_poll(void) {
    if (!dmm_fast_diode_enabled()) {
        return;
    }

    if (!frontend_ready()) {
        return;
    }

    frontend_read_status(g_frontend_status);
    if (!frontend_status_has_sample(g_frontend_status)) {
        return;
    }

    int32_t sample = frontend_decode_signed24_sample(g_frontend_status);
    if (sample < DIODE_BEEP_MAX && sample >= DIODE_BEEP_MIN) {
        buzzer_force_on_ms(DIODE_BEEP_MS);
    }
}

uint8_t dmm_p10_cycle_mode(uint8_t mode) {
    if (mode == DMM_MODE_V_DC) {
        g_voltage_alt_bank_active = false;
        return DMM_MODE_V_AC;
    }
    if (mode == DMM_MODE_V_AC) {
        if (!g_voltage_alt_bank_active) {
            g_voltage_alt_bank_active = true;
            g_voltage_alt_primary_path = false;
            g_voltage_alt_counter = 0;
            g_voltage_alt_primary_value = 0;
            g_voltage_alt_secondary_value = 0;
            return DMM_MODE_V_AC;
        }
        g_voltage_alt_bank_active = false;
        return DMM_MODE_V_DC;
    }
    return rotary_mode_task_cycle(mode);
}

uint8_t dmm_p10_default_range(uint8_t mode) {
    if (mode == DMM_MODE_P10_OHMS && p10_ohms_stock_logic_enabled()) {
        /*
         * Stock enters automatic Ohms at range 5. Range 6 remains available
         * through manual RANGE selection, but is not part of autoranging.
         */
        return 5u;
    }
    return 0u;
}

uint8_t dmm_p10_next_range(uint8_t mode, uint8_t range) {
    uint8_t count = measurement_profile_manual_range_count(mode);
    ++range;
    if (range >= count) {
        range = 0u;
    }
    return range;
}

static uint32_t div_u64_u32(uint64_t numerator, uint32_t denominator) {
    if (denominator == 0u) {
        return 0xFFFFFFFFu;
    }

    uint64_t remainder = 0;
    uint32_t quotient = 0;
    uint64_t divisor = denominator;

    for (uint8_t i = 0; i < 64u; ++i) {
        uint8_t bit = (uint8_t)(63u - i);
        remainder = (remainder << 1) | ((numerator >> bit) & 1u);
        if (remainder >= divisor) {
            remainder -= divisor;
            if (bit >= 32u) {
                quotient = 0xFFFFFFFFu;
            } else if (quotient != 0xFFFFFFFFu) {
                quotient |= (uint32_t)(1u << bit);
            }
        }
    }

    return quotient;
}

static uint32_t abs_i32_to_u32(int32_t value) {
    if (value < 0) {
        return (uint32_t)(-value);
    }
    return (uint32_t)value;
}

static int32_t clamp_i32(int32_t value, int32_t min_value, int32_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint32_t isqrt_u64(uint64_t value) {
    uint64_t bit = 1ULL << 62;
    uint64_t result = 0;

    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0u) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)result;
}

static int32_t apply_gain_fixed(int32_t value, uint32_t gain) {
    uint32_t abs_value = abs_i32_to_u32(value);
    /*
     * Stock stores the gain as an integer and loads gain_factor = gain / scale.
     * Its fixed/float helper then divides the offset-corrected value by that
     * factor, which is equivalent to value * scale / gain.
     */
    uint32_t scaled_abs = div_u64_u32((uint64_t)abs_value * g_p10_gain_scale, gain);
    if (scaled_abs > 0x7FFFFFFFu) {
        scaled_abs = 0x7FFFFFFFu;
    }
    if (value < 0) {
        return -(int32_t)scaled_abs;
    }
    return (int32_t)scaled_abs;
}

static int32_t interpolate_i32(const int32_t *table, uint8_t index,
                               int32_t numerator, int32_t denominator) {
    int32_t lo = table[index - 1u];
    int32_t hi = table[index];
    if (denominator == 0) {
        return lo;
    }
    int32_t delta = (hi - lo) * numerator;
    if (delta >= 0) {
        delta += denominator / 2;
    } else {
        delta -= denominator / 2;
    }
    return lo + delta / denominator;
}

static int32_t stock_scale_secondary(uint8_t scale, int32_t value) {
    if (scale == 0u) {
        return value / 1000;
    }
    if (scale == 1u) {
        return value / 100;
    }
    if (scale == 2u) {
        return value / 10;
    }
    if (scale == 4u) {
        return value * 10;
    }
    return value;
}

static int32_t clamp_stock_threshold(int32_t value, bool wide) {
    int32_t limit = wide ? 20000 : 10000;
    return (value > limit || value < -limit) ? 0 : value;
}

static uint32_t p10_threshold_offset_for_mode_range(uint8_t mode, uint8_t range) {
    return calibration_stock_threshold_offset(mode, range, g_lowz_alt_path);
}

static void p10_load_stock_thresholds(uint8_t mode, uint8_t range) {
    uint32_t offset;
    if (mode == DMM_MODE_LOZ) {
        offset = p10_threshold_offset_for_mode_range(DMM_MODE_V_AC, 3u);
    } else {
        offset = p10_threshold_offset_for_mode_range(mode, range);
    }
    g_cal_threshold0_signed =
        clamp_stock_threshold((int16_t)calibration_read_u16_be(offset), true);
    g_cal_threshold1_signed =
        clamp_stock_threshold((int16_t)calibration_read_u16_be(offset + 2u), true);
    g_cal_threshold2_signed =
        clamp_stock_threshold((int16_t)calibration_read_u16_be(offset + 4u), false);
    g_cal_threshold3_signed =
        clamp_stock_threshold((int16_t)calibration_read_u16_be(offset + 6u), false);
}

static bool p10_stock_secondary_for_correction(int32_t *value, uint8_t *scale) {
    if (g_p10_mode == DMM_MODE_V_AC && g_voltage_alt_bank_active) {
        *value = g_voltage_alt_secondary_value;
        *scale = g_p10_range == 0u ? 0u : (g_p10_range == 1u ? 1u : 2u);
        return true;
    }
    if (g_p10_mode == DMM_MODE_MA_AC || g_p10_mode == DMM_MODE_A_AC ||
        g_p10_mode == DMM_MODE_A_DC_PERSIST) {
        if (g_current_single_aux_enabled) {
            *value = g_current_single_aux_value;
            *scale = 0u;
            return true;
        }
        if (dmm_mode_is_dual_current(g_p10_mode)) {
            *value = g_current_bank1_value;
            *scale = g_current_alt_range1;
            return true;
        }
    }
    return false;
}

static bool p10_stock_correction_mode(uint8_t *corr_mode) {
    if (g_p10_mode == DMM_MODE_MV_AC) {
        *corr_mode = g_p10_range;
        return true;
    }
    if (g_p10_mode == DMM_MODE_V_AC && !g_voltage_alt_bank_active) {
        *corr_mode = (uint8_t)(g_p10_range + 2u);
        return true;
    }
    if (g_p10_mode == DMM_MODE_LOZ && g_lowz_alt_path) {
        *corr_mode = 5u;
        return true;
    }
    if (g_p10_mode == DMM_MODE_MA_AC) {
        *corr_mode = 6u;
        return true;
    }
    if (g_p10_mode == DMM_MODE_A_AC) {
        *corr_mode = 7u;
        return true;
    }
    if (g_p10_mode == DMM_MODE_A_DC_PERSIST) {
        *corr_mode = 8u;
        return true;
    }
    return false;
}

static int32_t stock_threshold_relative_correction(int32_t value, uint8_t corr_mode,
                                                   int32_t secondary_scaled,
                                                   bool marker31_missing) {
    uint8_t index = 8u;
    const int32_t *axis = secondary_scaled < 20001 ? k_corr_axis_rel_low :
                          k_corr_axis_rel_high;
    for (uint8_t i = 1u; i < 9u; ++i) {
        if (axis[i] >= secondary_scaled) {
            index = i;
            break;
        }
    }

    const int32_t *table;
    if (corr_mode == 2u) {
        if (marker31_missing) {
            table = secondary_scaled < 20001 ? k_corr_rel2_alt_low :
                    k_corr_rel2_alt_high;
        } else {
            table = secondary_scaled < 20001 ? k_corr_rel2_low :
                    k_corr_rel2_high;
        }
    } else {
        table = secondary_scaled < 20001 ? k_corr_rel3_low :
                k_corr_rel3_high;
    }

    int32_t den = axis[index] - axis[index - 1u];
    int32_t num = secondary_scaled - axis[index - 1u];
    int32_t pct0 = interpolate_i32(table, index, num, den);
    int32_t pct1 = interpolate_i32(&table[9], index, num, den);
    int32_t t0 = g_cal_threshold0_signed;
    int32_t t1 = g_cal_threshold1_signed;
    int32_t t2 = g_cal_threshold2_signed;
    int32_t t3 = g_cal_threshold3_signed;
    int32_t c0;
    int32_t c1;
    if (secondary_scaled < 20001) {
        c0 = (t2 * pct0) / 100;
        c1 = (t3 * pct1) / 100;
    } else {
        c0 = t2 + ((t1 - t3) * pct0) / 100;
        c1 = t3 + ((t0 - t2) * pct1) / 100;
    }

    int32_t pivot = t2 + 5000;
    if (pivot >= value) {
        int32_t pct = pivot != 0 ? ((pivot - value) * 100) / pivot : 0;
        return c0 - (c0 * pct) / 100;
    }
    int32_t span = 100000 - pivot;
    int32_t pct = span != 0 ? ((value - pivot) * 100) / span : 0;
    return c1 - ((c1 - c0) * (100 - pct)) / 100;
}

static int32_t stock_threshold_correction(int32_t value, uint8_t corr_mode,
                                          int32_t secondary_value, uint8_t secondary_scale,
                                          bool marker31_missing) {
    if ((corr_mode == 5u && value >= 6600) ||
        (corr_mode == 4u && value >= 60000) ||
        value >= 55000 || corr_mode >= 8u) {
        return 0;
    }

    int32_t secondary_scaled = stock_scale_secondary(secondary_scale, secondary_value);
    if (secondary_scaled < 51) {
        return 0;
    }
    if (corr_mode == 2u || corr_mode == 3u) {
        return stock_threshold_relative_correction(value, corr_mode,
                                                   secondary_scaled,
                                                   marker31_missing);
    }

    uint8_t count = corr_mode >= 4u ? 7u : 9u;
    const int32_t *axis = corr_mode >= 4u ? k_corr_axis_7 : k_corr_axis_9;
    uint8_t index = (uint8_t)(count - 1u);
    for (uint8_t i = 1u; i < count; ++i) {
        if (axis[i] >= secondary_scaled) {
            index = i;
            break;
        }
    }
    int32_t axis_den = axis[index] - axis[index - 1u];
    int32_t axis_num = secondary_scaled - axis[index - 1u];
    int32_t pct_axis = axis_den != 0 ? (axis_num * 100) / axis_den : 0;
    bool high_value = value >= 5001;
    const int32_t *table = corr_mode == 1u ? k_corr_mode1 : k_corr_mode0;
    if (corr_mode >= 4u) {
        if (corr_mode == 4u) {
            table = k_corr_mode4;
        } else if (corr_mode == 5u) {
            table = k_corr_mode5;
        } else if (corr_mode == 6u) {
            table = k_corr_mode6;
        } else {
            table = k_corr_mode7;
        }
    }
    uint8_t base = high_value ? count : 0u;
    int32_t pct = interpolate_i32(&table[base], index, pct_axis, 100);
    int32_t t0 = g_cal_threshold0_signed;
    int32_t t1 = g_cal_threshold1_signed;
    int32_t pivot = (t0 * pct) / 100;
    int32_t lower = (t1 * pct) / 100;
    int32_t offset = corr_mode == 5u ? 600 : 5000;
    int32_t span = corr_mode == 5u ? 5400 : 45000;
    int32_t base_start = corr_mode == 5u ? 600 : 5000;
    if (value > pivot + offset) {
        int32_t den = lower + span;
        return lower + (den != 0 ? ((lower - pivot) * (value - base_start)) / den : 0);
    }
    int32_t den = pivot + offset;
    return den != 0 ? (pivot * value) / den : 0;
}

static int32_t stock_current_alt_secondary_correction(int32_t value,
                                                      int32_t secondary_value,
                                                      uint8_t secondary_scale) {
    if (value >= 57000) {
        return 0;
    }
    int32_t secondary_scaled = stock_scale_secondary(secondary_scale, secondary_value);
    if (secondary_scaled < 51) {
        return 0;
    }

    uint8_t index = 16u;
    for (uint8_t i = 1u; i < 17u; ++i) {
        if (k_current_alt_axis[i] >= secondary_scaled) {
            index = i;
            break;
        }
    }
    int32_t den = k_current_alt_axis[index] - k_current_alt_axis[index - 1u];
    int32_t num = secondary_scaled - k_current_alt_axis[index - 1u];
    int32_t c0;
    int32_t c1;
    if (value >= 5001) {
        c0 = interpolate_i32(k_current_alt_table, index, num, den);
        c1 = interpolate_i32(&k_current_alt_table[17], index, num, den);
    } else {
        c0 = 0;
        c1 = interpolate_i32(k_current_alt_table, index, num, den);
    }
    int32_t pivot = c0 + 5000;
    int32_t pct;
    if (pivot >= value) {
        pct = pivot != 0 ? ((pivot - value) * 100) / pivot : 0;
    } else {
        int32_t span = 100000 - pivot;
        pct = span != 0 ? ((value - pivot) * 100) / span : 0;
    }
    return c1 - ((c1 - c0) * (100 - pct)) / 100;
}

static int32_t voltage_alt_axis_value(uint8_t index) {
    if (index <= 198u) {
        return 20 + (int32_t)index * 10;
    }
    if (index <= 213u) {
        return 2200 + (int32_t)(index - 199u) * 200;
    }
    return index == 214u ? 5500 : 6000;
}

static int32_t stock_voltage_alt_table_interp(const uint16_t *table,
                                              uint8_t index,
                                              int32_t secondary_scaled) {
    int32_t lo_axis = voltage_alt_axis_value(index - 1u);
    int32_t hi_axis = voltage_alt_axis_value(index);
    int32_t pair[2] = {(int32_t)table[index - 1u], (int32_t)table[index]};
    return interpolate_i32(pair, 1u, secondary_scaled - lo_axis, hi_axis - lo_axis);
}

static int32_t __attribute__((unused))
stock_voltage_alt_secondary_compensation(int32_t value,
                                         int32_t secondary_value,
                                         uint8_t secondary_scale) {
    int32_t secondary_scaled = stock_scale_secondary(secondary_scale, secondary_value);
    if (secondary_scaled < 20) {
        return value;
    }
    if (secondary_scaled >= 6001) {
        return 0;
    }

    uint8_t index = 215u;
    for (uint8_t i = 1u; i < 216u; ++i) {
        if (voltage_alt_axis_value(i) >= secondary_scaled) {
            index = i;
            break;
        }
    }

    uint32_t abs_value = abs_i32_to_u32(value);
    int32_t factor;
    if (abs_value < 100000u) {
        factor = stock_voltage_alt_table_interp(k_voltage_alt_low, index, secondary_scaled);
    } else if (abs_value >= 500001u) {
        factor = stock_voltage_alt_table_interp(k_voltage_alt_high, index, secondary_scaled);
    } else {
        int32_t low = stock_voltage_alt_table_interp(k_voltage_alt_low, index, secondary_scaled);
        int32_t high = stock_voltage_alt_table_interp(k_voltage_alt_high, index, secondary_scaled);
        int32_t pair[2] = {low, high};
        factor = interpolate_i32(pair, 1u, (int32_t)abs_value - 100000, 400000);
    }

    uint32_t scaled = div_u64_u32((uint64_t)abs_value * (uint32_t)factor, 10000u);
    if (scaled > 0x7FFFFFFFu) {
        scaled = 0x7FFFFFFFu;
    }
    return value < 0 ? -(int32_t)scaled : (int32_t)scaled;
}

static int32_t p10_apply_stock_threshold_corrections(int32_t value) {
    uint8_t corr_mode;
    int32_t secondary;
    uint8_t secondary_scale;
    bool marker31_missing = calibration_read_u8(31u) != 0xA5u;
    /*
     * Stock enters AC+DC with g_measure_alt_path_b9 clear. That means the
     * voltage-alt compensation table is not applied to either AC+DC subpath;
     * the two already-calibrated subpath values are combined later as RMS.
     */
    if (p10_stock_correction_mode(&corr_mode) &&
        p10_stock_secondary_for_correction(&secondary, &secondary_scale)) {
        value -= stock_threshold_correction(value, corr_mode, secondary,
                                            secondary_scale, marker31_missing);
    }
    if (marker31_missing && g_p10_mode == DMM_MODE_V_AC && !g_voltage_alt_bank_active &&
        g_p10_range == 0u &&
        p10_stock_secondary_for_correction(&secondary, &secondary_scale)) {
        value -= stock_current_alt_secondary_correction(value, secondary, secondary_scale);
    }
    return value;
}

uint8_t dmm_p10_decimal_position(uint8_t mode, uint8_t range) {
    if (mode == DMM_MODE_P10_DIODE) {
        return g_p10_diode_option_15 ? 3u : 4u;
    }
    return measurement_profile_decimal_position(mode, range);
}

void dmm_p10_set_autorange(bool enabled) {
    g_p10_autorange = enabled;
}

bool dmm_p10_autorange_enabled(void) {
    return g_p10_autorange;
}

void dmm_p10_set_ohms_stock_guard(bool enabled) {
    g_p10_ohms_guard_policy = enabled ? P10_OHMS_GUARD_STOCK : P10_OHMS_GUARD_LEGACY;
    g_p10_ohms_stock_guard_state = false;
    g_p10_ohms_stock_guard_until = 0;
    p10_range_controller_reset();
    g_p10_ohms_legacy_block_one_downshift = false;
    g_p10_ohms_legacy_downshift_block_until = 0;
    g_p10_ohms_legacy_upshift_block_until = 0;
}

bool dmm_p10_ohms_stock_guard_enabled(void) {
    return g_p10_ohms_guard_policy == P10_OHMS_GUARD_STOCK;
}

void dmm_p10_set_task_fast_range(bool enabled) {
    g_p10_task_fast_range_enabled = enabled;
    p10_range_controller_reset();
}

bool dmm_p10_task_fast_range_enabled(void) {
    return g_p10_task_fast_range_enabled;
}

void dmm_p10_set_continuity_option(uint8_t option) {
    g_p10_continuity_option = option < 4u ? option : 0u;
}

uint8_t dmm_p10_continuity_option(void) {
    return g_p10_continuity_option;
}

void dmm_p10_toggle_diode_option(void) {
    g_p10_diode_option_15 = !g_p10_diode_option_15;
    if (g_p10_mode == DMM_MODE_P10_DIODE) {
        frontend_set_diode_voltage_15v(g_p10_diode_option_15);
        g_p10_diode_voltage_gpio_active = true;
    }
    p10_reset_average();
    p10_reset_filter();
}

bool dmm_p10_diode_option_15(void) {
    return g_p10_diode_option_15;
}

bool dmm_p10_voltage_alt_active(void) {
    return g_p10_mode == DMM_MODE_V_AC && g_voltage_alt_bank_active;
}

void dmm_p10_voltage_set_alt_active(bool enabled) {
    g_voltage_alt_bank_active = enabled;
    g_voltage_alt_primary_path = false;
    g_voltage_alt_counter = 0;
    g_voltage_alt_primary_value = 0;
    g_voltage_alt_secondary_value = 0;
}

int32_t dmm_p10_voltage_alt_secondary_value(void) {
    return g_voltage_alt_secondary_value;
}

bool dmm_p10_frequency_secondary_active(void) {
    return p10_mode_uses_frequency_secondary();
}

bool dmm_p10_frequency_secondary_valid(void) {
    return g_frequency_secondary_valid;
}

bool dmm_p10_frequency_secondary_overrange(void) {
    return g_frequency_secondary_overrange;
}

int32_t dmm_p10_frequency_secondary_display_value(void) {
    return g_frequency_secondary_value / 10;
}

uint8_t dmm_p10_frequency_secondary_range(void) {
    return g_frequency_secondary_range;
}

uint8_t dmm_p10_frequency_secondary_decimal(void) {
    return p10_frequency_secondary_decimal_for_range(g_frequency_secondary_range);
}

bool dmm_p10_cal_reject_unstable_alt_secondary_band(void) {
    if (g_p10_mode != DMM_MODE_MA_AC && g_p10_mode != DMM_MODE_A_AC &&
        g_p10_mode != DMM_MODE_A_DC_PERSIST && g_p10_mode != DMM_MODE_V_AC &&
        g_p10_mode != DMM_MODE_MV_AC) {
        return false;
    }

    int32_t secondary;
    uint8_t scale;
    if (!p10_stock_secondary_for_correction(&secondary, &scale)) {
        return false;
    }

    int32_t scaled = stock_scale_secondary(scale, secondary);
    if (g_p10_mode == DMM_MODE_V_AC && g_p10_range >= 2u &&
        scaled >= 8000 && scaled < 12001) {
        return true;
    }
    if ((g_p10_mode == DMM_MODE_V_AC || g_p10_mode == DMM_MODE_MV_AC) &&
        scaled >= 90000) {
        return true;
    }
    if ((g_p10_mode == DMM_MODE_V_AC || g_p10_mode == DMM_MODE_MV_AC) &&
        scaled >= 18000 && scaled < 22001) {
        return true;
    }
    if ((g_p10_mode == DMM_MODE_MA_AC || g_p10_mode == DMM_MODE_A_AC ||
         g_p10_mode == DMM_MODE_A_DC_PERSIST) &&
        scaled >= 8000 && scaled < 12001) {
        return true;
    }
    return false;
}

int32_t dmm_p10_current_single_aux_value(void) {
    return g_current_single_aux_value;
}

bool dmm_p10_current_single_aux_overrange(void) {
    return g_current_single_aux_over_threshold;
}

int32_t dmm_p10_current_dual_bank0_value(void) {
    return g_current_bank0_value;
}

int32_t dmm_p10_current_dual_bank1_value(void) {
    return g_current_bank1_value;
}

uint8_t dmm_p10_current_alt_range0(void) {
    return g_current_alt_range0;
}

uint8_t dmm_p10_current_alt_range1(void) {
    return g_current_alt_range1;
}

bool dmm_p10_current_dual_bank_active(void) {
    return g_current_dual_bank != 0u;
}

bool dmm_p10_current_dual_display_ready(void) {
    return !dmm_mode_is_dual_current(g_p10_mode) ||
           (g_current_bank0_value != (-0x7FFFFFFF - 1) &&
            g_current_bank1_value != (-0x7FFFFFFF - 1));
}

static int32_t stock_typek_emf_from_tenths_c(int16_t tenths_c) {
    bool negative = tenths_c < 0;
    uint16_t abs_tenths = negative ? (uint16_t)(-tenths_c) : (uint16_t)tenths_c;
    if (abs_tenths > 1000u) {
        return 0;
    }

    uint8_t whole = (uint8_t)(abs_tenths / 100u);
    uint8_t frac = (uint8_t)(abs_tenths % 100u);
    if (negative) {
        if (whole > 21u) {
            whole = 21u;
            frac = 0;
        }
        uint8_t idx = (uint8_t)(21u - whole);
        if (idx == 0u) {
            return k_stock_typek_table[0];
        }
        int32_t step = (10 * (k_stock_typek_table[idx] - k_stock_typek_table[idx - 1u])) / 100;
        return k_stock_typek_table[idx] - ((int32_t)frac * step) / 10;
    }

    uint8_t idx = (uint8_t)(21u + whole);
    if ((uint32_t)idx + 1u >= STOCK_TYPEK_TABLE_COUNT) {
        idx = (uint8_t)(STOCK_TYPEK_TABLE_COUNT - 2u);
        frac = 0;
    }
    int32_t step = (10 * (k_stock_typek_table[idx + 1u] - k_stock_typek_table[idx])) / 100;
    return k_stock_typek_table[idx] + ((int32_t)frac * step) / 10;
}

static bool stock_typek_tenths_from_emf(int32_t emf, int32_t *tenths_c) {
    if (emf < -5891 || emf >= 54139) {
        return false;
    }

    uint8_t idx = 0;
    while ((uint32_t)idx < STOCK_TYPEK_TABLE_COUNT && k_stock_typek_table[idx] < emf) {
        ++idx;
    }
    if (idx == 0u || (uint32_t)idx >= STOCK_TYPEK_TABLE_COUNT) {
        return false;
    }

    int32_t prev = k_stock_typek_table[idx - 1u];
    int32_t curr = k_stock_typek_table[idx];
    int32_t denom = prev - curr;
    int32_t interp = denom != 0 ? (1000 * (prev - emf)) / denom : 0;
    int32_t centi_c = (int32_t)(1000 * (int32_t)idx - 22000) + interp;
    int32_t rounded = centi_c / 10;
    if (centi_c >= 0) {
        if ((centi_c % 10) >= 5) {
            ++rounded;
        }
    } else if ((-centi_c % 10) >= 5) {
        --rounded;
    }
    *tenths_c = rounded;
    return true;
}

void dmm_p10_set_cold_junction_temperature(int16_t tenths_c) {
    g_temperature_cold_junction_emf = stock_typek_emf_from_tenths_c(tenths_c);
}

void dmm_p10_set_burden_voltage_enabled(bool enabled) {
    if (g_current_single_aux_enabled == enabled) {
        return;
    }
    g_current_single_aux_enabled = enabled;
    p10_reset_current_single_aux_state();
    if (p10_current_single_aux_stock_mode(g_p10_mode)) {
        p10_apply_current_mode_range_config();
    }
}

void dmm_p10_set_frontend_timing_toggle_enabled(bool enabled) {
    if (g_frontend_timing_toggle_enabled == enabled) {
        return;
    }
    g_frontend_timing_toggle_enabled = enabled;
    g_p10_frontend_timing_idle_counter = 0;
    g_p10_frontend_timing_toggle_at = clock_millis() +
        (p10_mode_uses_frontend_timing_toggle() ? P10_FRONTEND_TIMING_TOGGLE_MS :
         p10_mode_uses_frequency_secondary() ? P10_FREQ_SECONDARY_REARM_MS :
         P10_FRONTEND_TIMING_TOGGLE_MS);
    if (!p10_mode_uses_frontend_timing_toggle() && g_p10_mode != DMM_MODE_LOZ) {
        g_lowz_timing_toggle_at = 0u;
    }
    if (p10_mode_uses_frontend_timing_toggle() || p10_current_single_main_stock_mode(g_p10_mode)) {
        p10_configure_current_frontend();
    }
}

static void p10_load_calibration(uint8_t mode, uint8_t range) {
    uint32_t zero = 0;
    uint8_t threshold_mode = mode;
    uint8_t threshold_range = range;
    g_current_single_extra_zero = 0;
    p10_load_stock_thresholds(threshold_mode, threshold_range);
    uint32_t base = calibration_stock_value_offset(mode, range, 0u, false,
                                                   g_voltage_alt_bank_active,
                                                   g_voltage_alt_primary_path,
                                                   g_lowz_alt_path);
    uint32_t gain = calibration_read_u32_be(base + 4u);
    uint32_t extra = calibration_read_u32_be(base + 8u);
    g_p10_gain_scale = mode == DMM_MODE_P10_CAP ? CAL_GAIN_CAP : CAL_GAIN_ONE;
    if (mode == DMM_MODE_P10_CAP && range == 4u && frontend_cap_option_table_enabled()) {
        g_p10_gain_scale = CAL_GAIN_CAP / 10u;
    }

    if (dmm_mode_is_dual_current(mode)) {
        uint32_t gain0 = gain;
        uint32_t zero1 = calibration_read_u32_be(base + 8u);
        uint32_t gain1 = calibration_read_u32_be(base + 12u);
        if (gain0 == 0u || gain0 == 0xFFFFFFFFu || gain0 < 1000u) {
            gain0 = CAL_GAIN_ONE;
        }
        if (gain1 == 0u || gain1 == 0xFFFFFFFFu || gain1 < 1000u) {
            gain1 = CAL_GAIN_ONE;
        }
        g_current_dual_zero[0] = (int32_t)calibration_read_u32_be(base);
        g_current_dual_gain[0] = gain0;
        g_current_dual_zero[1] = (int32_t)zero1;
        g_current_dual_gain[1] = gain1;
        g_p10_zero_offset = g_current_dual_zero[g_current_dual_bank != 0u ? 1u : 0u];
        g_p10_gain = g_current_dual_gain[g_current_dual_bank != 0u ? 1u : 0u];
        g_p10_extra_comp = 0;
        return;
    }

    if (mode >= DMM_MODE_MA_AC && mode <= DMM_MODE_A_AC_PERSIST && range == 0u) {
        uint16_t extra_zero_addr = 0;
        if (mode == DMM_MODE_MA_AC) {
            extra_zero_addr = 0x04D0u;
        } else if (mode == DMM_MODE_MA_DC) {
            extra_zero_addr = 0x04D8u;
        } else if (mode == DMM_MODE_A_AC) {
            extra_zero_addr = 0x04E0u;
        } else if (mode == DMM_MODE_A_DC) {
            extra_zero_addr = 0x04E8u;
        } else if (mode == DMM_MODE_A_DC_PERSIST) {
            extra_zero_addr = 0x04F0u;
        } else if (mode == DMM_MODE_A_AC_PERSIST) {
            extra_zero_addr = 0x04F8u;
        }
        if (extra_zero_addr != 0u && calibration_read_u8(extra_zero_addr) == 0xA5u) {
            g_current_single_extra_zero = (int32_t)calibration_read_u32_be(extra_zero_addr + 1u);
        }
    }

    if (mode != DMM_MODE_P10_DIODE) {
        zero = calibration_read_u32_be(base);
        if (zero == 0xFFFFFFFFu) {
            zero = 0;
        }
    }

    if (gain == 0u || gain == 0xFFFFFFFFu || gain < 1000u) {
        gain = g_p10_gain_scale;
    }

    g_p10_zero_offset = (int32_t)zero;
    g_p10_gain = gain;
    if (mode == DMM_MODE_P10_OHMS && range == 6u && extra != 0xFFFFFFFFu) {
        g_p10_extra_comp = (int32_t)extra;
    } else {
        g_p10_extra_comp = 0;
    }
}

static int32_t p10_apply_calibration(int32_t raw_sample) {
    int32_t corrected = raw_sample;
    if (g_p10_mode != DMM_MODE_P10_DIODE) {
        if (dmm_mode_is_dual_current(g_p10_mode)) {
            uint8_t bank = g_current_dual_bank != 0u ? 1u : 0u;
            corrected -= g_current_dual_zero[bank];
            return apply_gain_fixed(corrected, g_current_dual_gain[bank]);
        }
        corrected -= g_p10_zero_offset;
    }

    corrected = apply_gain_fixed(corrected, g_p10_gain);
    if ((g_p10_mode == DMM_MODE_P10_OHMS || g_p10_mode == DMM_MODE_P10_CONT) &&
        corrected < 0) {
        corrected = 0;
    }
    return corrected;
}

static int32_t p10_apply_ac_voltage_calibration(int32_t raw_sample) {
    int32_t corrected = raw_sample - g_p10_zero_offset;
    if (corrected < 1) {
        corrected = 0;
    }
    return apply_gain_fixed(corrected, g_p10_gain);
}

static uint32_t p10_cap_gain_scale_for_range(uint8_t range) {
    if (range == 4u && frontend_cap_option_table_enabled()) {
        return CAL_GAIN_CAP / 10u;
    }
    return CAL_GAIN_CAP;
}

static int32_t p10_apply_cap_calibration(uint32_t raw_value) {
    uint32_t corrected = raw_value;
    if (g_p10_range < 2u) {
        uint32_t zero = (uint32_t)g_p10_zero_offset;
        corrected = corrected > zero ? corrected - zero : 0u;
    }
    if (corrected > 0x7FFFFFFFu) {
        corrected = 0x7FFFFFFFu;
    }

    /*
     * Stock converts the stored CAP gain integer to float, divides it by
     * 100000 (or by 10000 for the special range-4 option table), then divides
     * the corrected counter ratio by that gain factor.
     */
    uint32_t scale = p10_cap_gain_scale_for_range(g_p10_range);
    uint32_t gain = g_p10_gain;
    if (gain == 0u || gain == 0xFFFFFFFFu || gain < 1000u) {
        gain = scale;
    }
    int32_t value = (int32_t)div_u64_u32((uint64_t)corrected * scale, gain);
    if (value < 0) {
        value = 0;
    }
    return value;
}

static uint32_t p10_cap_raw_from_counters(uint32_t cap_count, uint32_t cap_ref) {
    uint64_t numerator = cap_count;
    if (g_p10_range >= 2u && g_p10_range != 5u) {
        numerator *= 100u;
    }
    return div_u64_u32(numerator, cap_ref);
}

static uint32_t p10_freq_reference_hz(void) {
    uint32_t ref = calibration_read_u32_be(0x0104u);
    if (ref < 40000000u || ref > 60000000u || ref == 0xFFFFFFFFu) {
        ref = 49152050u;
    }
    return ref;
}

uint32_t dmm_p10_freq_reference_hz(void) {
    return p10_freq_reference_hz();
}

static uint32_t p10_counter_rate_per_second(uint32_t cap_count, uint32_t elapsed) {
    uint32_t elapsed_ms = elapsed / 1000u;
    if (elapsed_ms == 0u) {
        return 0xFFFFFFFFu;
    }
    return cap_count / elapsed_ms;
}

static uint32_t p10_freq_raw_hz(uint32_t cap_ref, uint32_t elapsed) {
    if (elapsed == 0u) {
        return 0xFFFFFFFFu;
    }
    return div_u64_u32((uint64_t)p10_freq_reference_hz() * 100u * cap_ref, elapsed);
}

static uint32_t p10_period_raw_us(uint32_t cap_count, uint32_t cap_ref, uint32_t elapsed) {
    if (cap_ref == 0u) {
        return 0xFFFFFFFFu;
    }
    uint32_t ref_khz = p10_freq_reference_hz() / 1000u;
    uint64_t denom = (uint64_t)ref_khz * cap_ref;
    if (denom == 0u || denom > 0xFFFFFFFFu) {
        return 0xFFFFFFFFu;
    }
    uint32_t ratio = div_u64_u32((uint64_t)100000u * elapsed, (uint32_t)denom);
    uint32_t rate = p10_counter_rate_per_second(cap_count, elapsed);
    if (rate == 0xFFFFFFFFu) {
        return rate;
    }
    uint64_t scaled = div_u64_u32((uint64_t)rate * ratio, 1000u);
    return scaled > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)scaled;
}

static int32_t p10_freq_scale_value(uint32_t raw, uint8_t *range, bool *overrange) {
    uint8_t selected = *range;
    *overrange = false;
    if (g_p10_autorange) {
        if (raw >= 1000000000u) {
            selected = 4u;
            *overrange = true;
        } else if (raw >= 100000000u || (*range == 4u && raw >= 80000000u)) {
            selected = 4u;
        } else if (raw >= 10000000u || (*range == 3u && raw >= 8000000u)) {
            selected = 3u;
        } else if (raw >= 1000000u || (*range == 2u && raw >= 800000u)) {
            selected = 2u;
        } else if (raw >= 100000u || (*range == 1u && raw >= 80000u)) {
            selected = 1u;
        } else {
            selected = 0u;
        }
    } else {
        if ((*range == 0u && raw >= 100000u) ||
            (*range == 1u && raw >= 1000000u) ||
            (*range == 2u && raw >= 10000000u) ||
            (*range == 3u && raw >= 100000000u) ||
            (*range >= 4u && raw >= 1000000000u)) {
            *overrange = true;
        }
    }

    *range = selected;
    if (*overrange) {
        return 0;
    }
    if (selected == 4u) {
        return (int32_t)div_u64_u32((uint64_t)raw + 5000u, 10000u);
    }
    if (selected == 3u) {
        return (int32_t)div_u64_u32((uint64_t)raw + 500u, 1000u);
    }
    if (selected == 2u) {
        return (int32_t)div_u64_u32((uint64_t)raw + 50u, 100u);
    }
    if (selected == 1u) {
        return (int32_t)div_u64_u32((uint64_t)raw + 5u, 10u);
    }
    return raw > 0x7FFFFFFFu ? 0x7FFFFFFF : (int32_t)raw;
}

static int32_t p10_freq_scale_auto_value(uint32_t raw, uint8_t *range, bool *overrange) {
    uint8_t selected = *range;
    *overrange = false;
    if (raw >= 1000000000u) {
        selected = 4u;
        *overrange = true;
    } else if (raw >= 100000000u || (*range == 4u && raw >= 80000000u)) {
        selected = 4u;
    } else if (raw >= 10000000u || (*range == 3u && raw >= 8000000u)) {
        selected = 3u;
    } else if (raw >= 1000000u || (*range == 2u && raw >= 800000u)) {
        selected = 2u;
    } else if (raw >= 100000u || (*range == 1u && raw >= 80000u)) {
        selected = 1u;
    } else {
        selected = 0u;
    }

    *range = selected;
    if (*overrange) {
        return 0;
    }
    if (selected == 4u) {
        return (int32_t)div_u64_u32((uint64_t)raw + 5000u, 10000u);
    }
    if (selected == 3u) {
        return (int32_t)div_u64_u32((uint64_t)raw + 500u, 1000u);
    }
    if (selected == 2u) {
        return (int32_t)div_u64_u32((uint64_t)raw + 50u, 100u);
    }
    if (selected == 1u) {
        return (int32_t)div_u64_u32((uint64_t)raw + 5u, 10u);
    }
    return raw > 0x7FFFFFFFu ? 0x7FFFFFFF : (int32_t)raw;
}

static bool p10_mode_uses_frequency_secondary(void) {
    return (g_p10_mode == DMM_MODE_V_AC && !g_voltage_alt_bank_active) ||
           g_p10_mode == DMM_MODE_MV_AC;
}

static uint16_t p10_default_frontend_timing_word(void) {
    if (dmm_mode_is_freq_group(g_p10_mode)) {
        return P10_FAST_TOGGLE_TIMING_WORD;
    }
    if (g_p10_mode == DMM_MODE_P10_OHMS &&
        g_p10_task_fast_range_enabled &&
        p10_ohms_stock_logic_enabled() &&
        g_p10_autorange) {
        return P10_FAST_SEARCH_TIMING_WORD;
    }
    return P10_STOCK_TIMING_WORD;
}

static bool p10_mode_forces_frontend_timing_default(void) {
    if (g_p10_frontend_timing_word == P10_FAST_SEARCH_TIMING_WORD) {
        return true;
    }
    return g_p10_mode == DMM_MODE_P10_CONT ||
           g_p10_mode == DMM_MODE_P10_OHMS ||
           g_p10_mode == DMM_MODE_P10_DIODE ||
           g_p10_mode == DMM_MODE_P10_CAP ||
           dmm_mode_is_freq_group(g_p10_mode);
}

static void p10_reset_frontend_timing_state_for_mode(void) {
    g_p10_frontend_timing_idle_counter = 0;
    g_p10_frontend_timing_toggle_at = 0u;
    if (g_p10_mode == DMM_MODE_V_DC || g_p10_mode == DMM_MODE_V_AC) {
        g_p10_frontend_timing_word = P10_STOCK_TIMING_WORD;
        g_p10_frontend_timing_phase = false;
        return;
    }
    if (!p10_mode_forces_frontend_timing_default()) {
        return;
    }

    g_p10_frontend_timing_word = p10_default_frontend_timing_word();
    g_p10_frontend_timing_phase =
        g_p10_frontend_timing_word == P10_FAST_TOGGLE_TIMING_WORD;
}

static void p10_frequency_secondary_mark_frontend_activity(bool restore_stock_timing) {
    if (!p10_mode_uses_frequency_secondary()) {
        return;
    }
    g_p10_frontend_timing_idle_counter = 0;
    if (!p10_mode_uses_frontend_timing_toggle()) {
        g_p10_frontend_timing_toggle_at = clock_millis() + P10_FRONTEND_TIMING_TOGGLE_MS;
    }
    if (restore_stock_timing && g_p10_frontend_timing_word != P10_STOCK_TIMING_WORD) {
        g_p10_frontend_timing_phase = false;
        g_p10_frontend_timing_word = P10_STOCK_TIMING_WORD;
        p10_configure_current_frontend();
    }
}

static uint8_t p10_frequency_secondary_decimal_for_range(uint8_t range) {
    if (range == 0u) {
        return 2u;
    }
    if (range == 1u || range == 4u) {
        return 1u;
    }
    if (range == 2u) {
        return 3u;
    }
    if (range == 3u) {
        return 2u;
    }
    return 0u;
}

static void p10_frequency_secondary_timeout_service(void) {
    if (!p10_mode_uses_frequency_secondary()) {
        g_frequency_secondary_valid = false;
        g_frequency_secondary_raw = 0;
        g_frequency_secondary_value = 0;
        g_frequency_secondary_range = 0;
        g_frequency_secondary_overrange = false;
        g_frequency_secondary_synced = false;
        g_frequency_secondary_last_event_ms = 0;
        g_frequency_secondary_last_count = 0;
        g_frequency_secondary_last_ref = 0;
        g_frequency_secondary_last_wrap = 0;
        return;
    }
    if (g_frequency_secondary_valid &&
        (int32_t)(clock_millis() - (g_frequency_secondary_last_event_ms + 2625u)) >= 0) {
        g_frequency_secondary_valid = false;
        g_frequency_secondary_raw = 0;
        g_frequency_secondary_value = 0;
        g_frequency_secondary_range = 0;
        g_frequency_secondary_overrange = false;
    }
}

static bool p10_decode_frequency_secondary(bool restart_conversion, bool require_changed) {
    if (!p10_mode_uses_frequency_secondary()) {
        return false;
    }

    uint32_t cap_count = frontend_decode_u24_le_at(g_frontend_status, 0x15u);
    uint32_t cap_ref = frontend_decode_u24_le_at(g_frontend_status, 0x18u);
    uint32_t cap_wrap = frontend_decode_u24_le_at(g_frontend_status, 0x1Bu);
    if (cap_ref == 0u) {
        return false;
    }
    bool changed = cap_count != g_frequency_secondary_last_count ||
                   cap_ref != g_frequency_secondary_last_ref ||
                   cap_wrap != g_frequency_secondary_last_wrap;
    if (require_changed && !changed) {
        return false;
    }
    g_frequency_secondary_last_count = cap_count;
    g_frequency_secondary_last_ref = cap_ref;
    g_frequency_secondary_last_wrap = cap_wrap;

    uint16_t timing_word = g_p10_frontend_timing_word;
    uint32_t cap_timing_base = 0x1000000u - ((uint32_t)timing_word << 8);
    uint32_t elapsed = cap_timing_base + cap_wrap;

    ++g_p10_diag.cap_events;
    g_p10_diag.flags |= 0x02u;
    g_p10_diag.cap_count_lsb = (uint8_t)cap_count;
    g_p10_diag.cap_ref_lsb = (uint8_t)cap_ref;
    g_p10_diag.cap_count = cap_count;
    g_p10_diag.cap_ref = cap_ref;
    g_p10_diag.cap_wrap = cap_wrap;
    g_p10_diag.cap_elapsed = elapsed;
    g_p10_diag.cap_timing_word = timing_word;
    g_frequency_secondary_synced = true;
    p10_frequency_secondary_mark_frontend_activity(false);

    if (restart_conversion) {
        g_p10_cap_timing_state = P10_CAP_TIMING_EVENT_RESTART;
        frontend_restart_p10_conversion(timing_word);
    }

    uint8_t range = g_frequency_secondary_range;
    bool overrange = false;
    uint32_t raw = p10_freq_raw_hz(cap_ref, elapsed);
    if (raw == 0xFFFFFFFFu) {
        return false;
    }
    int32_t value = p10_freq_scale_auto_value(raw, &range, &overrange);
    g_frequency_secondary_raw = raw;
    g_frequency_secondary_value = value;
    g_frequency_secondary_range = range;
    g_frequency_secondary_overrange = overrange;
    g_frequency_secondary_valid = true;
    g_frequency_secondary_last_event_ms = clock_millis();
    return true;
}

static int32_t p10_period_scale_value(uint32_t raw, uint8_t *range, bool *overrange) {
    uint8_t selected = *range;
    *overrange = false;
    if (g_p10_autorange) {
        if (raw >= 10000000u) {
            selected = 2u;
            *overrange = true;
        } else if (raw >= 1000000u || (*range == 2u && raw >= 800000u)) {
            selected = 2u;
        } else if (raw >= 100000u || (*range == 1u && raw >= 80000u)) {
            selected = 1u;
        } else {
            selected = 0u;
        }
    } else {
        if ((*range == 0u && raw >= 100000u) ||
            (*range == 1u && raw >= 1000000u) ||
            (*range >= 2u && raw >= 10000000u)) {
            *overrange = true;
        }
    }

    *range = selected;
    if (*overrange) {
        return 0;
    }
    if (selected == 2u) {
        return (int32_t)div_u64_u32(raw, 100u);
    }
    if (selected == 1u) {
        return (int32_t)div_u64_u32(raw, 10u);
    }
    return raw > 0x7FFFFFFFu ? 0x7FFFFFFF : (int32_t)raw;
}

static bool p10_decode_freq_group_value(dmm_reading_t *reading, int32_t *value) {
    uint32_t cap_count = frontend_decode_u24_le_at(g_frontend_status, 0x15u);
    uint32_t cap_ref = frontend_decode_u24_le_at(g_frontend_status, 0x18u);
    uint32_t cap_wrap = frontend_decode_u24_le_at(g_frontend_status, 0x1Bu);
    uint16_t timing_word = g_p10_frontend_timing_word;
    uint32_t cap_timing_base = 0x1000000u - ((uint32_t)timing_word << 8);
    uint32_t elapsed = cap_timing_base + cap_wrap;

    ++g_p10_diag.cap_events;
    g_p10_diag.flags |= 0x02u;
    g_p10_diag.cap_count_lsb = (uint8_t)cap_count;
    g_p10_diag.cap_ref_lsb = (uint8_t)cap_ref;
    g_p10_diag.cap_count = cap_count;
    g_p10_diag.cap_ref = cap_ref;
    g_p10_diag.cap_wrap = cap_wrap;
    g_p10_diag.cap_elapsed = elapsed;
    g_p10_diag.cap_timing_word = timing_word;

    g_p10_cap_timing_state = P10_CAP_TIMING_EVENT_RESTART;
    frontend_restart_p10_conversion(timing_word);

    uint8_t range = g_p10_range;
    bool overrange = false;
    uint32_t raw = 0;
    if (g_p10_mode == DMM_MODE_FREQ) {
        raw = p10_freq_raw_hz(cap_ref, elapsed);
        *value = p10_freq_scale_value(raw, &range, &overrange);
    } else if (g_p10_mode == DMM_MODE_PERIOD) {
        raw = p10_period_raw_us(cap_count, cap_ref, elapsed);
        *value = p10_period_scale_value(raw, &range, &overrange);
    } else {
        raw = p10_counter_rate_per_second(cap_count, elapsed);
        *value = raw > 0x7FFFFFFFu ? 0x7FFFFFFF : (int32_t)raw;
        range = 0u;
    }

    g_p10_diag.cap_raw = raw;
    g_p10_diag.last_value = *value;
    g_p10_overrange_latched = overrange;
    g_p10_range = range;
    reading->range = range;
    reading->overrange = overrange;
    return raw != 0xFFFFFFFFu;
}

static bool p10_decode_cap_value(int32_t *value) {
    uint32_t cap_count = frontend_decode_u24_le_at(g_frontend_status, 0x15u);
    uint32_t cap_ref = frontend_decode_u24_le_at(g_frontend_status, 0x18u);
    uint32_t cap_wrap = frontend_decode_u24_le_at(g_frontend_status, 0x1Bu);
    uint16_t timing_word = g_p10_frontend_timing_word;
    uint32_t cap_timing_base = 0x1000000u - ((uint32_t)timing_word << 8);
    ++g_p10_diag.cap_events;
    g_p10_diag.flags |= 0x02u;
    g_p10_diag.cap_count_lsb = (uint8_t)cap_count;
    g_p10_diag.cap_ref_lsb = (uint8_t)cap_ref;
    g_p10_diag.cap_count = cap_count;
    g_p10_diag.cap_ref = cap_ref;
    g_p10_diag.cap_wrap = cap_wrap;
    g_p10_diag.cap_elapsed = cap_timing_base + cap_wrap;
    g_p10_diag.cap_timing_word = timing_word;

    /*
     * Stock sub_800AB48 restarts CAP timing as part of the bit-0 event
     * handler before sub_800B33C divides the counters. Keep the conversion
     * moving even if the current reference counter is not usable yet.
     */
    g_p10_cap_timing_state = P10_CAP_TIMING_EVENT_RESTART;
    frontend_restart_p10_conversion(timing_word);
    p10_schedule_cap_timeout();

    if (cap_ref == 0u) {
        return false;
    }

    uint32_t ratio = p10_cap_raw_from_counters(cap_count, cap_ref);
    *value = p10_apply_cap_calibration(ratio);
    g_p10_diag.cap_raw = (uint32_t)*value;
    g_p10_diag.last_value = *value;
    return true;
}

static int32_t p10_mode9_range5_comp(int32_t value) {
    if (value >= 50000 || value < 1000) {
        return 0;
    }
    if (value < 15000) {
        return value / 1000;
    }
    if (value < 36000) {
        return 15;
    }
    if (value >= 49000) {
        return 1;
    }
    return 50 - (value / 1000);
}

static int32_t p10_mode9_range6_comp(int32_t value, int32_t extra) {
    if (value >= 56000 || value <= 0 || extra == 0) {
        return 0;
    }

    uint8_t index = 1u;
    while (index < 13u && (int32_t)k_mode9_range6_x_kohm[index] * 1000 < value) {
        ++index;
    }

    int32_t x0 = (int32_t)k_mode9_range6_x_kohm[index - 1u] * 1000;
    int32_t x1 = (int32_t)k_mode9_range6_x_kohm[index] * 1000;
    int32_t y0 = k_mode9_range6_y_percent[index - 1u];
    int32_t y1 = k_mode9_range6_y_percent[index];
    int32_t y = y0;
    if (x1 != x0) {
        y += ((value - x0) * (y1 - y0)) / (x1 - x0);
    }
    return (extra * y) / 100;
}

static void p10_smoothing_params(uint16_t *deadband, uint8_t *count) {
    measurement_profile_smoothing(g_p10_mode, g_p10_range, deadband, count);
}

static int32_t p10_smooth_with_params(int32_t value, uint16_t deadband, uint8_t max_count) {
    if (deadband == 0u || max_count == 0u) {
        return value;
    }

    int32_t delta = value - g_p10_filtered_value;
    if (delta < 0) {
        delta = -delta;
    }
    if (g_p10_filter_count != 0u && delta > deadband) {
        g_p10_filter_count = 0;
    }

    if (g_p10_filter_count < max_count) {
        ++g_p10_filter_count;
    }
    if (g_p10_filter_count > (uint8_t)(sizeof(g_p10_filter_samples) / sizeof(g_p10_filter_samples[0]))) {
        g_p10_filter_count = (uint8_t)(sizeof(g_p10_filter_samples) / sizeof(g_p10_filter_samples[0]));
    }

    for (uint8_t i = (uint8_t)(g_p10_filter_count - 1u); i > 0u; --i) {
        g_p10_filter_samples[i] = g_p10_filter_samples[i - 1u];
    }
    g_p10_filter_samples[0] = value;

    int32_t sum = 0;
    for (uint8_t i = 0; i < g_p10_filter_count; ++i) {
        sum += g_p10_filter_samples[i];
    }
    g_p10_filtered_value = sum / (int32_t)g_p10_filter_count;
    return g_p10_filtered_value;
}

static int32_t p10_smooth_value(int32_t value) {
    uint16_t deadband;
    uint8_t max_count;
    p10_smoothing_params(&deadband, &max_count);
    return p10_smooth_with_params(value, deadband, max_count);
}

static int32_t p10_smooth_cap_value(int32_t value) {
    uint32_t deadband = (uint32_t)value / (g_p10_range == 5u ? 10u : 50u);
    if (deadband < 20u) {
        deadband = 20u;
    }
    if (deadband > 0xFFFFu) {
        deadband = 0xFFFFu;
    }
    return p10_smooth_with_params(value, (uint16_t)deadband, 10u);
}

static int32_t p10_apply_diode_stock_display_offset(int32_t value) {
    if (g_p10_mode == DMM_MODE_P10_DIODE && g_p10_diode_option_15 &&
        frontend_cap_option_table_enabled() && value >= 14001) {
        return value > (0x7FFFFFFF - 500) ? 0x7FFFFFFF : value + 500;
    }
    return value;
}

static int32_t p10_condition_display_value(int32_t value) {
    if (g_p10_mode == DMM_MODE_P10_CAP) {
        int32_t conditioned = p10_smooth_cap_value(value);
        if (g_p10_range >= 2u && g_p10_range < 5u && conditioned < 21) {
            conditioned = 0;
        }
        return conditioned;
    }

    int32_t conditioned = p10_smooth_value(value);
    if (g_p10_mode == DMM_MODE_P10_DIODE) {
        return p10_apply_diode_stock_display_offset(conditioned);
    }
    if (g_p10_mode == DMM_MODE_P10_OHMS) {
        if (g_p10_range == 5u) {
            conditioned -= p10_mode9_range5_comp(conditioned);
        } else if (g_p10_range == 6u) {
            conditioned -= p10_mode9_range6_comp(conditioned, g_p10_extra_comp);
        }
        if (conditioned < 0) {
            conditioned = 0;
        }
    }
    return conditioned;
}

static void p10_reset_average(void) {
    g_p10_display_accum = 0;
    g_p10_display_count = 0;
}

static uint8_t p10_autorange_holdoff_count(uint8_t mode, uint8_t range) {
    if (mode == DMM_MODE_P10_OHMS) {
        return range >= 5u ? 2u : 1u;
    }
    return 0u;
}

static void p10_reset_filter(void) {
    g_p10_filter_count = 0;
    g_p10_filtered_value = 0;
    for (uint8_t i = 0; i < (uint8_t)(sizeof(g_p10_filter_samples) / sizeof(g_p10_filter_samples[0])); ++i) {
        g_p10_filter_samples[i] = 0;
    }
}

static void p10_reset_current_bank_filters(void) {
    for (uint8_t bank = 0; bank < 2u; ++bank) {
        g_current_bank_filter_count[bank] = 0;
        g_current_bank_filtered_value[bank] = 0;
        for (uint8_t i = 0; i < 10u; ++i) {
            g_current_bank_filter_samples[bank][i] = 0;
        }
    }
}

static int32_t p10_smooth_current_bank(uint8_t bank, int32_t value,
                                       uint16_t deadband, uint8_t max_count) {
    if (bank >= 2u || deadband == 0u || max_count == 0u) {
        return value;
    }
    if (max_count > 10u) {
        max_count = 10u;
    }

    int32_t delta = value - g_current_bank_filtered_value[bank];
    if (delta < 0) {
        delta = -delta;
    }
    if (g_current_bank_filter_count[bank] != 0u && delta > deadband) {
        g_current_bank_filter_count[bank] = 0;
    }

    if (g_current_bank_filter_count[bank] < max_count) {
        ++g_current_bank_filter_count[bank];
    }

    for (uint8_t i = (uint8_t)(g_current_bank_filter_count[bank] - 1u); i > 0u; --i) {
        g_current_bank_filter_samples[bank][i] = g_current_bank_filter_samples[bank][i - 1u];
    }
    g_current_bank_filter_samples[bank][0] = value;

    int32_t sum = 0;
    for (uint8_t i = 0; i < g_current_bank_filter_count[bank]; ++i) {
        sum += g_current_bank_filter_samples[bank][i];
    }
    g_current_bank_filtered_value[bank] = sum / (int32_t)g_current_bank_filter_count[bank];
    return g_current_bank_filtered_value[bank];
}

static bool p10_current_mode_uses_range_pair(uint8_t mode) {
    return mode == DMM_MODE_MA_AUX || mode == DMM_MODE_LEGACY_CURRENT_AC ||
           mode == DMM_MODE_MVA_AC || mode == DMM_MODE_MA_BURDEN ||
           mode == DMM_MODE_MA_ALT_AC || mode == DMM_MODE_MVA_DC;
}

static bool p10_current_mode_uses_dc_alt1_range2(uint8_t mode) {
    return mode == DMM_MODE_MVA_AC || mode == DMM_MODE_MVA_DC;
}

static bool p10_current_single_aux_stock_mode(uint8_t mode) {
    return mode == DMM_MODE_MA_AC || mode == DMM_MODE_MA_DC;
}

static bool p10_current_single_main_stock_mode(uint8_t mode) {
    return mode == DMM_MODE_MA_AC || mode == DMM_MODE_MA_DC ||
           mode == DMM_MODE_A_AC || mode == DMM_MODE_A_DC ||
           mode == DMM_MODE_A_DC_PERSIST || mode == DMM_MODE_A_AC_PERSIST;
}

static bool p10_current_single_ac_main_mode(uint8_t mode) {
    return mode == DMM_MODE_MA_AC || mode == DMM_MODE_A_AC ||
           mode == DMM_MODE_A_DC_PERSIST;
}

static uint8_t p10_current_single_aux_mode(uint8_t mode) {
    return mode == DMM_MODE_MA_AC ? DMM_MODE_V_AC : DMM_MODE_V_DC;
}

static uint8_t p10_current_single_switch_threshold(uint8_t mode) {
    return mode == DMM_MODE_MA_AC ? 6u : 3u;
}

static void p10_current_apply_range_pair(uint8_t mode, uint8_t range) {
    if (!p10_current_mode_uses_range_pair(mode)) {
        return;
    }

    if (range >= 4u) {
        range = 3u;
    }
    g_current_alt_range0 = (range & 0x01u) != 0u ? 1u : 0u;
    g_current_alt_range1 = (range & 0x02u) != 0u ? 1u : 0u;
    if (p10_current_mode_uses_dc_alt1_range2(mode) && g_current_alt_range1 != 0u) {
        g_current_alt_range1 = 2u;
    }
}

static uint8_t p10_current_range_from_pair(uint8_t mode) {
    if (!p10_current_mode_uses_range_pair(mode)) {
        return g_p10_range;
    }

    uint8_t range = g_current_alt_range0 != 0u ? 1u : 0u;
    if (g_current_alt_range1 != 0u) {
        range |= 2u;
    }
    return range;
}

static void p10_reset_current_dual_state(void) {
    g_current_dual_bank_active = dmm_mode_is_dual_current(g_p10_mode);
    g_current_dual_bank = 0;
    g_current_dual_counter = 0;
    g_current_alt_range0 = 0;
    g_current_alt_range1 = 0;
    g_current_bank0_value = (-0x7FFFFFFF - 1);
    g_current_bank1_value = (-0x7FFFFFFF - 1);
    p10_reset_current_bank_filters();
    p10_current_apply_range_pair(g_p10_mode, g_p10_range);
}

static void p10_reset_current_single_aux_state(void) {
    g_current_single_aux_active = false;
    g_current_single_counter = 0;
    g_current_single_aux_value = 0;
    g_current_single_jitter_count = 0;
    g_current_single_jitter_sum = 0;
    g_current_single_jitter_value = 0;
    g_current_single_aux_over_threshold = false;
}

static void p10_reset_voltage_alt_state(void) {
    g_voltage_alt_primary_path = false;
    g_voltage_alt_counter = 0;
    g_voltage_alt_primary_value = 0;
    g_voltage_alt_secondary_value = 0;
}

static uint8_t p10_settle_count(uint8_t mode) {
    if (mode == DMM_MODE_V_AC) {
        return g_voltage_alt_bank_active ? 1u : 3u;
    }
    return measurement_profile_settle_count(mode, g_p10_range);
}

static uint8_t p10_average_limit(void) {
    return measurement_profile_average_count(g_p10_mode, g_p10_range);
}

static uint32_t p10_cap_timeout_ms(void) {
    uint16_t ticks;
    if (frontend_cap_option_table_enabled()) {
        if (g_p10_range == 0u) {
            ticks = 200u;
            return ((uint32_t)ticks * STOCK_TIM2_TICK_US + 999u) / 1000u;
        }
        if (g_p10_range == 1u) {
            ticks = g_p10_autorange ? 150u : 300u;
            return ((uint32_t)ticks * STOCK_TIM2_TICK_US + 999u) / 1000u;
        }
    } else if (g_p10_range <= 1u) {
        ticks = g_p10_autorange ? 200u : 300u;
        return ((uint32_t)ticks * STOCK_TIM2_TICK_US + 999u) / 1000u;
    }

    ticks = g_p10_range == 5u ? 300u : 200u;
    return ((uint32_t)ticks * STOCK_TIM2_TICK_US + 999u) / 1000u;
}

static uint16_t p10_cap_timeout_stock_ticks(void) {
    if (frontend_cap_option_table_enabled()) {
        if (g_p10_range == 0u) {
            return 200u;
        }
        if (g_p10_range == 1u) {
            return g_p10_autorange ? 150u : 300u;
        }
    } else if (g_p10_range <= 1u) {
        return g_p10_autorange ? 200u : 300u;
    }

    return g_p10_range == 5u ? 300u : 200u;
}

static bool p10_mode_uses_frontend_timing_toggle(void) {
    if (!g_frontend_timing_toggle_enabled) {
        return false;
    }
    if (g_voltage_alt_bank_active) {
        return false;
    }
    return g_p10_mode == DMM_MODE_MV_AC ||
           g_p10_mode == DMM_MODE_MA_AC ||
           g_p10_mode == DMM_MODE_A_AC ||
           g_p10_mode == DMM_MODE_A_DC_PERSIST;
}

static void p10_frontend_timing_idle_service(void) {
    if (g_p10_mode == DMM_MODE_LOZ && g_lowz_alt_path) {
        uint32_t now = clock_millis();
        if (g_lowz_timing_toggle_at == 0u) {
            g_lowz_timing_toggle_at = now + P10_FRONTEND_TIMING_LOZ_TOGGLE_MS;
        }
        if ((int32_t)(now - g_lowz_timing_toggle_at) >= 0) {
            g_lowz_timing_toggle_at = now + P10_FRONTEND_TIMING_LOZ_TOGGLE_MS;
            g_p10_frontend_timing_phase = !g_p10_frontend_timing_phase;
            g_p10_frontend_timing_word =
                g_p10_frontend_timing_phase ? P10_FAST_TOGGLE_TIMING_WORD : P10_STOCK_TIMING_WORD;
            p10_configure_current_frontend();
        }
        return;
    }

    if (p10_mode_uses_frontend_timing_toggle()) {
        uint32_t now = clock_millis();
        if (g_p10_frontend_timing_toggle_at == 0u) {
            g_p10_frontend_timing_toggle_at = now + P10_FRONTEND_TIMING_TOGGLE_MS;
            return;
        }
        if ((int32_t)(now - g_p10_frontend_timing_toggle_at) < 0) {
            return;
        }
        g_p10_frontend_timing_idle_counter = 0;
        g_p10_frontend_timing_toggle_at = now + P10_FRONTEND_TIMING_TOGGLE_MS;
        g_p10_frontend_timing_phase = !g_p10_frontend_timing_phase;
        g_p10_frontend_timing_word =
            g_p10_frontend_timing_phase ? P10_FAST_TOGGLE_TIMING_WORD : P10_STOCK_TIMING_WORD;
        p10_configure_current_frontend();
        return;
    }

    if (p10_mode_uses_frequency_secondary()) {
        uint32_t now = clock_millis();
        if (g_p10_frontend_timing_toggle_at == 0u) {
            g_p10_frontend_timing_toggle_at = now + P10_FREQ_SECONDARY_REARM_MS;
            return;
        }
        if ((int32_t)(now - g_p10_frontend_timing_toggle_at) < 0) {
            return;
        }
        g_p10_frontend_timing_idle_counter = 0;
        g_p10_frontend_timing_toggle_at = now + P10_FREQ_SECONDARY_REARM_MS;
        g_p10_frontend_timing_phase = false;
        g_p10_frontend_timing_word = P10_STOCK_TIMING_WORD;
        frontend_restart_p10_conversion(P10_STOCK_TIMING_WORD);
        return;
    }

    if (!p10_mode_uses_frontend_timing_toggle()) {
        g_p10_frontend_timing_idle_counter = 0;
        g_p10_frontend_timing_toggle_at = 0u;
        g_lowz_timing_toggle_at = 0u;
        return;
    }
}

static uint32_t p10_cap_timeout_tick_ms(void) {
    return (STOCK_TIM2_TICK_US + 999u) / 1000u;
}

static void p10_cap_timing_stop(void) {
    g_p10_cap_timeout_active = false;
    g_p10_cap_timing_state = P10_CAP_TIMING_OFF;
    g_p10_cap_timeout_counter = 0;
    g_p10_cap_next_tick_at = 0;
    g_p10_diag.cap_timeout_ticks = 0;
    g_p10_diag.cap_timeout_counter = 0;
}

static void p10_schedule_cap_timeout(void) {
    if (g_p10_mode == DMM_MODE_P10_CAP && g_p10_cap_timeout_enabled) {
        uint32_t now = clock_millis();
        g_p10_cap_timeout_at = now + p10_cap_timeout_ms();
        g_p10_cap_timeout_active = true;
        g_p10_cap_timing_state = P10_CAP_TIMING_ARMED;
        g_p10_cap_timeout_counter = 0;
        g_p10_cap_next_tick_at = now + p10_cap_timeout_tick_ms();
        g_p10_diag.cap_timeout_ticks = p10_cap_timeout_stock_ticks();
        g_p10_diag.cap_timeout_counter = 0;
    } else {
        p10_cap_timing_stop();
    }
}

static void p10_cap_timing_service(void) {
    if (g_p10_mode != DMM_MODE_P10_CAP || !g_p10_cap_timeout_active ||
        g_p10_cap_timing_state == P10_CAP_TIMING_TIMEOUT_PENDING) {
        return;
    }
    if (g_p10_cap_frontend_sync_pending) {
        g_p10_cap_timeout_counter = 0;
        g_p10_diag.cap_timeout_counter = 0;
        return;
    }

    uint32_t now = clock_millis();
    uint32_t timeout_ms = p10_cap_timeout_ms();
    uint32_t start_ms = g_p10_cap_timeout_at - timeout_ms;
    uint32_t elapsed_ms = now - start_ms;
    uint32_t elapsed_ticks = (elapsed_ms * 1000u) / STOCK_TIM2_TICK_US;
    if (elapsed_ticks > 0xFFFFu) {
        elapsed_ticks = 0xFFFFu;
    }
    g_p10_cap_timeout_counter = (uint16_t)elapsed_ticks;
    g_p10_diag.cap_timeout_counter = g_p10_cap_timeout_counter;
    if ((int32_t)(now - g_p10_cap_timeout_at) >= 0 ||
        g_p10_cap_timeout_counter >= g_p10_diag.cap_timeout_ticks) {
        g_p10_cap_timeout_counter = g_p10_diag.cap_timeout_ticks;
        g_p10_diag.cap_timeout_counter = g_p10_cap_timeout_counter;
        g_p10_cap_timing_state = P10_CAP_TIMING_TIMEOUT_PENDING;
    }
}

static void p10_reset_measurement_transients(void) {
    p10_reset_average();
    p10_reset_filter();
    p10_reset_current_single_aux_state();
    p10_reset_current_dual_state();
    if (!(g_p10_mode == DMM_MODE_V_AC && g_voltage_alt_bank_active)) {
        p10_reset_voltage_alt_state();
    }
    g_p10_overrange_latched = false;
    g_p10_continuity_beeping = false;
    g_p10_diag.raw_sample = 0;
    g_p10_diag.avg_sample = 0;
    g_p10_settle_samples =
        (g_p10_mode == DMM_MODE_P10_OHMS && p10_ohms_stock_logic_enabled()) ? 0u :
        p10_settle_count(g_p10_mode);
    g_p10_autorange_direction = 0;
    g_p10_autorange_holdoff = p10_ohms_stock_logic_enabled() ?
                              p10_autorange_holdoff_count(g_p10_mode, g_p10_range) : 0u;
    g_p10_ohms_stock_guard_state = false;
    g_p10_ohms_stock_guard_until = 0;
    g_p10_ohms_legacy_block_one_downshift = false;
    g_p10_ohms_legacy_downshift_block_until = 0;
    g_p10_ohms_legacy_upshift_block_until = 0;
    p10_range_controller_reset();
    p10_reset_frontend_timing_state_for_mode();
    g_frequency_secondary_valid = false;
    g_frequency_secondary_raw = 0;
    g_frequency_secondary_value = 0;
    g_frequency_secondary_range = 0;
    g_frequency_secondary_overrange = false;
    g_frequency_secondary_synced = false;
    g_frequency_secondary_last_event_ms = 0;
    g_frequency_secondary_last_count = 0;
    g_frequency_secondary_last_ref = 0;
    g_frequency_secondary_last_wrap = 0;
    if (!p10_mode_uses_frontend_timing_toggle() && g_p10_mode != DMM_MODE_LOZ) {
        g_lowz_timing_toggle_at = 0u;
    }
    if (g_p10_mode == DMM_MODE_LOZ) {
        g_lowz_alt_path = false;
        g_lowz_tight_threshold = false;
        g_lowz_switch_countdown = 5u;
        g_lowz_tight_countdown = 5u;
        g_lowz_timing_toggle_at = clock_millis() + P10_FRONTEND_TIMING_LOZ_TOGGLE_MS;
    } else {
        g_lowz_alt_path = false;
    }
    if (g_p10_mode == DMM_MODE_P10_CAP) {
        g_p10_diag.flags &= (uint8_t)~0x02u;
        g_p10_diag.instant_value = 0;
        g_p10_diag.display_value = 0;
        g_p10_diag.last_value = 0;
        g_p10_diag.cap_raw = 0;
        g_p10_cap_frontend_settle_until = 0u;
        g_p10_cap_frontend_sync_pending = true;
    } else {
        g_p10_cap_frontend_settle_until = 0u;
        g_p10_cap_frontend_sync_pending = false;
        p10_cap_timing_stop();
    }
}

static void p10_apply_current_mode_range_config(void) {
    g_p10_diag.mode = g_p10_mode;
    g_p10_diag.range = g_p10_range;

    p10_configure_current_frontend();
    p10_load_calibration(g_p10_mode, g_p10_range);
    if (dmm_mode_is_freq_group(g_p10_mode)) {
        g_p10_diag.status = 0u;
        g_p10_diag.reg1e = 0x00u;
        g_p10_diag.reg1f = 0x01u;
        g_p10_diag.reg20 = 0xFEu;
        g_p10_diag.reg29 = 0x00u;
        g_p10_diag.flags = 0u;
    }
    g_p10_diag.zero_offset = (uint32_t)g_p10_zero_offset;
    g_p10_diag.gain = g_p10_gain;
    g_p10_diag.gain_scale = g_p10_gain_scale;
    p10_schedule_cap_timeout();

    if (g_p10_mode == DMM_MODE_P10_DIODE) {
        frontend_set_diode_voltage_15v(g_p10_diode_option_15);
        g_p10_diode_voltage_gpio_active = true;
    } else if (g_p10_diode_voltage_gpio_active) {
        frontend_set_diode_voltage_15v(false);
        g_p10_diode_voltage_gpio_active = false;
    }
}

static void p10_current_single_select_path(bool aux_active) {
    g_current_single_aux_active = aux_active &&
                                  g_current_single_aux_enabled &&
                                  p10_current_single_aux_stock_mode(g_p10_mode);
    p10_reset_average();
    p10_reset_filter();
    if (g_current_single_aux_active) {
        uint8_t aux_mode = p10_current_single_aux_mode(g_p10_mode);
        frontend_configure_p10_voltage(aux_mode, 0u, false, false, g_p10_frontend_timing_word);
        p10_load_calibration(aux_mode, 0u);
    } else {
        p10_configure_current_frontend();
        p10_load_calibration(g_p10_mode, g_p10_range);
    }
    g_p10_diag.zero_offset = (uint32_t)g_p10_zero_offset;
    g_p10_diag.gain = g_p10_gain;
    g_p10_diag.gain_scale = g_p10_gain_scale;
}

static void p10_configure_current_frontend(void) {
    if (g_p10_mode == DMM_MODE_LOZ) {
        frontend_configure_p10_lowz(g_lowz_alt_path, g_p10_frontend_timing_word);
    } else if (g_current_single_aux_active &&
               p10_current_single_aux_stock_mode(g_p10_mode)) {
        frontend_configure_p10_voltage(p10_current_single_aux_mode(g_p10_mode), 0u,
                                       false, false, g_p10_frontend_timing_word);
    } else if (dmm_mode_uses_voltage_frontend(g_p10_mode)) {
        frontend_configure_p10_voltage(g_p10_mode, g_p10_range,
                                       g_voltage_alt_bank_active,
                                       g_voltage_alt_primary_path,
                                       g_p10_frontend_timing_word);
    } else if (dmm_mode_is_dual_current(g_p10_mode)) {
        frontend_configure_p10_current_dual_bank(g_p10_mode, g_p10_range,
                                                 g_current_dual_bank != 0u,
                                                 g_current_alt_range0,
                                                 g_current_alt_range1,
                                                 g_p10_frontend_timing_word);
    } else {
        frontend_configure_p10_mode(g_p10_mode, g_p10_range, g_p10_frontend_timing_word,
                                    p10_ohms_search_profile_active());
    }
}

static bool p10_ohms_fast_search_enabled(void) {
    return g_p10_mode == DMM_MODE_P10_OHMS &&
           g_p10_task_fast_range_enabled &&
           p10_ohms_stock_logic_enabled() &&
           g_p10_autorange;
}

static void p10_ohms_set_search_timing(void) {
    if (!p10_ohms_fast_search_enabled()) {
        return;
    }
    p10_ohms_begin_search();
    if (g_p10_frontend_timing_word == P10_FAST_SEARCH_TIMING_WORD) {
        return;
    }
    g_p10_frontend_timing_word = P10_FAST_SEARCH_TIMING_WORD;
}

static bool p10_ohms_restore_stock_timing_if_needed(void) {
    if (g_p10_mode != DMM_MODE_P10_OHMS ||
        (g_p10_frontend_timing_word == P10_STOCK_TIMING_WORD &&
         g_p10_measure_phase != DMM_MEASURE_PHASE_SEARCH)) {
        return false;
    }

    g_p10_frontend_timing_word = P10_STOCK_TIMING_WORD;
    p10_reset_average();
    p10_reset_filter();
    p10_ohms_begin_settle();
    p10_configure_current_frontend();
    return true;
}

static bool p10_ohms_accelerate_top_range_confirmation(void) {
    if (g_p10_mode != DMM_MODE_P10_OHMS ||
        g_p10_range < 5u ||
        g_p10_measure_phase != DMM_MEASURE_PHASE_PRECISION ||
        g_p10_frontend_timing_word != P10_STOCK_TIMING_WORD ||
        !p10_ohms_fast_search_enabled()) {
        return false;
    }

    /*
     * Range 5 uses the long stock precision conversion. Once that conversion
     * reports a possible range change or terminal overflow, confirm it in the
     * same range with the search profile instead of waiting for another long
     * conversion. No search-profile sample is accumulated for display.
     */
    p10_reset_average();
    p10_reset_filter();
    p10_ohms_set_search_timing();
    g_p10_autorange_holdoff = p10_autorange_holdoff_count(g_p10_mode, g_p10_range);
    p10_apply_current_mode_range_config();
    return true;
}

static void p10_reconfigure_after_range_change(uint8_t range, int8_t direction) {
    g_p10_range = range;
    if (!(g_p10_mode == DMM_MODE_P10_OHMS && p10_ohms_stock_logic_enabled())) {
        p10_reset_measurement_transients();
    } else {
        p10_reset_average();
        p10_reset_filter();
        p10_range_controller_reset();
        g_p10_overrange_latched = false;
        g_p10_autorange_holdoff = p10_autorange_holdoff_count(g_p10_mode, g_p10_range);
        g_p10_settle_samples = 0;
        g_p10_continuity_beeping = false;
        if (direction != 0) {
            if (g_p10_phase_range_steps != 0xFFFFu) {
                ++g_p10_phase_range_steps;
            }
            p10_ohms_set_search_timing();
        }
    }
    p10_apply_current_mode_range_config();
    g_p10_autorange_direction = direction;
}

static bool p10_defer_range_reconfigure_to_mainloop(void) {
    return g_p10_mode == DMM_MODE_V_DC || g_p10_mode == DMM_MODE_V_AC ||
           g_p10_mode == DMM_MODE_MV_DC || g_p10_mode == DMM_MODE_MV_AC;
}

static void p10_apply_or_defer_range_change(uint8_t range, int8_t direction) {
    if (p10_defer_range_reconfigure_to_mainloop()) {
        g_p10_range = range;
        g_p10_autorange_direction = direction;
        g_p10_range_reconfigure_pending = true;
        return;
    }
    p10_reconfigure_after_range_change(range, direction);
}

void dmm_p10_set_cap_timeout_enabled(bool enabled) {
    g_p10_cap_timeout_enabled = enabled;
    if (!enabled) {
        g_p10_cap_timeout_active = false;
    } else {
        p10_schedule_cap_timeout();
    }
}

static bool p10_handle_cap_timeout(dmm_reading_t *reading) {
    if (g_p10_mode != DMM_MODE_P10_CAP || g_p10_range >= 5u ||
        !g_p10_cap_timeout_active || g_p10_cap_frontend_sync_pending) {
        return false;
    }
    p10_cap_timing_service();
    if (g_p10_cap_timing_state != P10_CAP_TIMING_TIMEOUT_PENDING) {
        return false;
    }

    g_p10_cap_timeout_active = false;
    g_p10_cap_timing_state = P10_CAP_TIMING_RECONFIGURE;
    ++g_p10_diag.cap_timeouts;
    reading->has_sample = true;
    reading->range = g_p10_range;

    if (g_p10_autorange) {
        uint8_t next_range = g_p10_range;
        if (next_range < 2u) {
            next_range = 2u;
        } else if (next_range < 5u) {
            next_range = 5u;
        }
        if (next_range == 2u && g_p10_cap_force_low_range_after_timeout) {
            g_p10_cap_force_low_range_after_timeout = false;
            next_range = 0u;
        }
        if (next_range != g_p10_range) {
            int8_t direction = next_range > g_p10_range ? 1 : -1;
            p10_apply_or_defer_range_change(next_range, direction);
            reading->range = g_p10_range;
            reading->range_changed = true;
            reading->overrange = false;
            reading->instant_value = 0;
            reading->display_value = 0;
            reading->display_ready = false;
            g_p10_diag.instant_value = 0;
            g_p10_diag.display_value = 0;
            g_p10_diag.last_value = 0;
            g_p10_diag.cap_raw = 0;
            return true;
        }
    }

    reading->overrange = g_p10_overrange_latched;
    reading->display_ready = false;
    if (!reading->overrange) {
        reading->instant_value = 0;
        reading->display_value = 0;
        g_p10_diag.instant_value = 0;
        g_p10_diag.display_value = 0;
        g_p10_diag.last_value = 0;
        g_p10_diag.cap_raw = 0;
    }
    p10_schedule_cap_timeout();
    return true;
}

static bool p10_status_has_sample(uint8_t status) {
    uint8_t mask = (g_p10_mode == DMM_MODE_P10_CAP || dmm_mode_is_freq_group(g_p10_mode)) ? 0x01u : 0x04u;
    return (status & mask) != 0u;
}

static uint8_t p10_range_limit(uint8_t mode) {
    return measurement_profile_autorange_count(mode);
}

static bool p10_ohms_stock_guard_active(void) {
    if (!g_p10_ohms_stock_guard_state) {
        return false;
    }
    if ((int32_t)(clock_millis() - g_p10_ohms_stock_guard_until) < 0) {
        return true;
    }
    g_p10_ohms_stock_guard_state = false;
    return false;
}

static void p10_ohms_stock_guard_arm(void) {
    g_p10_ohms_stock_guard_state = true;
    g_p10_ohms_stock_guard_until = clock_millis() + P10_OHMS_STOCK_GUARD_MS;
}

static void p10_ohms_stock_guard_reset(void) {
    g_p10_ohms_stock_guard_state = false;
    g_p10_ohms_stock_guard_until = 0;
}

static bool p10_ohms_legacy_downshift_blocked(void) {
    if (g_p10_mode != DMM_MODE_P10_OHMS || !g_p10_ohms_legacy_block_one_downshift) {
        return false;
    }
    if ((int32_t)(clock_millis() - g_p10_ohms_legacy_downshift_block_until) < 0) {
        return true;
    }
    g_p10_ohms_legacy_block_one_downshift = false;
    return false;
}

static bool p10_ohms_legacy_upshift_blocked(void) {
    return g_p10_mode == DMM_MODE_P10_OHMS &&
           (int32_t)(clock_millis() - g_p10_ohms_legacy_upshift_block_until) < 0;
}

static bool p10_ohms_autorange_step_blocked(bool upshift) {
    if (g_p10_mode != DMM_MODE_P10_OHMS || !g_p10_autorange) {
        p10_ohms_stock_guard_reset();
        return false;
    }

    if (g_p10_ohms_guard_policy == P10_OHMS_GUARD_LEGACY) {
        return upshift ? p10_ohms_legacy_upshift_blocked() : p10_ohms_legacy_downshift_blocked();
    }

    if (!upshift && p10_ohms_stock_guard_active()) {
        return true;
    }
    return false;
}

static void p10_ohms_autorange_note_step(bool upshift) {
    if (g_p10_mode != DMM_MODE_P10_OHMS || !g_p10_autorange) {
        return;
    }

    if (g_p10_ohms_guard_policy == P10_OHMS_GUARD_LEGACY) {
        if (upshift) {
            g_p10_ohms_legacy_block_one_downshift = true;
            g_p10_ohms_legacy_downshift_block_until = clock_millis() + 100u;
        } else {
            g_p10_ohms_legacy_upshift_block_until = clock_millis() + 250u;
        }
        return;
    }

    p10_ohms_stock_guard_arm();
}

static void p10_ohms_autorange_note_in_range(void) {
    if (g_p10_mode == DMM_MODE_P10_OHMS && g_p10_ohms_guard_policy == P10_OHMS_GUARD_STOCK) {
        p10_ohms_stock_guard_reset();
    }
}

static uint16_t p10_overrange_threshold(uint8_t mode, uint8_t range) {
    if (dmm_mode_is_voltage(mode)) {
        (void)range;
        return 50000u;
    }
    if (mode == DMM_MODE_P10_OHMS) {
        if (range >= 7u) {
            range = 6u;
        }
        return k_p10_mode9_thresholds[range];
    }
    if (mode == DMM_MODE_P10_CAP) {
        if (range >= 6u) {
            range = 5u;
        }
        if (range == 5u) {
            return 50000u;
        }
        return k_p10_mode12_thresholds[range];
    }
    if (mode == DMM_MODE_P10_DIODE) {
        return g_p10_diode_option_15 ? DIODE_OPEN_MAX_15V : DIODE_OPEN_MAX_3V;
    }
    if (dmm_mode_is_current(mode)) {
        return 50000u;
    }
    return 50000u;
}

static uint32_t p10_high_threshold(uint8_t mode, uint8_t range) {
    uint32_t threshold = p10_overrange_threshold(mode, range);
    if (mode == DMM_MODE_P10_DIODE) {
        return threshold;
    }
    return threshold + threshold / (g_p10_overrange_latched ? 12u : 10u);
}

static bool p10_current_range_overrange(int32_t value) {
    uint32_t high = p10_high_threshold(g_p10_mode, g_p10_range);
    return abs_i32_to_u32(value) > high;
}

static bool p10_autorange_display_blank_active(void) {
    return g_p10_diag.autorange_decision == 4u ||
           g_p10_diag.autorange_decision == 5u ||
           g_p10_diag.autorange_decision == 6u;
}

static void p10_diag_autorange(uint8_t decision, uint8_t from, uint8_t to,
                               int32_t value, uint32_t abs_value,
                               uint32_t low, uint32_t high) {
    g_p10_diag.autorange_decision = decision;
    g_p10_diag.autorange_from = from;
    g_p10_diag.autorange_to = to;
    g_p10_diag.autorange_value = value;
    g_p10_diag.autorange_abs = abs_value;
    g_p10_diag.autorange_low = low;
    g_p10_diag.autorange_high = high;
}

static bool p10_current_dual_update_alt_ranges(int32_t bank0, int32_t bank1) {
    if (!g_p10_autorange) {
        return false;
    }

    bool changed = false;
    uint32_t abs0 = abs_i32_to_u32(bank0);
    uint32_t abs1 = abs_i32_to_u32(bank1);
    bool low_range_dc = p10_current_mode_uses_dc_alt1_range2(g_p10_mode) &&
                        g_current_alt_range1 != 0u;
    uint32_t high1 = low_range_dc ? 11000u : 55000u;
    uint32_t low1 = low_range_dc ? 400u : 4000u;

    if (abs0 > 55000u && g_current_alt_range0 == 0u) {
        g_current_alt_range0 = 1u;
        changed = true;
    } else if (abs0 < 3200u && g_current_alt_range0 != 0u) {
        g_current_alt_range0 = 0u;
        changed = true;
    }

    if (abs1 > high1 && g_current_alt_range1 == 0u) {
        g_current_alt_range1 = p10_current_mode_uses_dc_alt1_range2(g_p10_mode) ? 2u : 1u;
        changed = true;
    } else if (abs1 < low1 && g_current_alt_range1 != 0u) {
        g_current_alt_range1 = 0u;
        changed = true;
    }

    if (changed) {
        g_p10_range = p10_current_range_from_pair(g_p10_mode);
        g_p10_diag.range = g_p10_range;
    }

    return changed;
}

static int32_t p10_current_single_jitter_filter(int32_t value) {
    int32_t delta = value - g_current_single_jitter_value;
    if (delta >= 11 || delta < -10) {
        g_current_single_jitter_count = 0;
        g_current_single_jitter_sum = 0;
    }
    if (g_current_single_jitter_count == 0u) {
        g_current_single_jitter_count = 1u;
        g_current_single_jitter_sum = value;
        g_current_single_jitter_value = value;
        return value;
    }

    ++g_current_single_jitter_count;
    if (g_current_single_jitter_count < 4u) {
        g_current_single_jitter_sum += value;
    } else {
        g_current_single_jitter_count = 3u;
        g_current_single_jitter_sum = value + g_current_single_jitter_sum -
                                      g_current_single_jitter_value;
    }
    g_current_single_jitter_value =
        g_current_single_jitter_sum / (int32_t)g_current_single_jitter_count;
    return g_current_single_jitter_value;
}

static bool p10_current_single_accept_average(int32_t averaged, dmm_reading_t *reading) {
    if (!p10_current_single_main_stock_mode(g_p10_mode)) {
        return false;
    }

    bool aux_path = g_current_single_aux_active;
    int32_t value = averaged;
    if (p10_current_single_ac_main_mode(g_p10_mode) && value < 0) {
        value = 0;
    }
    uint16_t deadband = 10u;
    uint8_t max_count = 10u;
    if (p10_current_single_ac_main_mode(g_p10_mode)) {
        deadband = 20u;
    } else if (aux_path) {
        deadband = 2u;
        max_count = 2u;
    }
    value = p10_smooth_with_params(value, deadband, max_count);
    if (!aux_path && g_p10_range == 0u) {
        value -= g_current_single_extra_zero;
        if (p10_current_single_ac_main_mode(g_p10_mode) && value < 0) {
            value = 0;
        }
    }
    if (!aux_path && p10_current_single_ac_main_mode(g_p10_mode) &&
        g_p10_range == 0u && value < 31) {
        value = 0;
    }

    bool aux_cycle_enabled = g_current_single_aux_enabled &&
                             p10_current_single_aux_stock_mode(g_p10_mode);
    if (aux_cycle_enabled) {
        ++g_current_single_counter;
        uint8_t threshold = p10_current_single_switch_threshold(g_p10_mode);
        if (g_current_single_counter <= threshold) {
            reading->display_ready = false;
            return true;
        }

        g_current_single_counter = 0;
        if (aux_path) {
            g_current_single_aux_value = p10_current_single_jitter_filter(value);
            g_current_single_aux_over_threshold = g_current_single_aux_value >= 5001;
            (void)g_current_single_aux_over_threshold;
            p10_current_single_select_path(false);
            reading->display_ready = false;
            return true;
        }
    }

    reading->display_value = value;
    reading->instant_value = value;
    g_p10_diag.display_value = value;
    g_p10_diag.instant_value = value;
    g_p10_diag.last_value = value;

    if (g_p10_settle_samples != 0u) {
        --g_p10_settle_samples;
        reading->display_ready = false;
        reading->overrange = false;
        return true;
    }

    if (p10_apply_autorange(value)) {
        reading->range_changed = true;
        reading->range = g_p10_range;
        reading->overrange = g_p10_overrange_latched;
        reading->display_ready = false;
        return true;
    }

    if (aux_cycle_enabled) {
        p10_current_single_select_path(true);
    }
    reading->display_ready = true;
    reading->overrange = g_p10_overrange_latched;
    return true;
}

static bool p10_current_single_process_raw_sample(int32_t raw_sample, dmm_reading_t *reading) {
    if (!p10_current_single_main_stock_mode(g_p10_mode)) {
        return false;
    }

    int32_t instant = p10_apply_calibration(raw_sample);
    if (p10_current_single_ac_main_mode(g_p10_mode)) {
        instant = p10_apply_stock_threshold_corrections(instant);
    }
    reading->has_sample = true;
    reading->instant_value = instant;
    reading->display_value = instant;
    g_p10_diag.instant_value = instant;
    g_p10_diag.display_value = instant;
    g_p10_diag.last_value = instant;
    if (p10_range_controller_fast_service(instant, reading)) {
        return true;
    }

    g_p10_display_accum += raw_sample;
    ++g_p10_display_count;
    if (g_p10_display_count < 16u) {
        reading->display_ready = false;
        return true;
    }

    int64_t avg = g_p10_display_accum < 0 ? -((-g_p10_display_accum) >> 4) :
                                             (g_p10_display_accum >> 4);
    if (avg > 0x7FFFFFFF) {
        avg = 0x7FFFFFFF;
    } else if (avg < (-0x7FFFFFFF - 1LL)) {
        avg = -0x7FFFFFFF - 1LL;
    }
    p10_reset_average();
    g_p10_diag.avg_sample = (int32_t)avg;

    int32_t value = p10_apply_calibration((int32_t)avg);
    if (p10_current_single_ac_main_mode(g_p10_mode)) {
        value = p10_apply_stock_threshold_corrections(value);
    }
    reading->display_value = value;
    g_p10_diag.display_value = value;
    if (p10_current_single_accept_average(value, reading)) {
        return true;
    }
    reading->display_ready = true;
    return true;
}

static int32_t p10_current_dual_combine(int32_t bank0, int32_t bank1) {
    uint64_t a = abs_i32_to_u32(bank0);
    uint64_t b = abs_i32_to_u32(bank1);
    uint32_t divisor = 100000u;
    if (g_p10_mode != DMM_MODE_LEGACY_CURRENT_AC &&
        g_p10_mode != DMM_MODE_MA_ALT_AC &&
        g_p10_mode != DMM_MODE_MA_AUX &&
        g_p10_mode != DMM_MODE_MA_BURDEN &&
        g_p10_range >= 2u) {
        divisor = 10000u;
    }
    uint32_t result = div_u64_u32(a * b, divisor);
    if (result > 0x7FFFFFFFu) {
        result = 0x7FFFFFFFu;
    }
    return (int32_t)result;
}

static uint8_t p10_dual_current_segment(const uint32_t *axis, uint8_t count, uint32_t value) {
    uint8_t segment = 1u;
    for (uint8_t i = 1u; i < (uint8_t)(count - 1u); ++i) {
        if (axis[i] < value) {
            segment = (uint8_t)(i + 1u);
        } else {
            break;
        }
    }
    return segment;
}

static int32_t p10_dual_current_interpolate(int32_t bank0, int32_t bank1,
                                            uint8_t y_axis_index,
                                            uint8_t x_segment_min,
                                            const int16_t *table,
                                            uint8_t cols) {
    uint32_t bank0_abs = abs_i32_to_u32(bank0);
    uint32_t bank1_abs = abs_i32_to_u32(bank1);
    uint8_t y_segment = p10_dual_current_segment(k_dual_current_temp_y_axis[y_axis_index],
                                                 8u, bank1_abs);
    uint8_t x_segment = p10_dual_current_segment(k_dual_current_temp_x_axis,
                                                 8u, bank0_abs);
    if (x_segment < x_segment_min) {
        x_segment = x_segment_min;
    }

    int16_t eb8 = table[(uint8_t)(cols * y_segment - (uint8_t)(cols + 1u) + x_segment)];
    int16_t ebc = table[(uint8_t)(cols * y_segment - cols + x_segment)];
    int16_t ec0 = table[(uint8_t)(cols * y_segment - 1u + x_segment)];
    int16_t ec4 = table[(uint8_t)(cols * y_segment + x_segment)];
    uint32_t y_low = k_dual_current_temp_y_axis[y_axis_index][y_segment - 1u];
    uint32_t y_high = k_dual_current_temp_y_axis[y_axis_index][y_segment];
    uint32_t x_low = k_dual_current_temp_x_axis[x_segment - 1u];
    uint32_t x_high = k_dual_current_temp_x_axis[x_segment];
    int32_t y_percent = y_high != y_low ?
        (int32_t)(100u * (bank1_abs - y_low) / (y_high - y_low)) : 0;
    int32_t x_percent = x_high != x_low ?
        (int32_t)(100u * (bank0_abs - x_low) / (x_high - x_low)) : 0;
    y_percent = clamp_i32(y_percent, -100, 100);
    x_percent = clamp_i32(x_percent, -100, 100);

    int32_t lower = ((int32_t)(ebc - eb8) * x_percent) / 100 + eb8;
    int32_t upper = ((int32_t)(ec4 - ec0) * x_percent) / 100 + ec0;
    return ((upper - lower) * y_percent) / 100 + lower;
}

static int32_t p10_dual_current_temperature_comp(int32_t bank0, int32_t bank1) {
    if (bank0 >= 60001) {
        return bank0;
    }

    uint8_t table_range = g_p10_range < 4u ? g_p10_range : 3u;
    uint8_t cap_table = frontend_cap_option_table_enabled() ? 1u : 0u;

    if (g_p10_mode == DMM_MODE_MA_AUX) {
        if (bank0 < 0 || bank1 < 0) {
            return bank0;
        }
        int32_t comp = p10_dual_current_interpolate(
            bank0, bank1, g_p10_range < 2u ? 0u : 1u, 1u,
            &k_mode13_temp_comp_table[cap_table][table_range][0][0], 8u);
        return bank0 + comp;
    }

    if (g_p10_mode == DMM_MODE_MVA_AC) {
        if (bank0 < 0 || bank1 < 0) {
            return bank0;
        }
        uint8_t y_axis = g_p10_range >= 2u ? 2u : 1u;
        const int16_t *table = k_mode15_temp_comp_table8[cap_table][table_range];
        uint8_t cols = 8u;
        if (!frontend_cap_option_table_enabled() && table_range >= 2u) {
            table = k_mode15_temp_comp_table7_nc[table_range - 2u];
            cols = 7u;
        }
        int32_t comp = p10_dual_current_interpolate(bank0, bank1, y_axis, 2u,
                                                    table, cols);
        return bank0 + comp;
    }

    if (g_p10_mode == DMM_MODE_MVA_DC) {
        bool signed_cap_range2 = frontend_cap_option_table_enabled() &&
                                 g_p10_range == 2u &&
                                 ((bank0 < 0 && bank1 >= 0) ||
                                  (bank0 >= 0 && bank1 < 0));
        if (!signed_cap_range2 && (bank0 < 0 || bank1 < 0)) {
            return bank0;
        }
        uint8_t table_set = signed_cap_range2 ? 2u : cap_table;
        int32_t comp = p10_dual_current_interpolate(bank0, bank1,
                                                    g_p10_range >= 2u ? 2u : 1u,
                                                    1u,
                                                    k_mode24_temp_comp_table[table_set][table_range],
                                                    8u);
        if (signed_cap_range2 && bank0 < 0) {
            return bank0 - comp;
        }
        return bank0 + comp;
    }

    return bank0;
}

static int32_t p10_current_dc_sign_offset_compensation(int32_t primary, int32_t secondary) {
    if (secondary >= -2500 && secondary <= 2500) {
        return 0;
    }
    if (frontend_cap_option_table_enabled() || calibration_read_u8(38u) == 0xA5u) {
        return 0;
    }

    if (g_p10_mode == DMM_MODE_MA_BURDEN) {
        switch (g_p10_range) {
        case 0:
            return secondary < 0 ? (primary < 0 ? -300 : -200) : 200;
        case 1:
            return secondary < 0 ? -2100 : 2100;
        case 2:
            return secondary < 0 ? -170 : (primary >= 0 ? 130 : 0);
        case 3:
            return secondary < 0 ? -2130 : (primary >= 0 ? 2110 : 0);
        default:
            return 0;
        }
    }

    if (g_p10_mode == DMM_MODE_MA_ALT_AC) {
        switch (g_p10_range) {
        case 0:
            return secondary < 0 ? (primary < 0 ? -70 : -71) :
                                    (primary < 0 ? 132 : 130);
        case 1:
            return secondary < 0 ? (primary < 0 ? -1529 : -1548) :
                                    (primary < 0 ? 1605 : 1583);
        case 2:
            return secondary < 0 ? -120 : (primary >= 0 ? 82 : 0);
        case 3:
            return secondary < 0 ? -1606 : (primary >= 0 ? 1537 : 0);
        default:
            return 0;
        }
    }

    if (g_p10_mode == DMM_MODE_MVA_DC) {
        switch (g_p10_range) {
        case 0:
            return secondary < 0 ? -65 : (primary < 0 ? 131 : 128);
        case 1:
            return secondary < 0 ? (primary < 0 ? -1499 : -1517) :
                                    (primary < 0 ? 1581 : 1559);
        case 2:
            return secondary < 0 ? -16 : (primary >= 0 ? 5 : 0);
        case 3:
            return secondary < 0 ? -161 : (primary >= 0 ? 156 : 0);
        default:
            return 0;
        }
    }

    return 0;
}

static bool p10_current_dual_accept_average(int32_t averaged, dmm_reading_t *reading) {
    if (!dmm_mode_is_dual_current(g_p10_mode)) {
        return false;
    }

    if (g_current_dual_bank != 0u) {
        if (g_p10_mode >= DMM_MODE_MA_AUX && g_p10_mode <= DMM_MODE_MVA_AC) {
            if (averaged < 0) {
                averaged = -averaged;
            }
        } else {
            averaged = p10_smooth_current_bank(1u, averaged, 10u, 10u);
            averaged += p10_current_dc_sign_offset_compensation(g_current_bank0_value, averaged);
        }
        g_current_bank1_value = averaged;
    } else {
        if (g_p10_mode >= DMM_MODE_MA_AUX && g_p10_mode <= DMM_MODE_MVA_AC) {
            averaged = p10_smooth_current_bank(0u, averaged, 20u, 10u);
        } else {
            averaged = p10_smooth_current_bank(0u, averaged, 2u, 2u);
        }
        g_current_bank0_value = averaged;
    }

    ++g_current_dual_counter;
    uint8_t required = g_current_dual_bank != 0u ? 7u : 4u;
    if (g_current_dual_counter < required) {
        reading->display_ready = false;
        return true;
    }

    g_current_dual_counter = 0;
    bool both_banks_valid = dmm_p10_current_dual_display_ready();
    if (g_current_dual_bank == 0u && both_banks_valid) {
        g_current_bank0_value =
            p10_dual_current_temperature_comp(g_current_bank0_value,
                                              g_current_bank1_value);
        if (g_p10_mode >= DMM_MODE_MA_AUX && g_p10_mode <= DMM_MODE_MVA_AC &&
            g_current_bank0_value < 0) {
            g_current_bank0_value = -g_current_bank0_value;
        }
    }
    bool changed = both_banks_valid &&
                   p10_current_dual_update_alt_ranges(g_current_bank0_value, g_current_bank1_value);
    if (changed) {
        /*
         * Stock range_change_reconfigure_frontend_display_mux() resets the
         * dual-bank phase to bank 0 and does not perform the normal bank
         * toggle after a range change.
         */
        g_current_dual_bank = 0;
        g_current_dual_counter = 0;
    } else {
        g_current_dual_bank ^= 1u;
    }
    p10_reset_average();
    p10_reset_filter();
    if (changed) {
        p10_apply_current_mode_range_config();
    } else {
        p10_configure_current_frontend();
    }

    reading->display_value = both_banks_valid ?
        p10_current_dual_combine(g_current_bank0_value, g_current_bank1_value) : 0;
    reading->instant_value = reading->display_value;
    reading->has_sample = true;
    reading->display_ready = both_banks_valid && !changed;
    reading->range_changed = changed;
    reading->overrange = false;
    return true;
}

static bool p10_voltage_alt_accept_average(int32_t averaged, dmm_reading_t *reading) {
    if (g_p10_mode != DMM_MODE_V_AC || !g_voltage_alt_bank_active) {
        return false;
    }

    if (g_voltage_alt_primary_path) {
        g_voltage_alt_primary_value = averaged;
    } else {
        g_voltage_alt_secondary_value = averaged;
    }

    ++g_voltage_alt_counter;
    uint8_t required = g_voltage_alt_primary_path ? 3u : 5u;
    if (g_voltage_alt_counter < required) {
        reading->display_ready = false;
        return true;
    }

    g_voltage_alt_counter = 0;
    if (!g_voltage_alt_primary_path) {
        g_voltage_alt_primary_path = true;
        p10_reset_average();
        p10_reset_filter();
        p10_configure_current_frontend();
        p10_load_calibration(g_p10_mode, g_p10_range);
        g_p10_diag.zero_offset = (uint32_t)g_p10_zero_offset;
        g_p10_diag.gain = g_p10_gain;
        g_p10_diag.gain_scale = g_p10_gain_scale;
        reading->display_ready = false;
        return true;
    }

    uint64_t primary = abs_i32_to_u32(g_voltage_alt_primary_value);
    uint64_t secondary = abs_i32_to_u32(g_voltage_alt_secondary_value);
    uint32_t combined = isqrt_u64(primary * primary + secondary * secondary);
    if (combined > 0x7FFFFFFFu) {
        combined = 0x7FFFFFFFu;
    }
    combined = (uint32_t)p10_smooth_with_params((int32_t)combined, 20u, 10u);

    g_voltage_alt_primary_path = false;
    p10_reset_average();
    p10_reset_filter();
    p10_configure_current_frontend();
    p10_load_calibration(g_p10_mode, g_p10_range);
    g_p10_diag.zero_offset = (uint32_t)g_p10_zero_offset;
    g_p10_diag.gain = g_p10_gain;
    g_p10_diag.gain_scale = g_p10_gain_scale;

    reading->display_value = (int32_t)combined;
    reading->instant_value = reading->display_value;
    reading->has_sample = true;
    reading->display_ready = true;
    reading->overrange = p10_current_range_overrange(reading->display_value);
    g_p10_overrange_latched = reading->overrange;
    return true;
}

static bool p10_ohms_fast_downrange(int32_t value) {
    if (g_p10_mode != DMM_MODE_P10_OHMS || !g_p10_autorange || p10_ohms_stock_logic_enabled() ||
        g_p10_range == 0u) {
        return false;
    }
    if (g_p10_settle_samples != 0u || p10_ohms_legacy_downshift_blocked()) {
        return false;
    }

    uint32_t low = p10_overrange_threshold(g_p10_mode, g_p10_range) / 10u;
    if (abs_i32_to_u32(value) >= low) {
        return false;
    }

    p10_reconfigure_after_range_change((uint8_t)(g_p10_range - 1u), -1);
    g_p10_autorange_direction = -1;
    g_p10_ohms_legacy_upshift_block_until = clock_millis() + 250u;
    return true;
}

static bool p10_apply_autorange(int32_t value) {
    uint8_t limit = p10_range_limit(g_p10_mode);
    uint32_t threshold = p10_overrange_threshold(g_p10_mode, g_p10_range);
    uint8_t from = g_p10_range;
    uint32_t abs_value = abs_i32_to_u32(value);
    uint32_t high = p10_high_threshold(g_p10_mode, g_p10_range);
    uint32_t low = threshold / ((g_p10_mode == DMM_MODE_P10_CAP && g_p10_range == 5u) ? 100u : 10u);
    p10_diag_autorange(0u, from, from, value, abs_value, low, high);

    if (!g_p10_autorange || limit < 2u) {
        g_p10_autorange_direction = 0;
        if (g_p10_mode == DMM_MODE_P10_OHMS) {
            p10_ohms_stock_guard_reset();
            (void)p10_ohms_restore_stock_timing_if_needed();
        }
        g_p10_overrange_latched = abs_value > high;
        return false;
    }

    if (g_p10_mode == DMM_MODE_P10_OHMS && p10_ohms_stock_logic_enabled() &&
        g_p10_autorange_holdoff != 0u) {
        --g_p10_autorange_holdoff;
        g_p10_autorange_direction = 0;
        p10_diag_autorange(4u, from, from, value, abs_value, low, high);
        return false;
    }

    if (abs_value > high) {
        if (g_p10_mode == DMM_MODE_P10_OHMS && p10_ohms_autorange_step_blocked(true)) {
            g_p10_autorange_direction = 0;
            p10_diag_autorange(5u, from, from, value, abs_value, low, high);
            return false;
        }
        if (g_p10_range + 1u < limit) {
            uint8_t to = (uint8_t)(g_p10_range + 1u);
            p10_diag_autorange(1u, from, to, value, abs_value, low, high);
            p10_apply_or_defer_range_change(to, 1);
            g_p10_autorange_direction = 1;
            if (g_p10_mode == DMM_MODE_P10_OHMS) {
                p10_ohms_autorange_note_step(true);
            }
            return true;
        }
        g_p10_overrange_latched = true;
        p10_diag_autorange(3u, from, from, value, abs_value, low, high);
        (void)p10_ohms_restore_stock_timing_if_needed();
        return false;
    }

    g_p10_overrange_latched = false;
    if (abs_value >= low) {
        g_p10_autorange_direction = 0;
        p10_ohms_autorange_note_in_range();
        (void)p10_ohms_restore_stock_timing_if_needed();
        return false;
    }

    if (abs_value < low && g_p10_range > 0u) {
        if (g_p10_mode == DMM_MODE_P10_OHMS && p10_ohms_autorange_step_blocked(false)) {
            g_p10_autorange_direction = 0;
            p10_diag_autorange(6u, from, from, value, abs_value, low, high);
            return false;
        }
        if (g_p10_mode == DMM_MODE_P10_CAP && g_p10_range == 2u && abs_value < 11u) {
            g_p10_cap_force_low_range_after_timeout = true;
        }
        uint8_t to = (uint8_t)(g_p10_range - 1u);
        p10_diag_autorange(2u, from, to, value, abs_value, low, high);
        p10_apply_or_defer_range_change(to, -1);
        g_p10_autorange_direction = -1;
        if (g_p10_mode == DMM_MODE_P10_OHMS) {
            p10_ohms_autorange_note_step(false);
        }
        return true;
    }
    g_p10_autorange_direction = 0;
    return false;
}

static bool p10_range_controller_fast_mode_enabled(void) {
    if (!g_p10_task_fast_range_enabled || !g_p10_autorange) {
        return false;
    }
    if (g_p10_mode == DMM_MODE_P10_OHMS) {
        return p10_ohms_stock_logic_enabled();
    }
    if (g_p10_mode == DMM_MODE_V_DC || g_p10_mode == DMM_MODE_MV_DC ||
        g_p10_mode == DMM_MODE_MV_AC) {
        return true;
    }
    if (g_p10_mode == DMM_MODE_V_AC) {
        return !g_voltage_alt_bank_active;
    }
    if (p10_current_single_main_stock_mode(g_p10_mode)) {
        return !g_current_single_aux_active;
    }
    return false;
}

static bool p10_range_controller_obvious_ohms_downrange(uint8_t from,
                                                        uint32_t abs_value,
                                                        uint32_t low) {
    if (g_p10_mode != DMM_MODE_P10_OHMS || !p10_ohms_stock_logic_enabled() ||
        from == 0u || low < 4u) {
        return false;
    }

    /*
     * The stock guard is useful near a boundary, but it costs about one second
     * per downrange step. For a clear short/open-to-low transition the value is
     * far below the lower threshold, so we can continue searching immediately
     * after the normal frontend holdoff without displaying an intermediate
     * value.
     */
    return abs_value < (low / P10_OHMS_OBVIOUS_DOWN_DIV);
}

static void p10_range_controller_hold_display(dmm_reading_t *reading,
                                              uint8_t range,
                                              bool overrange) {
    reading->range_changed = true;
    reading->range = range;
    reading->display_ready = false;
    reading->overrange = overrange;
}

static bool p10_range_controller_fast_service(int32_t value, dmm_reading_t *reading) {
    if (!p10_range_controller_fast_mode_enabled()) {
        p10_range_controller_reset();
        return false;
    }

    uint8_t limit = p10_range_limit(g_p10_mode);
    if (limit < 2u || g_p10_settle_samples != 0u) {
        p10_range_controller_reset();
        return false;
    }

    uint8_t from = g_p10_range;
    uint32_t abs_value = abs_i32_to_u32(value);
    uint32_t threshold = p10_overrange_threshold(g_p10_mode, from);
    uint32_t high = p10_high_threshold(g_p10_mode, from);
    uint32_t low = threshold / 10u;
    bool terminal_overrange = abs_value > high && from + 1u >= limit;
    int8_t direction = 0;
    if (abs_value > high) {
        direction = 1;
    } else if (abs_value < low && from > 0u) {
        direction = -1;
    } else {
        if (g_p10_mode == DMM_MODE_P10_OHMS &&
            g_p10_measure_phase == DMM_MEASURE_PHASE_SEARCH) {
            if (g_p10_autorange_holdoff != 0u) {
                --g_p10_autorange_holdoff;
                reading->range = from;
                reading->display_ready = false;
                reading->overrange = false;
                return true;
            }
            if (g_p10_range_controller_target_count + 1u <
                    P10_RANGE_CONTROLLER_TARGET_CONFIRM_SAMPLES) {
                ++g_p10_range_controller_target_count;
                reading->range = from;
                reading->display_ready = false;
                reading->overrange = false;
                return true;
            }
        }
        if (p10_ohms_restore_stock_timing_if_needed()) {
            reading->range = g_p10_range;
            reading->range_changed = false;
            reading->display_ready = false;
            reading->overrange = false;
            p10_range_controller_reset();
            return true;
        }
        p10_range_controller_reset();
        return false;
    }
    g_p10_range_controller_target_count = 0u;

    bool obvious_ohms_downrange =
        direction < 0 && p10_range_controller_obvious_ohms_downrange(from, abs_value, low);

    if (direction != g_p10_range_controller_candidate_dir) {
        g_p10_range_controller_candidate_dir = direction;
        g_p10_range_controller_candidate_count = 1u;
        if (!obvious_ohms_downrange) {
            (void)p10_ohms_accelerate_top_range_confirmation();
            p10_diag_autorange(4u, from, from, value, abs_value, low, high);
            p10_range_controller_hold_display(reading, from, direction > 0);
            return true;
        }
    }
    if (!obvious_ohms_downrange &&
        g_p10_range_controller_candidate_count < P10_RANGE_CONTROLLER_CONFIRM_SAMPLES) {
        ++g_p10_range_controller_candidate_count;
    }
    if (!obvious_ohms_downrange &&
        g_p10_range_controller_candidate_count < P10_RANGE_CONTROLLER_CONFIRM_SAMPLES) {
        p10_diag_autorange(4u, from, from, value, abs_value, low, high);
        p10_range_controller_hold_display(reading, from, direction > 0);
        return true;
    }

    if (!obvious_ohms_downrange && g_p10_autorange_holdoff != 0u) {
        --g_p10_autorange_holdoff;
        p10_diag_autorange(4u, from, from, value, abs_value, low, high);
        p10_range_controller_hold_display(reading, from, direction > 0);
        return true;
    }

    if (terminal_overrange) {
        p10_range_controller_reset();
        g_p10_overrange_latched = true;
        p10_diag_autorange(3u, from, from, value, abs_value, low, high);
        (void)p10_ohms_restore_stock_timing_if_needed();
        reading->range = from;
        reading->display_ready = false;
        reading->overrange = true;
        return true;
    }

    p10_range_controller_reset();
    if (obvious_ohms_downrange) {
        uint8_t to = (uint8_t)(from - 1u);
        p10_diag_autorange(2u, from, to, value, abs_value, low, high);
        p10_apply_or_defer_range_change(to, -1);
        p10_ohms_stock_guard_reset();
        reading->range_changed = true;
        reading->range = g_p10_range;
        reading->display_ready = false;
        reading->overrange = false;
        return true;
    }

    if (!p10_apply_autorange(value)) {
        return false;
    }

    reading->range_changed = true;
    reading->range = g_p10_range;
    reading->display_ready = false;
    reading->overrange = direction > 0;
    return true;
}

static bool p10_lowz_stock_path_service(int32_t value, dmm_reading_t *reading) {
    if (g_p10_mode != DMM_MODE_LOZ) {
        return false;
    }

    int32_t threshold = g_lowz_tight_threshold ? 30 : 100;
    bool outside_window = value > threshold || value < -threshold ||
                          g_p10_overrange_latched;
    if (outside_window) {
        g_lowz_switch_countdown = 5u;
        if (!g_lowz_tight_threshold) {
            reading->instant_value = 0;
            reading->display_value = 0;
            g_p10_diag.instant_value = 0;
            g_p10_diag.display_value = 0;
            g_p10_diag.last_value = 0;
            if (g_lowz_tight_countdown > 0u) {
                --g_lowz_tight_countdown;
            }
            if (g_lowz_tight_countdown == 0u) {
                g_lowz_tight_threshold = true;
            }
        }
        return false;
    }

    g_lowz_tight_threshold = false;
    g_lowz_tight_countdown = 5u;
    reading->instant_value = 0;
    reading->display_value = 0;
    reading->overrange = false;
    g_p10_overrange_latched = false;
    g_p10_diag.instant_value = 0;
    g_p10_diag.display_value = 0;
    g_p10_diag.last_value = 0;
    if (g_lowz_switch_countdown > 0u) {
        --g_lowz_switch_countdown;
    }
    if (g_lowz_switch_countdown == 0u) {
        g_lowz_alt_path = !g_lowz_alt_path;
        g_lowz_switch_countdown = 5u;
        g_p10_frontend_timing_phase = false;
        g_p10_frontend_timing_word = P10_STOCK_TIMING_WORD;
        g_lowz_timing_toggle_at = clock_millis() + P10_FRONTEND_TIMING_LOZ_TOGGLE_MS;
        p10_reset_average();
        p10_reset_filter();
        g_p10_settle_samples = 0;
        p10_apply_current_mode_range_config();
        reading->display_ready = false;
    }
    return false;
}

static bool p10_voltage_process_raw_sample(int32_t raw_sample, dmm_reading_t *reading) {
    if (!dmm_mode_is_voltage(g_p10_mode)) {
        return false;
    }

    bool ac_path = (g_p10_mode == DMM_MODE_LOZ && g_lowz_alt_path) ||
                   g_p10_mode == DMM_MODE_MV_AC ||
                   (g_p10_mode == DMM_MODE_V_AC &&
                    (!g_voltage_alt_bank_active || !g_voltage_alt_primary_path));
    reading->has_sample = true;
    if (!(g_p10_mode == DMM_MODE_V_AC && g_voltage_alt_bank_active)) {
        int32_t instant_display = ac_path ? p10_apply_ac_voltage_calibration(raw_sample) :
                                            p10_apply_calibration(raw_sample);
        instant_display = p10_apply_stock_threshold_corrections(instant_display);
        reading->instant_value = instant_display;
        g_p10_diag.instant_value = instant_display;
        g_p10_diag.display_value = instant_display;
        if (p10_range_controller_fast_service(instant_display, reading)) {
            return true;
        }
    }
    g_p10_display_accum += raw_sample;
    ++g_p10_display_count;
    if (g_p10_display_count < 16u) {
        reading->display_ready = false;
        return true;
    }

    int64_t avg = g_p10_display_accum < 0 ? -((-g_p10_display_accum) >> 4) :
                                             (g_p10_display_accum >> 4);
    if (avg > 0x7FFFFFFF) {
        avg = 0x7FFFFFFF;
    } else if (avg < (-0x7FFFFFFF - 1LL)) {
        avg = -0x7FFFFFFF - 1LL;
    }
    p10_reset_average();
    g_p10_diag.avg_sample = (int32_t)avg;

    int32_t value = ac_path ? p10_apply_ac_voltage_calibration((int32_t)avg) :
                              p10_apply_calibration((int32_t)avg);
    value = p10_apply_stock_threshold_corrections(value);
    if (ac_path) {
        value = p10_smooth_with_params(value, 20u, 10u);
    } else if (g_p10_mode == DMM_MODE_MV_DC || g_p10_mode == DMM_MODE_TEMP) {
        value = p10_smooth_with_params(value, 5u, 10u);
    } else {
        value = p10_smooth_with_params(value, 2u, 2u);
    }

    if (g_p10_mode == DMM_MODE_TEMP) {
        int32_t tenths_c = 0;
        g_temperature_typek_input_emf = g_temperature_cold_junction_emf + value;
        if (!stock_typek_tenths_from_emf(g_temperature_typek_input_emf, &tenths_c)) {
            reading->has_sample = true;
            reading->display_ready = true;
            reading->overrange = true;
            g_p10_overrange_latched = true;
            return true;
        }
        value = tenths_c;
    }

    reading->display_value = value;
    g_p10_diag.last_value = value;

    if (g_p10_settle_samples != 0u) {
        --g_p10_settle_samples;
        reading->display_ready = false;
        reading->overrange = false;
        return true;
    }

    if (p10_lowz_stock_path_service(value, reading)) {
        reading->has_sample = true;
        reading->range = g_p10_range;
        reading->range_changed = true;
        reading->overrange = false;
        return true;
    }

    if (g_p10_mode == DMM_MODE_V_AC && g_voltage_alt_bank_active) {
        return p10_voltage_alt_accept_average(value, reading);
    }

    if (p10_apply_autorange(value)) {
        reading->range_changed = true;
        reading->range = g_p10_range;
        reading->display_ready = false;
        reading->overrange = false;
        return true;
    }

    reading->overrange = g_p10_overrange_latched;
    reading->display_ready = true;
    return true;
}

static uint16_t p10_continuity_threshold(void) {
    if (g_p10_continuity_beeping) {
        if (g_p10_continuity_option == 0u) {
            return 3500u;
        }
        if (g_p10_continuity_option == 1u) {
            return 2500u;
        }
        if (g_p10_continuity_option == 2u) {
            return 30500u;
        }
        return 29500u;
    }
    return g_p10_continuity_option < 2u ? 3000u : 30000u;
}

static bool p10_continuity_should_beep(int32_t value, uint16_t threshold) {
    if (g_p10_continuity_option == 1u || g_p10_continuity_option == 3u) {
        return value >= (int32_t)threshold;
    }
    return value <= (int32_t)threshold;
}

uint16_t dmm_p10_continuity_threshold_value(void) {
    return p10_continuity_threshold();
}

bool dmm_p10_continuity_beeping(void) {
    return g_p10_continuity_beeping;
}

static bool p10_raw_sample_prebeep_should_fire(int32_t raw_sample) {
    if (g_p10_mode == DMM_MODE_P10_DIODE) {
        int32_t min_value = DIODE_BEEP_MIN;
        int32_t max_value = DIODE_BEEP_MAX;
        if (g_p10_diode_option_15) {
            min_value *= 10;
            max_value *= 10;
        }
        return raw_sample < max_value && raw_sample >= min_value;
    }
    if (g_p10_mode == DMM_MODE_P10_CONT) {
        int32_t estimate = raw_sample - g_p10_zero_offset;
        if (estimate < 0) {
            estimate = 0;
        }
        return p10_continuity_should_beep(estimate, p10_continuity_threshold());
    }
    return false;
}

static void p10_raw_sample_prebeep(int32_t raw_sample) {
    if (p10_raw_sample_prebeep_should_fire(raw_sample)) {
        buzzer_force_on_ms(P10_FAST_PREBEEP_MS);
    }
}

void dmm_p10_enter_mode(uint8_t mode, uint8_t range) {
    if (!((mode >= DMM_MODE_P10_OHMS && mode <= DMM_MODE_P10_CAP) ||
          dmm_mode_is_voltage(mode) ||
          mode == DMM_MODE_TEMP ||
          dmm_mode_is_freq_group(mode) ||
          dmm_mode_is_current(mode))) {
        mode = DMM_MODE_P10_DIODE;
    }
    if (mode == DMM_MODE_P10_OHMS && range >= 7u) {
        range = 6u;
    } else if (mode == DMM_MODE_P10_CAP && range >= 6u) {
        range = 0u;
    } else if (dmm_mode_is_voltage(mode) || mode == DMM_MODE_TEMP ||
               dmm_mode_is_freq_group(mode)) {
        uint8_t limit = p10_range_limit(mode);
        if (range >= limit) {
            range = 0u;
        }
    } else if (dmm_mode_is_current(mode)) {
        uint8_t limit = p10_range_limit(mode);
        if (range >= limit) {
            range = 0u;
        }
    } else if (mode == DMM_MODE_P10_CONT || mode == DMM_MODE_P10_DIODE) {
        range = 0u;
    }

    if (mode != g_p10_mode) {
        g_p10_cap_force_low_range_after_timeout = false;
        if (mode != DMM_MODE_V_AC) {
            g_voltage_alt_bank_active = false;
        }
    }
    g_p10_mode = mode;
    g_p10_range = range;
    g_p10_range_reconfigure_pending = false;
    p10_reset_measurement_transients();
    p10_measure_phase_reset_for_mode();
    p10_apply_current_mode_range_config();
}

bool dmm_p10_range_reconfigure_pending(void) {
    return g_p10_range_reconfigure_pending;
}

bool dmm_p10_poll(dmm_reading_t *reading) {
    if (reading == 0) {
        return false;
    }

    reading->mode = g_p10_mode;
    reading->range = g_p10_range;
    reading->status = 0;
    reading->has_sample = false;
    reading->display_ready = false;
    reading->range_changed = false;
    reading->overrange = g_p10_overrange_latched;
    reading->instant_value = 0;
    reading->display_value = 0;
    g_p10_diag.display_count = g_p10_display_count;
    g_p10_diag.settle_samples = g_p10_settle_samples;
    g_p10_diag.autorange_direction = g_p10_autorange_direction;

    bool cap_mode = g_p10_mode == DMM_MODE_P10_CAP;
    p10_frequency_secondary_timeout_service();
    if (cap_mode) {
        p10_cap_timing_service();
    }
    if (!frontend_ready()) {
        g_p10_diag.flags &= (uint8_t)~0x01u;
        if (cap_mode && g_p10_range < 5u && p10_handle_cap_timeout(reading)) {
            return true;
        }
        return false;
    }
    g_p10_diag.flags |= 0x01u;

    frontend_read_status(g_frontend_status);
    uint8_t status = (uint8_t)(g_frontend_status[0x1Eu] & g_frontend_status[0x1Fu]);
    g_p10_diag.status = status;
    g_p10_diag.reg1e = g_frontend_status[0x1Eu];
    g_p10_diag.reg1f = g_frontend_status[0x1Fu];
    g_p10_diag.reg20 = g_frontend_status[0x20u];
    g_p10_diag.reg29 = g_frontend_status[0x29u];
    g_p10_diag.range = g_p10_range;
    g_p10_diag.mode = g_p10_mode;
    reading->status = status;
    if (cap_mode && g_p10_cap_frontend_sync_pending) {
        /*
         * Stock consumes CAP range changes in the main loop before arming the
         * next timeout. In this task build, stale frames from the previous
         * frontend range can otherwise spend the timeout and jump to uF.
         */
        if ((status & 0x01u) == 0u ||
            g_frontend_status[0x20u] != frontend_configured_reg20()) {
            frontend_restore_configured_reg20_shadow();
            return true;
        }
        g_p10_cap_frontend_sync_pending = false;
    }
    if ((status & 0x01u) != 0u) {
        (void)p10_decode_frequency_secondary(true, false);
    }
    p10_frontend_timing_idle_service();
    if (!p10_status_has_sample(status)) {
        if (cap_mode && g_p10_range < 5u) {
            (void)p10_handle_cap_timeout(reading);
        }
        return true;
    }

    int32_t value = 0;
    if (g_p10_mode == DMM_MODE_P10_CAP) {
        if (!p10_decode_cap_value(&value)) {
            return true;
        }
    } else if (dmm_mode_is_freq_group(g_p10_mode)) {
        if (!p10_decode_freq_group_value(reading, &value)) {
            return true;
        }
    } else {
        int32_t raw_sample = frontend_decode_signed24_sample(g_frontend_status);
        g_p10_diag.raw_sample = raw_sample;
        p10_measure_phase_note_sample();
        p10_raw_sample_prebeep(raw_sample);
        if (p10_voltage_process_raw_sample(raw_sample, reading)) {
            return true;
        }
        if (p10_current_single_process_raw_sample(raw_sample, reading)) {
            return true;
        }
        value = p10_apply_calibration(raw_sample);
        if (g_p10_mode == DMM_MODE_P10_DIODE && g_p10_diode_option_15) {
            value /= 10;
        }
    }
    reading->has_sample = true;
    reading->instant_value = value;
    reading->display_value = value;
    g_p10_diag.instant_value = value;
    g_p10_diag.last_value = value;

    if (g_p10_mode == DMM_MODE_P10_DIODE) {
        g_p10_overrange_latched = p10_current_range_overrange(value);
        if (value < DIODE_BEEP_MAX && value >= DIODE_BEEP_MIN) {
            buzzer_force_on_ms(P10_DIODE_BEEP_MS);
        } else {
            buzzer_off();
        }
        reading->overrange = g_p10_overrange_latched;
    } else if (g_p10_mode == DMM_MODE_P10_CONT) {
        g_p10_overrange_latched = p10_current_range_overrange(value);
        reading->overrange = g_p10_overrange_latched;
        uint16_t threshold = p10_continuity_threshold();
        g_p10_continuity_beeping = p10_continuity_should_beep(value, threshold);
        if (g_p10_continuity_beeping) {
            buzzer_force_on_ms(P10_CONTINUITY_BEEP_MS);
        } else {
            buzzer_off();
        }
    } else if (g_p10_mode == DMM_MODE_P10_OHMS) {
        if (!p10_ohms_stock_logic_enabled() && p10_current_range_overrange(value)) {
            g_p10_overrange_latched = true;
            reading->overrange = true;
        } else if (p10_ohms_fast_downrange(value)) {
            reading->range_changed = true;
            reading->range = g_p10_range;
            reading->overrange = false;
            reading->display_ready = false;
            return true;
        } else if (p10_range_controller_fast_service(value, reading)) {
            return true;
        }
    }

    if (g_p10_settle_samples != 0u &&
        !p10_current_single_main_stock_mode(g_p10_mode)) {
        --g_p10_settle_samples;
        if (g_p10_mode == DMM_MODE_P10_OHMS &&
            g_p10_measure_phase == DMM_MEASURE_PHASE_SETTLE) {
            g_p10_phase_settle_remaining = g_p10_settle_samples;
            if (g_p10_phase_settle_remaining == 0u) {
                p10_measure_phase_enter(DMM_MEASURE_PHASE_PRECISION);
            }
        }
        p10_reset_average();
        return true;
    }

    bool autorange_after_average = g_p10_mode == DMM_MODE_P10_OHMS && p10_ohms_stock_logic_enabled();
    int32_t display_value = dmm_mode_is_freq_group(g_p10_mode) ? value :
                            (autorange_after_average ? value : p10_condition_display_value(value));
    g_p10_diag.display_value = display_value;

    if (!dmm_mode_is_freq_group(g_p10_mode) &&
        !(g_p10_mode == DMM_MODE_V_AC && g_voltage_alt_bank_active) &&
        !p10_current_single_main_stock_mode(g_p10_mode) &&
        !dmm_mode_is_dual_current(g_p10_mode) &&
        !autorange_after_average &&
        p10_apply_autorange(display_value)) {
        bool ohms_upshift = g_p10_mode == DMM_MODE_P10_OHMS && g_p10_autorange_direction > 0;
        reading->range_changed = true;
        reading->range = g_p10_range;
        reading->overrange = ohms_upshift;
        reading->display_ready = false;
        return true;
    }

    g_p10_display_accum += display_value;
    ++g_p10_display_count;
    if (g_p10_display_count >= p10_average_limit()) {
        int64_t accum = g_p10_display_accum;
        uint8_t average_count = g_p10_display_count;
        int64_t averaged = accum;
        if (average_count == 16u) {
            averaged = accum < 0 ? -((-accum) >> 4) : (accum >> 4);
        } else if (average_count == 8u) {
            averaged = accum < 0 ? -((-accum) >> 3) : (accum >> 3);
        }
        if (averaged > 0x7FFFFFFF) {
            averaged = 0x7FFFFFFF;
        } else if (averaged < (-0x7FFFFFFF - 1LL)) {
            averaged = -0x7FFFFFFF - 1LL;
        }
        reading->display_value = (int32_t)averaged;
        if (autorange_after_average) {
            reading->display_value = p10_condition_display_value(reading->display_value);
        }
        g_p10_diag.display_value = reading->display_value;
        p10_reset_average();
        if (p10_current_single_accept_average(reading->display_value, reading)) {
            return true;
        }
        if (p10_voltage_alt_accept_average(reading->display_value, reading)) {
            return true;
        }
        if (p10_current_dual_accept_average(reading->display_value, reading)) {
            return true;
        }
        if (autorange_after_average && p10_apply_autorange(reading->display_value)) {
            bool ohms_upshift = g_p10_mode == DMM_MODE_P10_OHMS && g_p10_autorange_direction > 0;
            reading->range_changed = true;
            reading->range = g_p10_range;
            reading->overrange = ohms_upshift;
            reading->display_ready = false;
            return true;
        }
        if (autorange_after_average && p10_autorange_display_blank_active()) {
            reading->display_ready = false;
            reading->overrange = g_p10_overrange_latched;
            return true;
        }
        reading->display_ready = true;
        reading->overrange = g_p10_overrange_latched;
        p10_measure_phase_note_display_ready();
    }

    return true;
}

void dmm_p10_get_diag(dmm_p10_diag_t *diag) {
    if (diag != 0) {
        uint8_t stock_flags = 0;
        if (g_p10_cap_timing_state == P10_CAP_TIMING_TIMEOUT_PENDING) {
            stock_flags |= 0x01u;
        }
        if (g_p10_display_count != 0u) {
            stock_flags |= 0x04u;
        }
        if (g_p10_overrange_latched) {
            stock_flags |= 0x08u;
        }
        if (!g_p10_autorange) {
            stock_flags |= 0x10u;
        }
        if (g_p10_cap_force_low_range_after_timeout) {
            stock_flags |= 0x20u;
        }
        if (frontend_cap_option_table_enabled()) {
            stock_flags |= 0x40u;
        }
        uint8_t substate_flags = 0;
        if (g_voltage_alt_bank_active) {
            substate_flags |= 0x01u;
        }
        if (g_voltage_alt_primary_path) {
            substate_flags |= 0x02u;
        }
        if (g_lowz_alt_path) {
            substate_flags |= 0x10u;
        }
        if (g_p10_frontend_timing_phase) {
            substate_flags |= 0x20u;
        }
        if (!g_frontend_timing_toggle_enabled) {
            substate_flags |= 0x40u;
        }

        diag->mode = g_p10_diag.mode;
        diag->status = g_p10_diag.status;
        diag->reg1e = g_p10_diag.reg1e;
        diag->reg1f = g_p10_diag.reg1f;
        diag->reg20 = g_p10_diag.reg20;
        diag->reg29 = g_p10_diag.reg29;
        diag->range = g_p10_diag.range;
        diag->flags = g_p10_diag.flags;
        diag->cap_events = g_p10_diag.cap_events;
        diag->cap_timeouts = g_p10_diag.cap_timeouts;
        diag->cap_ref_lsb = g_p10_diag.cap_ref_lsb;
        diag->cap_count_lsb = g_p10_diag.cap_count_lsb;
        diag->stock_flags = stock_flags;
        diag->substate_flags = substate_flags;
        diag->cap_holdoff = g_p10_settle_samples;
        diag->cap_timing_state = (uint8_t)g_p10_cap_timing_state;
        diag->cap_raw = g_p10_diag.cap_raw;
        diag->zero_offset = (uint32_t)g_p10_zero_offset;
        diag->gain = g_p10_gain;
        diag->gain_scale = g_p10_gain_scale;
        diag->cap_ref = g_p10_diag.cap_ref;
        diag->cap_count = g_p10_diag.cap_count;
        diag->cap_wrap = g_p10_diag.cap_wrap;
        diag->cap_elapsed = g_p10_diag.cap_elapsed;
        diag->frontend_timing_word = g_p10_frontend_timing_word;
        diag->cap_timing_word = g_p10_diag.cap_timing_word;
        diag->cap_timeout_ticks = g_p10_diag.cap_timeout_ticks;
        diag->cap_timeout_counter = g_p10_cap_timeout_counter;
        diag->raw_sample = g_p10_diag.raw_sample;
        diag->avg_sample = g_p10_diag.avg_sample;
        diag->temp_cold_junction_emf = g_temperature_cold_junction_emf;
        diag->temp_typek_input_emf = g_temperature_typek_input_emf;
        diag->instant_value = g_p10_diag.instant_value;
        diag->display_value = g_p10_diag.display_value;
        diag->last_value = g_p10_diag.last_value;
        diag->autorange_value = g_p10_diag.autorange_value;
        diag->autorange_abs = g_p10_diag.autorange_abs;
        diag->autorange_low = g_p10_diag.autorange_low;
        diag->autorange_high = g_p10_diag.autorange_high;
        diag->frequency_secondary_raw = g_frequency_secondary_raw;
        diag->frequency_secondary_value = g_frequency_secondary_value;
        diag->frequency_secondary_range = g_frequency_secondary_range;
        diag->frequency_secondary_flags = (g_frequency_secondary_valid ? 0x01u : 0u) |
                                          (g_frequency_secondary_overrange ? 0x02u : 0u);
        diag->autorange_from = g_p10_diag.autorange_from;
        diag->autorange_to = g_p10_diag.autorange_to;
        diag->autorange_decision = g_p10_diag.autorange_decision;
        diag->display_count = g_p10_display_count;
        diag->settle_samples = g_p10_settle_samples;
        diag->autorange_direction = g_p10_autorange_direction;
        diag->autorange_holdoff = g_p10_autorange_holdoff;
        diag->measure_phase = g_p10_measure_phase;
        diag->phase_settle_remaining = g_p10_phase_settle_remaining;
        diag->phase_transition_count = g_p10_phase_transition_count;
        diag->phase_range_steps = g_p10_phase_range_steps;
        diag->phase_age_ms = clock_millis() - g_p10_phase_entered_at;
        diag->first_precision_ms = g_p10_first_precision_ms;
        diag->phase_search_events = g_p10_phase_search_events;
        diag->phase_settle_events = g_p10_phase_settle_events;
        diag->phase_precision_events = g_p10_phase_precision_events;
    }
}
