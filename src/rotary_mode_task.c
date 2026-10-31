#include "rotary_mode_task.h"

#include "calibration.h"
#include "dmm.h"

#define ROTARY_MODE_LOWZ 0x01u
#define ROTARY_MODE_V 0x02u
#define ROTARY_MODE_MV_TEMP 0x04u
#define ROTARY_MODE_FREQ 0x08u
#define ROTARY_MODE_P10 0x10u
#define ROTARY_MODE_UA 0x20u
#define ROTARY_MODE_MA 0x40u
#define ROTARY_MODE_A 0x80u

#define ROTARY_MODE_CAL_ROT2 8u
#define ROTARY_MODE_CAL_ROT4 9u
#define ROTARY_MODE_CAL_ROT8 10u
#define ROTARY_MODE_CAL_ROT10 11u
#define ROTARY_MODE_CAL_ROT20 12u
#define ROTARY_MODE_CAL_ROT40 13u
#define ROTARY_MODE_CAL_ROT80 14u

static bool mode_is_rot2(uint8_t mode) {
    return mode == DMM_MODE_V_DC || mode == DMM_MODE_V_AC;
}

static bool mode_is_rot4(uint8_t mode) {
    return mode == DMM_MODE_MV_DC || mode == DMM_MODE_MV_AC || mode == DMM_MODE_TEMP;
}

static bool mode_is_rot8(uint8_t mode) {
    return mode == DMM_MODE_FREQ || mode == DMM_MODE_PERIOD || mode == DMM_MODE_DUTY;
}

static bool mode_is_rot10(uint8_t mode) {
    return mode == DMM_MODE_P10_OHMS || mode == DMM_MODE_P10_CONT ||
           mode == DMM_MODE_P10_DIODE || mode == DMM_MODE_P10_CAP;
}

static bool mode_is_rot20(uint8_t mode) {
    return mode == DMM_MODE_MVA_DC || mode == DMM_MODE_MVA_AC;
}

static bool mode_is_rot40(uint8_t mode) {
    return mode == DMM_MODE_MA_DC || mode == DMM_MODE_MA_AC ||
           mode == DMM_MODE_MA_AUX || mode == DMM_MODE_MA_BURDEN;
}

static bool mode_is_rot80(uint8_t mode) {
    return mode == DMM_MODE_A_DC || mode == DMM_MODE_A_AC ||
           mode == DMM_MODE_A_DC_PERSIST || mode == DMM_MODE_A_AC_PERSIST;
}

static bool persist_if_changed(uint32_t offset, uint8_t value) {
    if (calibration_read_u8(offset) == value) {
        return true;
    }
    return calibration_write_u8(offset, value);
}

bool rotary_mode_task_is_mode_for_rotary(uint8_t rotary, uint8_t mode) {
    if (rotary == ROTARY_MODE_LOWZ) {
        return mode == DMM_MODE_LOZ;
    }
    if (rotary == ROTARY_MODE_V) {
        return mode_is_rot2(mode);
    }
    if (rotary == ROTARY_MODE_MV_TEMP) {
        return mode_is_rot4(mode);
    }
    if (rotary == ROTARY_MODE_FREQ) {
        return mode_is_rot8(mode);
    }
    if (rotary == ROTARY_MODE_P10) {
        return mode_is_rot10(mode);
    }
    if (rotary == ROTARY_MODE_UA) {
        return mode_is_rot20(mode);
    }
    if (rotary == ROTARY_MODE_MA) {
        return mode_is_rot40(mode);
    }
    if (rotary == ROTARY_MODE_A) {
        return mode_is_rot80(mode);
    }
    return false;
}

uint8_t rotary_mode_task_default(uint8_t rotary) {
    if (rotary == ROTARY_MODE_LOWZ) {
        return DMM_MODE_LOZ;
    }
    if (rotary == ROTARY_MODE_V) {
        uint8_t saved = calibration_read_u8(ROTARY_MODE_CAL_ROT2);
        if (saved == 0x82u) {
            dmm_p10_voltage_set_alt_active(true);
            return DMM_MODE_V_AC;
        }
        dmm_p10_voltage_set_alt_active(false);
        return mode_is_rot2(saved) ? saved : DMM_MODE_V_DC;
    }
    if (rotary == ROTARY_MODE_MV_TEMP) {
        uint8_t saved = calibration_read_u8(ROTARY_MODE_CAL_ROT4);
        return mode_is_rot4(saved) ? saved : DMM_MODE_MV_DC;
    }
    if (rotary == ROTARY_MODE_FREQ) {
        uint8_t saved = calibration_read_u8(ROTARY_MODE_CAL_ROT8);
        return mode_is_rot8(saved) ? saved : DMM_MODE_FREQ;
    }
    if (rotary == ROTARY_MODE_P10) {
        uint8_t saved = calibration_read_u8(ROTARY_MODE_CAL_ROT10);
        return mode_is_rot10(saved) ? saved : DMM_MODE_P10_OHMS;
    }
    if (rotary == ROTARY_MODE_UA) {
        uint8_t saved = calibration_read_u8(ROTARY_MODE_CAL_ROT20);
        return mode_is_rot20(saved) ? saved : DMM_MODE_MVA_DC;
    }
    if (rotary == ROTARY_MODE_MA) {
        uint8_t saved = calibration_read_u8(ROTARY_MODE_CAL_ROT40);
        return mode_is_rot40(saved) ? saved : DMM_MODE_MA_DC;
    }
    if (rotary == ROTARY_MODE_A) {
        uint8_t saved = calibration_read_u8(ROTARY_MODE_CAL_ROT80);
        if (saved == DMM_MODE_A_DC || saved == DMM_MODE_A_AC_PERSIST) {
            return DMM_MODE_A_DC;
        }
        if (saved == DMM_MODE_A_AC || saved == DMM_MODE_A_DC_PERSIST) {
            return DMM_MODE_A_AC;
        }
        return DMM_MODE_A_DC;
    }
    return DMM_MODE_P10_OHMS;
}

uint8_t rotary_mode_task_cycle(uint8_t mode) {
    if (mode == DMM_MODE_MV_DC) {
        return DMM_MODE_MV_AC;
    }
    if (mode == DMM_MODE_MV_AC) {
        return DMM_MODE_TEMP;
    }
    if (mode == DMM_MODE_TEMP) {
        return DMM_MODE_MV_DC;
    }
    if (mode == DMM_MODE_FREQ) {
        return DMM_MODE_PERIOD;
    }
    if (mode == DMM_MODE_PERIOD) {
        return DMM_MODE_DUTY;
    }
    if (mode == DMM_MODE_DUTY) {
        return DMM_MODE_FREQ;
    }
    if (mode == DMM_MODE_P10_OHMS) {
        return DMM_MODE_P10_CONT;
    }
    if (mode == DMM_MODE_P10_CONT) {
        return DMM_MODE_P10_DIODE;
    }
    if (mode == DMM_MODE_P10_DIODE) {
        return DMM_MODE_P10_CAP;
    }
    if (mode == DMM_MODE_P10_CAP) {
        return DMM_MODE_P10_OHMS;
    }
    if (mode == DMM_MODE_MVA_AC) {
        return DMM_MODE_MVA_DC;
    }
    if (mode == DMM_MODE_MVA_DC) {
        return DMM_MODE_MVA_AC;
    }
    if (mode == DMM_MODE_MA_DC) {
        return DMM_MODE_MA_AC;
    }
    if (mode == DMM_MODE_MA_AC) {
        return DMM_MODE_MA_BURDEN;
    }
    if (mode == DMM_MODE_MA_BURDEN) {
        return DMM_MODE_MA_AUX;
    }
    if (mode == DMM_MODE_MA_AUX) {
        return DMM_MODE_MA_DC;
    }
    if (mode == DMM_MODE_LEGACY_CURRENT_AC) {
        return DMM_MODE_MA_ALT_AC;
    }
    if (mode == DMM_MODE_MA_ALT_AC) {
        return DMM_MODE_LEGACY_CURRENT_AC;
    }
    if (mode == DMM_MODE_A_AC) {
        return DMM_MODE_A_DC;
    }
    if (mode == DMM_MODE_A_DC) {
        return DMM_MODE_A_AC;
    }
    if (mode == DMM_MODE_A_DC_PERSIST) {
        return DMM_MODE_A_AC_PERSIST;
    }
    if (mode == DMM_MODE_A_AC_PERSIST) {
        return DMM_MODE_A_DC_PERSIST;
    }
    return mode;
}

bool rotary_mode_task_persist(uint8_t rotary, uint8_t mode) {
    if (rotary == ROTARY_MODE_V) {
        uint8_t saved = mode;
        if (mode == DMM_MODE_V_AC && dmm_p10_voltage_alt_active()) {
            saved = 0x82u;
        }
        if (mode_is_rot2(saved) || saved == 0x82u) {
            return persist_if_changed(ROTARY_MODE_CAL_ROT2, saved);
        }
        return false;
    }
    if (rotary == ROTARY_MODE_MV_TEMP && mode_is_rot4(mode)) {
        return persist_if_changed(ROTARY_MODE_CAL_ROT4, mode);
    }
    if (rotary == ROTARY_MODE_FREQ && mode_is_rot8(mode)) {
        return persist_if_changed(ROTARY_MODE_CAL_ROT8, mode);
    }
    if (rotary == ROTARY_MODE_P10 && mode_is_rot10(mode)) {
        return persist_if_changed(ROTARY_MODE_CAL_ROT10, mode);
    }
    if (rotary == ROTARY_MODE_UA && mode_is_rot20(mode)) {
        return persist_if_changed(ROTARY_MODE_CAL_ROT20, mode);
    }
    if (rotary == ROTARY_MODE_MA && mode_is_rot40(mode)) {
        return persist_if_changed(ROTARY_MODE_CAL_ROT40, mode);
    }
    if (rotary == ROTARY_MODE_A && mode_is_rot80(mode)) {
        return persist_if_changed(ROTARY_MODE_CAL_ROT80, mode);
    }
    return false;
}
