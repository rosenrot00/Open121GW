#ifndef EEVBLOG_DMM_H
#define EEVBLOG_DMM_H

#include <stdbool.h>
#include <stdint.h>

#define DMM_MODE_LOZ      0u
#define DMM_MODE_V_DC     1u
#define DMM_MODE_V_AC     2u
#define DMM_MODE_MV_DC    3u
#define DMM_MODE_MV_AC    4u
#define DMM_MODE_TEMP     5u
#define DMM_MODE_V_ACDC   25u
#define DMM_MODE_P10_OHMS  9u
#define DMM_MODE_P10_CONT  10u
#define DMM_MODE_P10_DIODE 11u
#define DMM_MODE_P10_CAP   12u
#define DMM_MODE_MA_AUX    13u
#define DMM_MODE_LEGACY_CURRENT_AC 14u
#define DMM_MODE_MVA_AC 15u
#define DMM_MODE_MA_AC     16u
#define DMM_MODE_MA_DC     17u
#define DMM_MODE_A_AC      18u
#define DMM_MODE_A_DC      19u
#define DMM_MODE_A_DC_PERSIST 20u
#define DMM_MODE_A_AC_PERSIST 21u
#define DMM_MODE_MA_BURDEN 22u
#define DMM_MODE_MA_ALT_AC 23u
#define DMM_MODE_MVA_DC 24u
#define DMM_MODE_FREQ      6u
#define DMM_MODE_PERIOD    7u
#define DMM_MODE_DUTY      8u

#define DMM_MEASURE_PHASE_SEARCH    0u
#define DMM_MEASURE_PHASE_SETTLE    1u
#define DMM_MEASURE_PHASE_PRECISION 2u
#define DMM_MEASURE_PHASE_FAULT     3u

typedef struct {
    uint8_t mode;
    uint8_t range;
    uint8_t status;
    bool has_sample;
    bool display_ready;
    bool range_changed;
    bool overrange;
    int32_t instant_value;
    int32_t display_value;
} dmm_reading_t;

typedef struct {
    uint8_t mode;
    uint8_t status;
    uint8_t reg1e;
    uint8_t reg1f;
    uint8_t reg20;
    uint8_t reg29;
    uint8_t range;
    uint8_t flags;
    uint8_t cap_events;
    uint8_t cap_timeouts;
    uint8_t cap_ref_lsb;
    uint8_t cap_count_lsb;
    uint8_t stock_flags;
    uint8_t substate_flags;
    uint8_t cap_holdoff;
    uint8_t cap_timing_state;
    uint32_t cap_raw;
    uint32_t zero_offset;
    uint32_t gain;
    uint32_t gain_scale;
    uint32_t cap_ref;
    uint32_t cap_count;
    uint32_t cap_wrap;
    uint32_t cap_elapsed;
    uint16_t frontend_timing_word;
    uint16_t cap_timing_word;
    uint16_t cap_timeout_ticks;
    uint16_t cap_timeout_counter;
    int32_t raw_sample;
    int32_t avg_sample;
    int32_t temp_cold_junction_emf;
    int32_t temp_typek_input_emf;
    int32_t instant_value;
    int32_t display_value;
    int32_t last_value;
    int32_t autorange_value;
    uint32_t autorange_abs;
    uint32_t autorange_low;
    uint32_t autorange_high;
    uint32_t frequency_secondary_raw;
    int32_t frequency_secondary_value;
    uint8_t frequency_secondary_range;
    uint8_t frequency_secondary_flags;
    uint8_t autorange_from;
    uint8_t autorange_to;
    uint8_t autorange_decision;
    uint8_t display_count;
    uint8_t settle_samples;
    int8_t autorange_direction;
    uint8_t autorange_holdoff;
    uint8_t measure_phase;
    uint8_t phase_settle_remaining;
    uint16_t phase_transition_count;
    uint16_t phase_range_steps;
    uint32_t phase_age_ms;
    uint32_t first_precision_ms;
    uint32_t phase_search_events;
    uint32_t phase_settle_events;
    uint32_t phase_precision_events;
} dmm_p10_diag_t;

void dmm_init(void);
void dmm_poll(void);
bool dmm_fast_diode_enabled(void);
uint8_t dmm_p10_cycle_mode(uint8_t mode);
uint8_t dmm_p10_default_range(uint8_t mode);
uint8_t dmm_p10_next_range(uint8_t mode, uint8_t range);
uint8_t dmm_p10_decimal_position(uint8_t mode, uint8_t range);
void dmm_p10_set_autorange(bool enabled);
bool dmm_p10_autorange_enabled(void);
void dmm_p10_set_ohms_stock_guard(bool enabled);
bool dmm_p10_ohms_stock_guard_enabled(void);
void dmm_p10_set_task_fast_range(bool enabled);
bool dmm_p10_task_fast_range_enabled(void);
void dmm_p10_set_continuity_option(uint8_t option);
uint8_t dmm_p10_continuity_option(void);
uint16_t dmm_p10_continuity_threshold_value(void);
bool dmm_p10_continuity_beeping(void);
void dmm_p10_toggle_diode_option(void);
bool dmm_p10_diode_option_15(void);
bool dmm_p10_voltage_alt_active(void);
void dmm_p10_voltage_set_alt_active(bool enabled);
int32_t dmm_p10_voltage_alt_secondary_value(void);
bool dmm_p10_frequency_secondary_active(void);
bool dmm_p10_frequency_secondary_valid(void);
bool dmm_p10_frequency_secondary_overrange(void);
int32_t dmm_p10_frequency_secondary_display_value(void);
uint8_t dmm_p10_frequency_secondary_range(void);
uint8_t dmm_p10_frequency_secondary_decimal(void);
int32_t dmm_p10_current_single_aux_value(void);
bool dmm_p10_current_single_aux_overrange(void);
bool dmm_p10_cal_reject_unstable_alt_secondary_band(void);
int32_t dmm_p10_current_dual_bank0_value(void);
int32_t dmm_p10_current_dual_bank1_value(void);
uint8_t dmm_p10_current_alt_range0(void);
uint8_t dmm_p10_current_alt_range1(void);
bool dmm_p10_current_dual_bank_active(void);
bool dmm_p10_current_dual_display_ready(void);
void dmm_p10_set_cold_junction_temperature(int16_t tenths_c);
uint32_t dmm_p10_freq_reference_hz(void);
void dmm_p10_set_burden_voltage_enabled(bool enabled);
void dmm_p10_set_frontend_timing_toggle_enabled(bool enabled);
void dmm_p10_enter_mode(uint8_t mode, uint8_t range);
bool dmm_p10_poll(dmm_reading_t *reading);
bool dmm_p10_range_reconfigure_pending(void);
void dmm_p10_get_diag(dmm_p10_diag_t *diag);
void dmm_p10_set_cap_timeout_enabled(bool enabled);

#endif
