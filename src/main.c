#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include "analog.h"
#include "apo_task.h"
#include "app_diag.h"
#include "aux_adc_task.h"
#include "backlight.h"
#include "battery.h"
#include "bluetooth.h"
#include "bluetooth_app_task.h"
#include "bluetooth_meter_task.h"
#include "buzzer.h"
#include "calibration.h"
#include "clock.h"
#include "dmm.h"
#include "display_model.h"
#include "display_task.h"
#include "dual_secondary_task.h"
#include "frontend.h"
#include "gpio.h"
#include "hw.h"
#include "input.h"
#include "input_task.h"
#include "lcd.h"
#include "lead_warning_task.h"
#include "measurement_controller.h"
#include "mem_logging_task.h"
#include "p10_measure_task.h"
#include "rotary_mode_task.h"
#include "sd_card_task.h"
#include "sd_fat.h"
#include "sdio_sd.h"
#include "status_task.h"
#include "temperature.h"
#include "task_runtime.h"
#include "uart_update.h"

#ifndef EEV_LCD_ONLY_TEST
#define EEV_LCD_ONLY_TEST 0
#endif

#ifndef EEV_LCD_TEXT_TEST
#define EEV_LCD_TEXT_TEST 0
#endif

#ifndef EEV_INPUT_TEST
#define EEV_INPUT_TEST 0
#endif

#ifndef EEV_DIODE_FE_TEST
#define EEV_DIODE_FE_TEST 0
#endif

#ifndef EEV_DIODE_MODE_TEST
#define EEV_DIODE_MODE_TEST 0
#endif

#ifndef EEV_BLE_BAUD_SWEEP
#define EEV_BLE_BAUD_SWEEP 0
#endif

#ifndef EEV_BLE_FIXED_38400
#define EEV_BLE_FIXED_38400 0
#endif

#ifndef EEV_BLE_FIXED_19200
#define EEV_BLE_FIXED_19200 0
#endif

#ifndef EEV_UART_PIN_TEST
#define EEV_UART_PIN_TEST 0
#endif

#ifndef EEV_BLE_LINK_TEST
#define EEV_BLE_LINK_TEST 0
#endif

#define BUTTON_MODE_MASK 0x01u
#define BUTTON_SETUP_MASK 0x02u
#define BUTTON_MINMAX_MASK 0x04u
#define BUTTON_MEM_MASK 0x08u
#define BUTTON_RANGE_MASK 0x10u
#define BUTTON_HOLD_MASK 0x20u
#define BUTTON_REL_MASK 0x40u
#define BUTTON_UP_MASK BUTTON_REL_MASK
#define BUTTON_DOWN_MASK 0x80u
#define LOWZ_ROTARY_MASK 0x01u
#define V_ROTARY_MASK  0x02u
#define MV_TEMP_ROTARY_MASK 0x04u
#define FREQ_ROTARY_MASK 0x08u
#define P10_ROTARY_MASK 0x10u
#define UA_ROTARY_MASK 0x20u
#define MA_ROTARY_MASK 0x40u
#define A_ROTARY_MASK 0x80u
#define P10_CONTINUITY_BEEP_MAX 3000
#define MODE_LONG_PRESS_MS 700u
#define MODE_SHORT_PRESS_MIN_MS 30u
#define MINMAX_LONG_PRESS_MS 700u
#define RANGE_LONG_PRESS_MS 700u
#define PEAK_BT_LONG_PRESS_MS 700u
#define SETUP_LONG_PRESS_MS 700u
#define SETUP_TIMEOUT_MS 8000u
#define SETUP_EDIT_BLINK_MS 300u
#define OHMS_INITIAL_OPEN_HOLDOFF_MS 350u
#define OHMS_RANGE_BLANK_MS 250u
#define P10_LCD_UPDATE_MS 250u
#define P10_BARGRAPH_UPDATE_MS 80u
#define OHMS_INITIAL_LOW_CONFIRM 5000
#define HOLD_LOW_ZONE_COUNTS 1000
#define HOLD_RECAPTURE_DELTA_COUNTS 400
#define HOLD_STABLE_DELTA_COUNTS 20
#define TIM2_BASE 0x40000000u
#define TIM3_BASE 0x40000400u
#define TIM_CR1(base) ((base) + 0x00u)
#define PWR_APB1EN BIT(28)
#define PWR_CR_CWUF BIT(2)
#define PWR_CR_VOS_MASK (BIT(11) | BIT(12))
#define SCB_SCR_SLEEPDEEP BIT(2)
#define CAL_OFFSET_LAST_ROT2_MODE 8u
#define CAL_OFFSET_LAST_ROT4_MODE 9u
#define CAL_OFFSET_LAST_ROT8_MODE 10u
#define CAL_OFFSET_LAST_ROT10_MODE 11u
#define CAL_OFFSET_LAST_ROT20_MODE 12u
#define CAL_OFFSET_LAST_ROT40_MODE 13u
#define CAL_OFFSET_LAST_ROT80_MODE 14u
#define CAL_OFFSET_TEMP_UNIT 16u
#define CAL_OFFSET_DIODE_VOLTAGE 18u
#define CAL_OFFSET_CONTINUITY_OPTION 20u
#define CAL_OFFSET_APO 22u
#define CAL_OFFSET_METER_ID 3u
#define CAL_OFFSET_LOG_INTERVAL 24u
#define CAL_OFFSET_LCD_CONTRAST 26u
#define CAL_OFFSET_BURDEN_VOLTAGE 27u
#define CAL_OFFSET_DBM_MODE 28u
#define CAL_OFFSET_FRONTEND_TIMING 29u
#define CAL_OFFSET_BUZZER 30u

volatile uint8_t g_app_diag_rotary;
volatile uint8_t g_app_diag_buttons;
volatile uint8_t g_app_diag_p10_mode;
volatile uint8_t g_app_diag_p10_range;

typedef struct {
    bool temp_fahrenheit;
    bool apo_enabled;
    bool buzzer_enabled;
    bool burden_voltage_enabled;
    bool dbm_enabled;
    bool frontend_timing_enabled;
    uint8_t continuity_option;
    uint8_t lcd_contrast;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint32_t meter_id;
    uint16_t logging_interval_s;
} setup_state_t;

typedef struct {
    const char *function;
    const char *unit;
    int32_t value;
    uint8_t decimal_pos;
    bool has_value;
    bool overrange;
} log_secondary_t;

static void setup_clamp_date(setup_state_t *setup);

typedef enum {
    SETUP_PAGE_TEMP = 0,
    SETUP_PAGE_BATTERY,
    SETUP_PAGE_APO,
    SETUP_PAGE_BUZZER,
    SETUP_PAGE_LCD,
    SETUP_PAGE_YEAR,
    SETUP_PAGE_DATE,
    SETUP_PAGE_TIME,
    SETUP_PAGE_ID,
    SETUP_PAGE_LOG_INTERVAL,
    SETUP_PAGE_FUNCTION,
    SETUP_PAGE_DBM,
    SETUP_PAGE_FRONTEND_TIMING,
    SETUP_PAGE_SECONDARY_B,
    SETUP_PAGE_BURDEN_VOLTAGE,
    SETUP_PAGE_BURDEN_VALUE,
    SETUP_PAGE_COUNT,
} setup_page_t;

static char __attribute__((unused)) hex_digit(uint8_t value) {
    value &= 0x0Fu;
    return (char)(value < 10u ? ('0' + value) : ('A' + value - 10u));
}

static void __attribute__((unused)) format_hex_line(char prefix, uint8_t value, char out[5]) {
    out[0] = prefix;
    out[1] = '-';
    out[2] = hex_digit((uint8_t)(value >> 4));
    out[3] = hex_digit(value);
    out[4] = ' ';
}

static uint8_t bt_clamp_bcd_datetime(uint8_t value, uint8_t min_value, uint8_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void bt_app_apply_f8_datetime(setup_state_t *setup, const uint8_t datetime[6]) {
    setup->second = bt_clamp_bcd_datetime(datetime[0], 0u, 59u);
    setup->minute = bt_clamp_bcd_datetime(datetime[1], 0u, 59u);
    setup->hour = bt_clamp_bcd_datetime(datetime[2], 0u, 23u);
    setup->day = bt_clamp_bcd_datetime(datetime[3], 1u, 31u);
    setup->month = bt_clamp_bcd_datetime(datetime[4], 1u, 12u);
    setup->year = (uint16_t)(2000u + bt_clamp_bcd_datetime(datetime[5], 0u, 99u));
    setup_clamp_date(setup);
}

static void __attribute__((unused)) bluetooth_send_text(const char *text) {
    while (*text != '\0') {
        (void)bluetooth_send_byte((uint8_t)*text);
        ++text;
    }
}

static void __attribute__((unused)) bluetooth_send_hex8(uint8_t value) {
    (void)bluetooth_send_byte((uint8_t)hex_digit((uint8_t)(value >> 4)));
    (void)bluetooth_send_byte((uint8_t)hex_digit(value));
}

static void __attribute__((unused)) bluetooth_send_hex24(uint32_t value) {
    bluetooth_send_hex8((uint8_t)(value >> 16));
    bluetooth_send_hex8((uint8_t)(value >> 8));
    bluetooth_send_hex8((uint8_t)value);
}

static void __attribute__((unused)) bluetooth_send_hex16(uint16_t value) {
    bluetooth_send_hex8((uint8_t)(value >> 8));
    bluetooth_send_hex8((uint8_t)value);
}

static void __attribute__((unused)) bluetooth_send_hex32(uint32_t value) {
    bluetooth_send_hex8((uint8_t)(value >> 24));
    bluetooth_send_hex8((uint8_t)(value >> 16));
    bluetooth_send_hex8((uint8_t)(value >> 8));
    bluetooth_send_hex8((uint8_t)value);
}

static void __attribute__((unused)) bluetooth_send_dec_u32(uint32_t value) {
    char digits[10];
    uint8_t count = 0;

    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && count < sizeof(digits));

    while (count != 0u) {
        (void)bluetooth_send_byte((uint8_t)digits[--count]);
    }
}

static void __attribute__((unused)) bluetooth_send_uart_pin_banner(uint32_t seq, uint32_t now) {
    bluetooth_send_text("UARTPIN seq=");
    bluetooth_send_dec_u32(seq);
    bluetooth_send_text(" ms=");
    bluetooth_send_dec_u32(now);
    bluetooth_send_text(" baud=19200 8N1 tx=PD5 rx=PD6\r\n");
}

static void __attribute__((unused)) bluetooth_send_uart_pin_rx(uint8_t byte) {
    bluetooth_send_text("UARTPIN RX 0x");
    bluetooth_send_hex8(byte);
    bluetooth_send_text(" '");
    if (byte >= 0x20u && byte <= 0x7Eu && byte != (uint8_t)'\\') {
        (void)bluetooth_send_byte(byte);
    } else {
        (void)bluetooth_send_byte((uint8_t)'.');
    }
    bluetooth_send_text("'\r\n");
}

static void __attribute__((unused)) make_ble_link_frame_19(uint8_t out[19]) {
    static const uint8_t fixed_frame[19] = {
        'B', 'L', 'E', ' ', 'E', 'E', 'V', '1', '2', '1',
        ' ', 'T', 'E', 'S', 'T', ' ', 'O', 'K', '\n'
    };
    for (uint8_t i = 0; i < sizeof(fixed_frame); ++i) {
        out[i] = fixed_frame[i];
    }
}

typedef struct {
    const char *name;
    uint32_t pclk_hz;
    uint32_t baud;
} ble_baud_candidate_t;

static void __attribute__((unused)) bluetooth_send_baud_candidate(uint32_t seq,
                                                                  const ble_baud_candidate_t *candidate) {
    bluetooth_send_text("BTEST ");
    bluetooth_send_dec_u32(seq);
    bluetooth_send_text(" ");
    bluetooth_send_text(candidate->name);
    bluetooth_send_text(" p");
    bluetooth_send_dec_u32(candidate->pclk_hz);
    bluetooth_send_text(" b");
    bluetooth_send_dec_u32(candidate->baud);
    bluetooth_send_text("\r\n");
}

static uint8_t __attribute__((unused)) append_text(char out[50], uint8_t pos, const char *text) {
    while (*text != '\0' && pos < 48u) {
        out[pos++] = *text++;
    }
    return pos;
}

static uint8_t __attribute__((unused)) append_dec(char out[50], uint8_t pos, uint32_t value) {
    char digits[10];
    uint8_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && count < sizeof(digits));
    while (count != 0u && pos < 48u) {
        out[pos++] = digits[--count];
    }
    return pos;
}

static void __attribute__((unused)) make_baud_test_50(uint32_t seq,
                                                      const ble_baud_candidate_t *candidate,
                                                      uint8_t out[50]) {
    for (uint8_t i = 0; i < 50u; ++i) {
        out[i] = (uint8_t)' ';
    }
    uint8_t pos = 0;
    pos = append_text((char *)out, pos, "B50 ");
    pos = append_dec((char *)out, pos, seq);
    pos = append_text((char *)out, pos, " ");
    pos = append_text((char *)out, pos, candidate->name);
    pos = append_text((char *)out, pos, " p");
    pos = append_dec((char *)out, pos, candidate->pclk_hz);
    pos = append_text((char *)out, pos, " b");
    (void)append_dec((char *)out, pos, candidate->baud);
    out[48] = (uint8_t)'\r';
    out[49] = (uint8_t)'\n';
}

static void __attribute__((unused)) make_ble_test_19(uint32_t seq, uint8_t out[19]) {
    out[0] = 0xF2u;
    out[1] = (uint8_t)'E';
    out[2] = (uint8_t)'E';
    out[3] = (uint8_t)'V';
    out[4] = 0x19u;
    out[5] = (uint8_t)seq;
    out[6] = (uint8_t)(seq >> 8);
    out[7] = (uint8_t)(seq >> 16);
    out[8] = (uint8_t)(seq >> 24);
    out[9] = 0x32u;
    out[10] = 0x12u;
    out[11] = 0x34u;
    out[12] = 0x56u;
    out[13] = 0x78u;
    out[14] = 0x9Au;
    out[15] = 0xBCu;
    out[16] = 0xDEu;
    out[17] = 0x0Fu;

    uint8_t xor_sum = 0;
    for (uint8_t i = 0; i < 18u; ++i) {
        xor_sum ^= out[i];
    }
    out[18] = xor_sum;
}

static void __attribute__((unused)) put_hex32_ascii(uint8_t *out, uint8_t pos, uint32_t value) {
    for (uint8_t i = 0; i < 8u; ++i) {
        uint8_t shift = (uint8_t)(28u - (i * 4u));
        out[pos + i] = (uint8_t)hex_digit((uint8_t)(value >> shift));
    }
}

static void __attribute__((unused)) put_text_ascii(uint8_t *out, uint8_t pos, const char *text) {
    while (*text != '\0') {
        out[pos++] = (uint8_t)*text++;
    }
}

static void __attribute__((unused)) make_ble_app_ascii_53(uint32_t seq, uint8_t out[53]) {
    out[0] = 0xF2u;
    for (uint8_t i = 1u; i < 53u; ++i) {
        out[i] = (uint8_t)('A' + ((seq + i) & 0x0Fu));
    }

    put_text_ascii(out, 1u, "EEV121GW");
    put_hex32_ascii(out, 9u, seq);
    put_text_ascii(out, 17u, "P16B19200");
    put_text_ascii(out, 26u, "APPFRAME");
    put_hex32_ascii(out, 34u, seq ^ 0xA5A55A5Au);
    put_text_ascii(out, 42u, "END");
}

static bool __attribute__((unused)) bluetooth_send_bytes_paced(const uint8_t *data, uint8_t len) {
    for (uint8_t i = 0; i < len; ++i) {
        if (!bluetooth_send_byte(data[i])) {
            return false;
        }
        if (!bluetooth_wait_tx_complete(300000u)) {
            return false;
        }
        if ((i % 20u) == 19u) {
            delay_ms(8u);
        }
    }
    return true;
}

static void __attribute__((unused)) format_jack_label(uint8_t value, char out[5]) {
    out[0] = 'J';
    out[1] = (value & 0x01u) != 0u ? 'A' : '-';
    out[2] = (value & 0x02u) != 0u ? 'B' : '-';
    out[3] = ' ';
    out[4] = ' ';
}

static void __attribute__((unused)) format_p10_mode_label(uint8_t mode, char out[5]) {
    if (mode == DMM_MODE_LOZ) {
        out[0] = 'L';
        out[1] = 'o';
        out[2] = 'Z';
        out[3] = ' ';
        out[4] = ' ';
    } else if (mode == DMM_MODE_MV_DC) {
        out[0] = 'D';
        out[1] = 'C';
        out[2] = 'm';
        out[3] = 'V';
        out[4] = ' ';
    } else if (mode == DMM_MODE_MV_AC) {
        out[0] = 'A';
        out[1] = 'C';
        out[2] = 'm';
        out[3] = 'V';
        out[4] = ' ';
    } else if (mode == DMM_MODE_TEMP) {
        out[0] = 'T';
        out[1] = 'E';
        out[2] = 'M';
        out[3] = 'P';
        out[4] = ' ';
    } else if (mode == DMM_MODE_V_DC) {
        out[0] = 'D';
        out[1] = 'C';
        out[2] = ' ';
        out[3] = 'V';
        out[4] = ' ';
    } else if (mode == DMM_MODE_V_AC) {
        out[0] = 'A';
        out[1] = 'C';
        out[2] = ' ';
        out[3] = 'V';
        out[4] = ' ';
    } else if (mode == DMM_MODE_V_ACDC) {
        out[0] = 'A';
        out[1] = 'C';
        out[2] = 'D';
        out[3] = 'C';
        out[4] = 'V';
    } else if (mode == DMM_MODE_P10_OHMS) {
        out[0] = 'O';
        out[1] = 'H';
        out[2] = 'M';
        out[3] = 'S';
        out[4] = ' ';
    } else if (mode == DMM_MODE_P10_CONT) {
        out[0] = 'C';
        out[1] = 'O';
        out[2] = 'N';
        out[3] = 'T';
        out[4] = ' ';
    } else if (mode == DMM_MODE_P10_CAP) {
        out[0] = 'C';
        out[1] = 'A';
        out[2] = 'P';
        out[3] = ' ';
        out[4] = ' ';
    } else if (mode == DMM_MODE_A_AC || mode == DMM_MODE_LEGACY_CURRENT_AC ||
               mode == DMM_MODE_MA_ALT_AC || mode == DMM_MODE_MVA_AC) {
        out[0] = 'A';
        out[1] = 'C';
        out[2] = ' ';
        out[3] = 'A';
        out[4] = ' ';
    } else if (mode == DMM_MODE_A_DC || mode == DMM_MODE_MVA_DC) {
        out[0] = 'D';
        out[1] = 'C';
        out[2] = ' ';
        out[3] = 'A';
        out[4] = ' ';
    } else if (mode == DMM_MODE_MA_AC || mode == DMM_MODE_MA_AUX) {
        out[0] = 'A';
        out[1] = 'C';
        out[2] = 'm';
        out[3] = 'A';
        out[4] = ' ';
    } else if (mode == DMM_MODE_MA_DC || mode == DMM_MODE_MA_BURDEN) {
        out[0] = 'D';
        out[1] = 'C';
        out[2] = 'm';
        out[3] = 'A';
        out[4] = ' ';
    } else if (mode == DMM_MODE_FREQ) {
        out[0] = 'F';
        out[1] = 'R';
        out[2] = 'E';
        out[3] = 'Q';
        out[4] = ' ';
    } else if (mode == DMM_MODE_PERIOD) {
        out[0] = 'P';
        out[1] = 'E';
        out[2] = 'R';
        out[3] = ' ';
        out[4] = ' ';
    } else if (mode == DMM_MODE_DUTY) {
        out[0] = 'D';
        out[1] = 'U';
        out[2] = 'T';
        out[3] = 'Y';
        out[4] = ' ';
    } else {
        out[0] = 'D';
        out[1] = 'I';
        out[2] = 'O';
        out[3] = 'D';
        out[4] = ' ';
    }
}

static void __attribute__((unused)) copy5(const char src[5], char out[5]) {
    for (uint8_t i = 0; i < 5u; ++i) {
        out[i] = src[i];
    }
}

static void __attribute__((unused)) format_u4(uint32_t value, char out[5]) {
    if (value > 9999u) {
        copy5("OFL  ", out);
        return;
    }

    uint16_t divisors[4] = {1000u, 100u, 10u, 1u};
    bool seen = false;
    for (uint8_t i = 0; i < 4u; ++i) {
        uint8_t digit = 0;
        while (value >= divisors[i]) {
            value -= divisors[i];
            ++digit;
        }
        if (digit != 0u || seen || i == 3u) {
            out[i] = (char)('0' + digit);
            seen = true;
        } else {
            out[i] = ' ';
        }
    }
    out[4] = ' ';
}

static void __attribute__((unused)) format_u5(uint32_t value, char out[5]) {
    if (value > 99999u) {
        value = 99999u;
    }
    for (int8_t i = 4; i >= 0; --i) {
        out[i] = (char)('0' + (value % 10u));
        value /= 10u;
    }
}

static void __attribute__((unused)) format_2digits(uint8_t value, char *out) {
    out[0] = (char)('0' + (value / 10u) % 10u);
    out[1] = (char)('0' + value % 10u);
}

static bool __attribute__((unused)) setup_page_has_subfields(setup_page_t page) {
    return page == SETUP_PAGE_DATE || page == SETUP_PAGE_TIME;
}

static bool voltage_dbm_milli(int32_t value, uint8_t decimal_pos, int32_t *dbm_milli);

static bool __attribute__((unused)) setup_is_leap_year(uint16_t year) {
    return ((year % 4u) == 0u && (year % 100u) != 0u) || (year % 400u) == 0u;
}

static uint8_t __attribute__((unused)) setup_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[12] = {
        31u, 28u, 31u, 30u, 31u, 30u,
        31u, 31u, 30u, 31u, 30u, 31u,
    };
    if (month < 1u || month > 12u) {
        month = 1u;
    }
    if (month == 2u && setup_is_leap_year(year)) {
        return 29u;
    }
    return days[month - 1u];
}

static void __attribute__((unused)) setup_clamp_date(setup_state_t *setup) {
    if (setup->month < 1u || setup->month > 12u) {
        setup->month = 1u;
    }
    uint8_t max_day = setup_days_in_month(setup->year, setup->month);
    if (setup->day < 1u) {
        setup->day = 1u;
    } else if (setup->day > max_day) {
        setup->day = max_day;
    }
}

static void setup_rtc_tick_one_second(setup_state_t *setup) {
    if (++setup->second < 60u) {
        return;
    }
    setup->second = 0u;
    if (++setup->minute < 60u) {
        return;
    }
    setup->minute = 0u;
    if (++setup->hour < 24u) {
        return;
    }
    setup->hour = 0u;
    uint8_t max_day = setup_days_in_month(setup->year, setup->month);
    if (++setup->day <= max_day) {
        return;
    }
    setup->day = 1u;
    if (++setup->month <= 12u) {
        return;
    }
    setup->month = 1u;
    if (setup->year < 2099u) {
        ++setup->year;
    } else {
        setup->year = 2000u;
    }
}

static void __attribute__((unused)) format_setup_battery_percent(uint8_t battery_percent,
                                                                 bool battery_ok,
                                                                 char upper[5]) {
    if (!battery_ok) {
        copy5("BAT--", upper);
        return;
    }
    if (battery_percent > 99u) {
        battery_percent = 99u;
    }
    upper[0] = 'B';
    upper[1] = 'A';
    upper[2] = 'T';
    upper[3] = (char)('0' + battery_percent / 10u);
    upper[4] = (char)('0' + battery_percent % 10u);
}

static void __attribute__((unused)) format_setup_interval(uint16_t seconds, char upper[5]) {
    if (seconds > 999u) {
        seconds = 999u;
    }
    upper[0] = 'I';
    upper[1] = 'n';
    upper[2] = seconds >= 100u ? (char)('0' + (seconds / 100u) % 10u) : ' ';
    upper[3] = seconds >= 10u ? (char)('0' + (seconds / 10u) % 10u) : ' ';
    upper[4] = (char)('0' + seconds % 10u);
}

static bool __attribute__((unused)) mode_has_burden_voltage_setup(uint8_t mode) {
    return mode == DMM_MODE_MA_AUX || mode == DMM_MODE_LEGACY_CURRENT_AC ||
           mode == DMM_MODE_MVA_AC || mode == DMM_MODE_MA_BURDEN ||
           mode == DMM_MODE_MA_ALT_AC || mode == DMM_MODE_MVA_DC;
}

static bool __attribute__((unused)) mode_has_voltage_secondary_setup(uint8_t mode) {
    return (mode == DMM_MODE_V_AC && !dmm_p10_voltage_alt_active()) ||
           mode == DMM_MODE_MV_AC;
}

static bool __attribute__((unused)) mode_has_dbm_setup(uint8_t mode) {
    return mode == DMM_MODE_V_AC && !dmm_p10_voltage_alt_active();
}

static bool __attribute__((unused)) mode_has_frontend_timing_setup(uint8_t mode) {
    return mode == DMM_MODE_MA_AC || mode == DMM_MODE_MA_DC ||
           mode == DMM_MODE_A_AC || mode == DMM_MODE_A_DC ||
           mode == DMM_MODE_A_AC_PERSIST || mode == DMM_MODE_A_DC_PERSIST;
}

static bool __attribute__((unused)) mode_has_single_current_secondary_b(uint8_t mode,
                                                                        const setup_state_t *setup) {
    if (mode == DMM_MODE_MA_AC || mode == DMM_MODE_A_AC || mode == DMM_MODE_A_DC_PERSIST) {
        return true;
    }
    if (mode == DMM_MODE_MA_DC || mode == DMM_MODE_A_DC || mode == DMM_MODE_A_AC_PERSIST) {
        return !setup->frontend_timing_enabled;
    }
    return false;
}

static bool __attribute__((unused)) setup_page_valid_for_mode(setup_page_t page, uint8_t mode,
                                                              const setup_state_t *setup) {
    if (page == SETUP_PAGE_FUNCTION) {
        return mode == DMM_MODE_P10_DIODE || mode == DMM_MODE_P10_CONT ||
               mode_has_voltage_secondary_setup(mode) ||
               mode_has_burden_voltage_setup(mode) ||
               mode_has_frontend_timing_setup(mode);
    }
    if (page == SETUP_PAGE_DBM) {
        return mode_has_dbm_setup(mode);
    }
    if (page == SETUP_PAGE_FRONTEND_TIMING) {
        return false;
    }
    if (page == SETUP_PAGE_SECONDARY_B) {
        return mode_has_single_current_secondary_b(mode, setup);
    }
    if (page == SETUP_PAGE_BURDEN_VOLTAGE) {
        return mode_has_burden_voltage_setup(mode);
    }
    if (page == SETUP_PAGE_BURDEN_VALUE) {
        return mode_has_burden_voltage_setup(mode) && setup->burden_voltage_enabled;
    }
    return page < SETUP_PAGE_COUNT;
}

static bool __attribute__((unused)) setup_page_editable_for_mode(setup_page_t page, uint8_t mode) {
    switch (page) {
    case SETUP_PAGE_TEMP:
    case SETUP_PAGE_APO:
    case SETUP_PAGE_BUZZER:
    case SETUP_PAGE_LCD:
    case SETUP_PAGE_YEAR:
    case SETUP_PAGE_DATE:
    case SETUP_PAGE_TIME:
    case SETUP_PAGE_ID:
    case SETUP_PAGE_LOG_INTERVAL:
    case SETUP_PAGE_FRONTEND_TIMING:
    case SETUP_PAGE_BURDEN_VOLTAGE:
        return true;
    case SETUP_PAGE_FUNCTION:
        return mode == DMM_MODE_P10_DIODE || mode == DMM_MODE_P10_CONT ||
               mode_has_frontend_timing_setup(mode);
    case SETUP_PAGE_BATTERY:
    case SETUP_PAGE_DBM:
    case SETUP_PAGE_SECONDARY_B:
    case SETUP_PAGE_BURDEN_VALUE:
    default:
        return false;
    }
}

static setup_page_t __attribute__((unused)) setup_next_page_for_mode(setup_page_t page,
                                                                     uint8_t mode,
                                                                     const setup_state_t *setup) {
    for (uint8_t i = 0; i < (uint8_t)SETUP_PAGE_COUNT; ++i) {
        page = (setup_page_t)((page + 1u) % SETUP_PAGE_COUNT);
        if (setup_page_valid_for_mode(page, mode, setup)) {
            return page;
        }
    }
    return SETUP_PAGE_TEMP;
}

static uint8_t __attribute__((unused)) setup_secondary_decimal_position(setup_page_t page,
                                                                        uint8_t mode) {
    if (page == SETUP_PAGE_FRONTEND_TIMING || page == SETUP_PAGE_BURDEN_VOLTAGE) {
        return 3u;
    }
    if (page == SETUP_PAGE_FUNCTION && mode_has_frontend_timing_setup(mode)) {
        return 3u;
    }
    if (page == SETUP_PAGE_APO) {
        return 2u;
    }
    if (page == SETUP_PAGE_BATTERY) {
        return 1u;
    }
    (void)mode;
    return 0u;
}

static uint8_t __attribute__((unused)) stock_voltage_secondary_decimal(uint8_t mode,
                                                                       uint8_t range) {
    if (mode != DMM_MODE_V_AC && mode != DMM_MODE_MV_AC) {
        return 0u;
    }
    return dmm_p10_decimal_position(mode, range);
}

static uint8_t __attribute__((unused)) stock_current_secondary_decimal(uint8_t mode,
                                                                       bool bank1) {
    if (!bank1) {
        return dmm_p10_current_alt_range0() != 0u ? 3u : 4u;
    }
    if (mode == DMM_MODE_MA_AUX || mode == DMM_MODE_MA_BURDEN) {
        return 4u;
    }
    if (mode == DMM_MODE_LEGACY_CURRENT_AC || mode == DMM_MODE_MA_ALT_AC) {
        return dmm_p10_current_alt_range1() != 0u ? 3u : 4u;
    }
    return dmm_p10_current_alt_range1() != 0u ? 3u : 2u;
}

static bool __attribute__((unused)) mode_has_dual_current_voltage_secondary(uint8_t mode) {
    return mode == DMM_MODE_MA_AUX || mode == DMM_MODE_LEGACY_CURRENT_AC ||
           mode == DMM_MODE_MVA_AC || mode == DMM_MODE_MA_BURDEN ||
           mode == DMM_MODE_MA_ALT_AC || mode == DMM_MODE_MVA_DC;
}

static bool __attribute__((unused)) render_dual_current_voltage_secondary(uint8_t mode,
                                                                          bool bank1) {
    if (!mode_has_dual_current_voltage_secondary(mode)) {
        return false;
    }
    if (!dmm_p10_current_dual_display_ready()) {
        lcd_set_secondary_sign(false);
        lcd_set_secondary_decimal(0u);
        lcd_write_5chars(true, "-----");
        return true;
    }
    lcd_write_upper_signed_numeric(bank1 ? dmm_p10_current_dual_bank1_value()
                                         : dmm_p10_current_dual_bank0_value(),
                                   stock_current_secondary_decimal(mode, bank1));
    lcd_set_secondary_voltage_annunciator(true);
    return true;
}

static bool __attribute__((unused)) render_voltage_frequency_secondary(uint8_t mode) {
    if ((mode != DMM_MODE_V_AC && mode != DMM_MODE_MV_AC) ||
        !dmm_p10_frequency_secondary_active()) {
        return false;
    }
    if (dmm_p10_frequency_secondary_overrange()) {
        lcd_set_secondary_sign(false);
        lcd_set_secondary_decimal(0u);
        lcd_write_5chars(true, " OFL ");
    } else {
        int32_t value = dmm_p10_frequency_secondary_valid() ?
            dmm_p10_frequency_secondary_display_value() : 0;
        lcd_write_upper_signed_numeric(value, dmm_p10_frequency_secondary_decimal());
    }
    lcd_set_secondary_frequency_annunciator(dmm_p10_frequency_secondary_range());
    return true;
}

static bool __attribute__((unused)) render_stock_setup_secondary_page(setup_page_t page,
                                                                      const setup_state_t *setup,
                                                                      uint8_t mode,
                                                                      uint8_t range,
                                                                      int32_t rendered_value,
                                                                      bool rendered_overrange) {
    if (page == SETUP_PAGE_DBM && mode_has_dbm_setup(mode)) {
        int32_t dbm_value = 0;
        if (rendered_overrange ||
            !voltage_dbm_milli(rendered_value, dmm_p10_decimal_position(mode, range), &dbm_value)) {
            lcd_set_secondary_decimal(0u);
            lcd_write_5chars(true, " OFL ");
        } else {
            lcd_write_upper_signed_numeric(dbm_value, 3u);
        }
        lcd_set_dbm_annunciator(true);
        return true;
    }

    if (page == SETUP_PAGE_FUNCTION && mode_has_voltage_secondary_setup(mode)) {
        int32_t secondary = dmm_p10_voltage_alt_secondary_value() / 10;
        if (secondary == 0 && !rendered_overrange) {
            secondary = rendered_value;
        }
        if (rendered_overrange) {
            lcd_set_secondary_decimal(0u);
            lcd_write_5chars(true, " OFL ");
        } else {
            lcd_write_upper_signed_numeric(secondary, stock_voltage_secondary_decimal(mode, range));
        }
        return true;
    }

    if (page == SETUP_PAGE_FUNCTION && mode_has_burden_voltage_setup(mode)) {
        bool bank1 = dmm_p10_current_dual_bank_active();
        int32_t secondary = bank1 ? dmm_p10_current_dual_bank1_value()
                                  : dmm_p10_current_dual_bank0_value();
        lcd_write_upper_signed_numeric(secondary, stock_current_secondary_decimal(mode, bank1));
        return true;
    }

    if (page == SETUP_PAGE_SECONDARY_B && mode_has_single_current_secondary_b(mode, setup)) {
        lcd_write_upper_b_scaled(dmm_p10_current_single_aux_value(), 100u,
                                 dmm_p10_current_single_aux_overrange());
        return true;
    }

    if (page == SETUP_PAGE_BURDEN_VALUE && mode_has_burden_voltage_setup(mode) &&
        setup->burden_voltage_enabled) {
        int32_t secondary = dmm_p10_current_dual_bank1_value();
        uint8_t alt_range = dmm_p10_current_alt_range1();
        int32_t stock_scaled_source = alt_range != 0u ? secondary * 10 : secondary * 100;
        lcd_write_upper_b_scaled(stock_scaled_source, alt_range, false);
        return true;
    }

    return false;
}

static bool __attribute__((unused)) render_current_timing_default_secondary(uint8_t mode,
                                                                            const setup_state_t *setup) {
    if (!mode_has_frontend_timing_setup(mode)) {
        return false;
    }
    lcd_set_secondary_decimal(3u);
    lcd_write_5chars(true, setup->frontend_timing_enabled ? "bdOFF" : "bdOn ");
    return true;
}

static void __attribute__((unused)) format_setup_page(setup_page_t page, const setup_state_t *setup,
                                                       uint8_t mode, bool editing, int16_t temp_c_tenths,
                                                       uint8_t battery_percent, bool battery_ok,
                                                       uint8_t edit_subfield, bool blink_blank,
                                                       char upper[5], char lower[5]) {
    (void)temp_c_tenths;
    copy5("     ", lower);
    switch (page) {
    case SETUP_PAGE_TEMP:
        copy5(setup->temp_fahrenheit ? "  F  " : "  C  ", upper);
        break;
    case SETUP_PAGE_BATTERY:
        format_setup_battery_percent(battery_percent, battery_ok, upper);
        break;
    case SETUP_PAGE_APO:
        copy5(setup->apo_enabled ? "APOoN" : "APOoF", upper);
        break;
    case SETUP_PAGE_BUZZER:
        copy5(setup->buzzer_enabled ? "b-On " : "b-OFF", upper);
        break;
    case SETUP_PAGE_LCD:
        upper[0] = 'L';
        upper[1] = 'c';
        upper[2] = 'd';
        upper[3] = '-';
        upper[4] = (char)('0' + ((setup->lcd_contrast >= 10u && setup->lcd_contrast <= 17u) ?
                                 (setup->lcd_contrast - 10u) : 0u));
        break;
    case SETUP_PAGE_YEAR:
        format_u4(setup->year, upper);
        upper[4] = ' ';
        break;
    case SETUP_PAGE_DATE:
        format_2digits(setup->month, &upper[0]);
        upper[2] = '-';
        format_2digits(setup->day, &upper[3]);
        break;
    case SETUP_PAGE_TIME:
        format_2digits(setup->hour, &upper[0]);
        upper[2] = '-';
        format_2digits(setup->minute, &upper[3]);
        break;
    case SETUP_PAGE_ID:
        format_u5(setup->meter_id, upper);
        break;
    case SETUP_PAGE_LOG_INTERVAL:
        format_setup_interval(setup->logging_interval_s, upper);
        break;
    case SETUP_PAGE_DBM:
        copy5(setup->dbm_enabled ? "dbOn " : "dbOFF", upper);
        break;
    case SETUP_PAGE_FRONTEND_TIMING:
        copy5(setup->frontend_timing_enabled ? "bdOFF" : "bdOn ", upper);
        break;
    case SETUP_PAGE_BURDEN_VOLTAGE:
        copy5(setup->burden_voltage_enabled ? "bdOn " : "bdOFF", upper);
        break;
    case SETUP_PAGE_SECONDARY_B:
    case SETUP_PAGE_BURDEN_VALUE:
        copy5("     ", upper);
        break;
    case SETUP_PAGE_FUNCTION:
    default:
        if (mode == DMM_MODE_P10_DIODE) {
            copy5(dmm_p10_diode_option_15() ? "   15" : "    3", upper);
        } else if (mode == DMM_MODE_P10_CONT) {
            static const char labels[4][5] = {
                {'D', 'N', ' ', '3', '0'},
                {'U', 'P', ' ', '3', '0'},
                {'D', 'N', '3', '0', '0'},
                {'U', 'P', '3', '0', '0'},
            };
            uint8_t option = setup->continuity_option < 4u ? setup->continuity_option : 0u;
            copy5(labels[option], upper);
        } else if (mode_has_frontend_timing_setup(mode)) {
            copy5(setup->frontend_timing_enabled ? "bdOFF" : "bdOn ", upper);
        } else if (mode_has_voltage_secondary_setup(mode) || mode_has_burden_voltage_setup(mode)) {
            copy5("     ", upper);
        } else {
            copy5("OHMS ", upper);
        }
        break;
    }
    if (editing && blink_blank) {
        if (page == SETUP_PAGE_DATE) {
            if (edit_subfield == 0u) {
                upper[0] = ' ';
                upper[1] = ' ';
            } else {
                upper[3] = ' ';
                upper[4] = ' ';
            }
        } else if (page == SETUP_PAGE_TIME) {
            if (edit_subfield == 0u) {
                upper[0] = ' ';
                upper[1] = ' ';
            } else {
                upper[3] = ' ';
                upper[4] = ' ';
            }
        } else if (page == SETUP_PAGE_ID) {
            upper[edit_subfield < 5u ? edit_subfield : 0u] = ' ';
        } else {
            copy5("     ", upper);
        }
    }
}

static void __attribute__((unused)) setup_adjust_value(setup_state_t *setup, setup_page_t page,
                                                       int8_t delta, uint8_t mode,
                                                       uint8_t edit_subfield) {
    switch (page) {
    case SETUP_PAGE_TEMP:
        setup->temp_fahrenheit = !setup->temp_fahrenheit;
        break;
    case SETUP_PAGE_APO:
        setup->apo_enabled = !setup->apo_enabled;
        break;
    case SETUP_PAGE_BUZZER:
        setup->buzzer_enabled = !setup->buzzer_enabled;
        buzzer_set_enabled(setup->buzzer_enabled);
        break;
    case SETUP_PAGE_LCD:
        if (delta > 0) {
            setup->lcd_contrast = setup->lcd_contrast >= 17u ? 10u :
                                  (uint8_t)(setup->lcd_contrast + 1u);
        } else {
            setup->lcd_contrast = setup->lcd_contrast <= 10u ? 17u :
                                  (uint8_t)(setup->lcd_contrast - 1u);
        }
        lcd_set_contrast(setup->lcd_contrast);
        break;
    case SETUP_PAGE_YEAR:
        if (delta > 0 && setup->year < 2099u) {
            ++setup->year;
        } else if (delta < 0 && setup->year > 2000u) {
            --setup->year;
        }
        setup_clamp_date(setup);
        break;
    case SETUP_PAGE_DATE:
        if (edit_subfield == 0u) {
            if (delta > 0) {
                setup->month = setup->month >= 12u ? 1u : (uint8_t)(setup->month + 1u);
            } else {
                setup->month = setup->month <= 1u ? 12u : (uint8_t)(setup->month - 1u);
            }
            setup_clamp_date(setup);
        } else {
            uint8_t max_day = setup_days_in_month(setup->year, setup->month);
            if (delta > 0) {
                setup->day = setup->day >= max_day ? 1u : (uint8_t)(setup->day + 1u);
            } else {
                setup->day = setup->day <= 1u ? max_day : (uint8_t)(setup->day - 1u);
            }
        }
        break;
    case SETUP_PAGE_TIME:
        if (edit_subfield == 0u) {
            if (delta > 0) {
                setup->hour = (uint8_t)((setup->hour + 1u) % 24u);
            } else {
                setup->hour = setup->hour == 0u ? 23u : (uint8_t)(setup->hour - 1u);
            }
        } else {
            if (delta > 0) {
                setup->minute = (uint8_t)((setup->minute + 1u) % 60u);
            } else {
                setup->minute = setup->minute == 0u ? 59u : (uint8_t)(setup->minute - 1u);
            }
        }
        setup->second = 0u;
        break;
    case SETUP_PAGE_ID:
    {
        uint32_t place = 1u;
        uint8_t digit_index = edit_subfield < 5u ? (uint8_t)(4u - edit_subfield) : 4u;
        for (uint8_t i = 0; i < digit_index; ++i) {
            place *= 10u;
        }
        uint8_t digit = (uint8_t)((setup->meter_id / place) % 10u);
        digit = delta > 0 ? (uint8_t)((digit + 1u) % 10u) :
                (digit == 0u ? 9u : (uint8_t)(digit - 1u));
        setup->meter_id = setup->meter_id - ((setup->meter_id / place) % 10u) * place +
                          (uint32_t)digit * place;
        break;
    }
    case SETUP_PAGE_LOG_INTERVAL:
        if (delta > 0 && setup->logging_interval_s < 999u) {
            ++setup->logging_interval_s;
        } else if (delta < 0 && setup->logging_interval_s > 0u) {
            --setup->logging_interval_s;
        }
        break;
    case SETUP_PAGE_DBM:
        setup->dbm_enabled = !setup->dbm_enabled;
        break;
    case SETUP_PAGE_FRONTEND_TIMING:
        setup->frontend_timing_enabled = !setup->frontend_timing_enabled;
        dmm_p10_set_frontend_timing_toggle_enabled(setup->frontend_timing_enabled);
        break;
    case SETUP_PAGE_BURDEN_VOLTAGE:
        setup->burden_voltage_enabled = !setup->burden_voltage_enabled;
        dmm_p10_set_burden_voltage_enabled(setup->burden_voltage_enabled);
        break;
    case SETUP_PAGE_SECONDARY_B:
    case SETUP_PAGE_BURDEN_VALUE:
        break;
    case SETUP_PAGE_FUNCTION:
        if (mode == DMM_MODE_P10_DIODE) {
            dmm_p10_toggle_diode_option();
        } else if (mode == DMM_MODE_P10_CONT) {
            if (delta > 0) {
                setup->continuity_option = (uint8_t)((setup->continuity_option + 1u) % 4u);
            } else {
                setup->continuity_option = setup->continuity_option == 0u ? 3u :
                                           (uint8_t)(setup->continuity_option - 1u);
            }
            dmm_p10_set_continuity_option(setup->continuity_option);
        } else if (mode_has_frontend_timing_setup(mode)) {
            setup->frontend_timing_enabled = !setup->frontend_timing_enabled;
            dmm_p10_set_frontend_timing_toggle_enabled(setup->frontend_timing_enabled);
        }
        break;
    case SETUP_PAGE_BATTERY:
    default:
        break;
    }
}

static uint8_t __attribute__((unused)) setup_load_lcd_contrast(void) {
    uint8_t saved = calibration_read_u8(CAL_OFFSET_LCD_CONTRAST);
    return (saved >= 12u && saved < 18u) ? saved : 14u;
}

static bool setup_write_u8_if_changed(uint32_t offset, uint8_t value) {
    return calibration_read_u8(offset) == value || calibration_write_u8(offset, value);
}

static bool setup_write_u16_be_if_changed(uint32_t offset, uint16_t value) {
    return calibration_read_u16_be(offset) == value || calibration_write_u16_be(offset, value);
}

static bool setup_write_u32_be_if_changed(uint32_t offset, uint32_t value) {
    return calibration_read_u32_be(offset) == value || calibration_write_u32_be(offset, value);
}

static bool __attribute__((unused)) setup_persist_page(const setup_state_t *setup, setup_page_t page,
                                                       uint8_t mode) {
    switch (page) {
    case SETUP_PAGE_TEMP:
        return setup_write_u8_if_changed(CAL_OFFSET_TEMP_UNIT,
                                         setup->temp_fahrenheit ? 0u : 1u);
    case SETUP_PAGE_APO:
        return setup_write_u8_if_changed(CAL_OFFSET_APO, setup->apo_enabled ? 0u : 1u);
    case SETUP_PAGE_BUZZER:
        return setup_write_u8_if_changed(CAL_OFFSET_BUZZER,
                                         setup->buzzer_enabled ? 1u : 0u);
    case SETUP_PAGE_LCD:
        return setup_write_u8_if_changed(CAL_OFFSET_LCD_CONTRAST, setup->lcd_contrast);
    case SETUP_PAGE_ID:
        return setup_write_u32_be_if_changed(CAL_OFFSET_METER_ID,
                                             setup->meter_id < 100000u ? setup->meter_id : 0u);
    case SETUP_PAGE_LOG_INTERVAL:
        return setup_write_u16_be_if_changed(CAL_OFFSET_LOG_INTERVAL,
                                             setup->logging_interval_s < 1000u ?
                                             setup->logging_interval_s : 1u);
    case SETUP_PAGE_DBM:
        return setup_write_u8_if_changed(CAL_OFFSET_DBM_MODE,
                                         setup->dbm_enabled ? 1u : 0u);
    case SETUP_PAGE_FRONTEND_TIMING:
        return true;
    case SETUP_PAGE_BURDEN_VOLTAGE:
        return setup_write_u8_if_changed(CAL_OFFSET_BURDEN_VOLTAGE,
                                         setup->burden_voltage_enabled ? 1u : 0u);
    case SETUP_PAGE_SECONDARY_B:
    case SETUP_PAGE_BURDEN_VALUE:
        return true;
    case SETUP_PAGE_FUNCTION:
        if (mode_has_frontend_timing_setup(mode)) {
            return true;
        }
        if (mode == DMM_MODE_P10_DIODE) {
            return setup_write_u8_if_changed(CAL_OFFSET_DIODE_VOLTAGE,
                                             dmm_p10_diode_option_15() ? 1u : 0u);
        }
        if (mode == DMM_MODE_P10_CONT) {
            return setup_write_u8_if_changed(CAL_OFFSET_CONTINUITY_OPTION,
                                             setup->continuity_option < 4u ?
                                             setup->continuity_option : 0u);
        }
        return true;
    default:
        return true;
    }
}

static void __attribute__((unused)) setup_apply_runtime_options(const setup_state_t *setup) {
    buzzer_set_enabled(setup->buzzer_enabled);
    lcd_set_contrast(setup->lcd_contrast);
    dmm_p10_set_continuity_option(setup->continuity_option);
    dmm_p10_set_burden_voltage_enabled(setup->burden_voltage_enabled);
    dmm_p10_set_frontend_timing_toggle_enabled(setup->frontend_timing_enabled);
}

static uint32_t __attribute__((unused)) abs_i32_main(int32_t value) {
    if (value < 0) {
        return (uint32_t)(-(value + 1)) + 1u;
    }
    return (uint32_t)value;
}

static uint32_t pow10_u32(uint8_t exponent) {
    uint32_t value = 1u;
    while (exponent-- != 0u) {
        value *= 10u;
    }
    return value;
}

static uint8_t u64_log2_floor(uint64_t value) {
    uint8_t result = 0u;
    while (value > 1u) {
        value >>= 1;
        ++result;
    }
    return result;
}

static uint64_t u64_div_u64(uint64_t numerator, uint64_t denominator) {
    if (denominator == 0u) {
        return UINT64_MAX;
    }

    uint64_t quotient = 0u;
    uint64_t remainder = 0u;
    for (int8_t bit = 63; bit >= 0; --bit) {
        remainder = (remainder << 1) | ((numerator >> (uint8_t)bit) & 1u);
        if (remainder >= denominator) {
            remainder -= denominator;
            quotient |= 1ull << (uint8_t)bit;
        }
    }
    return quotient;
}

static int64_t ln_u64_q20(uint64_t value) {
    if (value == 0u) {
        return INT64_MIN / 4;
    }

    const int64_t q31_one = 1ll << 31;
    const int64_t ln2_q20 = 726817ll;
    uint8_t exponent = u64_log2_floor(value);
    uint64_t mantissa_q31 = exponent <= 31u ?
                            (value << (31u - exponent)) :
                            (value >> (exponent - 31u));
    uint64_t z_numerator = (mantissa_q31 - (uint64_t)q31_one) * (uint64_t)q31_one;
    uint64_t z_denominator = mantissa_q31 + (uint64_t)q31_one;
    int64_t z_q31 = (int64_t)u64_div_u64(z_numerator, z_denominator);
    int64_t z2_q31 = (z_q31 * z_q31) >> 31;
    int64_t term_q31 = z_q31;
    int64_t sum_q31 = 0;
    for (uint8_t i = 0u; i < 9u; ++i) {
        sum_q31 += (int64_t)u64_div_u64((uint64_t)term_q31, (uint64_t)(2u * i + 1u));
        term_q31 = (term_q31 * z2_q31) >> 31;
    }

    int64_t ln_mantissa_q20 = (2ll * sum_q31) >> 11;
    return (int64_t)exponent * ln2_q20 + ln_mantissa_q20;
}

static bool voltage_dbm_milli(int32_t value, uint8_t decimal_pos, int32_t *dbm_milli) {
    uint64_t abs_value = abs_i32_main(value);
    if (abs_value == 0u || decimal_pos > 4u) {
        return false;
    }

    uint64_t numerator = 5u * abs_value * abs_value;
    uint64_t denominator = 3ull * pow10_u32((uint8_t)(2u * decimal_pos));
    int64_t diff_ln_q20 = ln_u64_q20(numerator) - ln_u64_q20(denominator);
    const int64_t ln10_q20 = 2414963ll;
    bool negative = diff_ln_q20 < 0;
    uint64_t diff_abs = negative ? (uint64_t)(-(diff_ln_q20 + 1)) + 1u :
                        (uint64_t)diff_ln_q20;
    int64_t milli = (int64_t)u64_div_u64(diff_abs * 10000ull, (uint64_t)ln10_q20);
    if (negative) {
        milli = -milli;
    }
    if (milli > 99999ll) {
        milli = 99999ll;
    } else if (milli < -99999ll) {
        milli = -99999ll;
    }
    *dbm_milli = (int32_t)milli;
    return true;
}

static int32_t temperature_display_tenths(int32_t c_tenths, bool fahrenheit) {
    if (!fahrenheit) {
        return c_tenths;
    }
    int32_t scaled = c_tenths * 9;
    if (scaled >= 0) {
        scaled = (scaled + 2) / 5;
    } else {
        scaled = (scaled - 2) / 5;
    }
    return scaled + 320;
}

static int16_t stock_aux_temperature_display_tenths_from_centideg(int16_t c_centideg,
                                                                  bool fahrenheit) {
    int32_t display_centideg = c_centideg;
    if (fahrenheit) {
        int32_t scaled = (int32_t)c_centideg * 9;
        if (scaled >= 0) {
            scaled = (scaled + 2) / 5;
        } else {
            scaled = (scaled - 2) / 5;
        }
        display_centideg = scaled + 3200;
    }

    int32_t tenths = display_centideg / 10;
    if ((tenths % 10) >= 5) {
        ++tenths;
    }
    if (tenths > 32767) {
        tenths = 32767;
    } else if (tenths < -32768) {
        tenths = -32768;
    }
    return (int16_t)tenths;
}

static bool __attribute__((unused)) format_p10_status_text(uint8_t mode, bool has_value, bool overrange, int32_t value, char out[5]) {
    if (overrange) {
        copy5("OFL  ", out);
        return true;
    }

    if (!has_value) {
        copy5("     ", out);
        return true;
    }

    (void)mode;
    (void)value;
    return false;
}

static char *log_append_char(char *p, char *end, char ch) {
    if (p < end) {
        *p++ = ch;
    }
    return p;
}

static char *log_append_text(char *p, char *end, const char *text) {
    while (*text != '\0') {
        p = log_append_char(p, end, *text++);
    }
    return p;
}

static const char k_log_csv_header[] =
    "Sample,Function,Value,Units,Secondary Function,Secondary Value,Secondary Units\r\n";

static void log_build_stock_name83(const setup_state_t *setup, uint8_t suffix, char name83[11]) {
    uint8_t yy = (uint8_t)(setup->year % 100u);
    uint8_t month = setup->month;
    uint8_t day = setup->day;
    if (month < 1u || month > 12u) {
        month = 1u;
    }
    uint8_t max_day = setup_days_in_month(setup->year, month);
    if (day < 1u || day > max_day) {
        day = 1u;
    }
    if (suffix > 99u) {
        suffix = 99u;
    }
    name83[0] = (char)('0' + yy / 10u);
    name83[1] = (char)('0' + yy % 10u);
    name83[2] = (char)('0' + month / 10u);
    name83[3] = (char)('0' + month % 10u);
    name83[4] = (char)('0' + day / 10u);
    name83[5] = (char)('0' + day % 10u);
    name83[6] = (char)('0' + suffix / 10u);
    name83[7] = (char)('0' + suffix % 10u);
    name83[8] = 'C';
    name83[9] = 'S';
    name83[10] = 'V';
}

static sd_fat_result_t log_select_stock_name83(const setup_state_t *setup, char name83[11]) {
    sd_fat_result_t last_rc = SD_FAT_ERR_FULL;
    for (uint8_t suffix = 0u; suffix < 100u; ++suffix) {
        log_build_stock_name83(setup, suffix, name83);
        bool exists = true;
        sd_fat_result_t rc = sd_fat_file_exists_83(name83, &exists);
        if (rc != SD_FAT_OK) {
            return rc;
        }
        last_rc = rc;
        if (!exists) {
            return SD_FAT_OK;
        }
    }
    log_build_stock_name83(setup, 99u, name83);
    return last_rc == SD_FAT_OK ? SD_FAT_ERR_FULL : last_rc;
}

static char *log_append_u32(char *p, char *end, uint32_t value) {
    char digits[10];
    uint8_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && count < sizeof(digits));
    while (count != 0u) {
        p = log_append_char(p, end, digits[--count]);
    }
    return p;
}

static char *log_append_fixed_value(char *p, char *end, int32_t value, uint8_t decimal_pos) {
    if (value < 0) {
        p = log_append_char(p, end, '-');
        value = (value == INT32_MIN) ? INT32_MAX : -value;
    }

    uint32_t scale = pow10_u32(decimal_pos);
    uint32_t whole = (uint32_t)value / scale;
    uint32_t frac = (uint32_t)value % scale;
    p = log_append_u32(p, end, whole);
    if (decimal_pos != 0u) {
        p = log_append_char(p, end, '.');
        for (uint8_t i = decimal_pos; i != 0u; --i) {
            uint32_t div = pow10_u32((uint8_t)(i - 1u));
            p = log_append_char(p, end, (char)('0' + (frac / div) % 10u));
        }
    }
    return p;
}

static void log_mode_function_unit(uint8_t mode, uint8_t range,
                                   const char **function, const char **unit) {
    switch (mode) {
    case DMM_MODE_LOZ:
    case DMM_MODE_V_DC:
        *function = "DCV";
        *unit = "V";
        break;
    case DMM_MODE_V_AC:
        *function = dmm_p10_voltage_alt_active() ? "AC+DCV" : "ACV";
        *unit = "V";
        break;
    case DMM_MODE_MV_DC:
        *function = "DCmV";
        *unit = "mV";
        break;
    case DMM_MODE_MV_AC:
        *function = "ACmV";
        *unit = "mV";
        break;
    case DMM_MODE_TEMP:
        *function = "TEMP";
        *unit = "C";
        break;
    case DMM_MODE_FREQ:
        *function = "FREQ";
        *unit = "Hz";
        break;
    case DMM_MODE_PERIOD:
        *function = "PER";
        *unit = "s";
        break;
    case DMM_MODE_DUTY:
        *function = "DUTY";
        *unit = "%";
        break;
    case DMM_MODE_P10_OHMS:
        *function = "OHM";
        *unit = "Ohm";
        break;
    case DMM_MODE_P10_CONT:
        *function = "CONT";
        *unit = "Ohm";
        break;
    case DMM_MODE_P10_DIODE:
        *function = "DIODE";
        *unit = "V";
        break;
    case DMM_MODE_P10_CAP:
        *function = "CAP";
        *unit = "F";
        break;
    case DMM_MODE_LEGACY_CURRENT_AC:
    case DMM_MODE_MA_ALT_AC:
        *function = "ACuA";
        *unit = "uA";
        break;
    case DMM_MODE_MVA_AC:
        *function = range < 2u ? "ACmVA" : "ACVA";
        *unit = range < 2u ? "mVA" : "VA";
        break;
    case DMM_MODE_MA_AC:
        *function = "ACmA";
        *unit = "mA";
        break;
    case DMM_MODE_A_AC:
    case DMM_MODE_A_AC_PERSIST:
        *function = "ACA";
        *unit = "A";
        break;
    case DMM_MODE_MA_AUX:
        *function = "DCuA";
        *unit = "uA";
        break;
    case DMM_MODE_MVA_DC:
        *function = range < 2u ? "DCmVA" : "DCVA";
        *unit = range < 2u ? "mVA" : "VA";
        break;
    case DMM_MODE_MA_DC:
    case DMM_MODE_MA_BURDEN:
        *function = "DCmA";
        *unit = "mA";
        break;
    case DMM_MODE_A_DC:
    case DMM_MODE_A_DC_PERSIST:
        *function = "DCA";
        *unit = "A";
        break;
    default:
        *function = "UNK";
        *unit = "";
        break;
    }
}

static log_secondary_t log_secondary_from_stock_state(uint8_t mode, uint8_t range,
                                                      bool main_overrange, int32_t main_value,
                                                      int16_t temp_tenths_c,
                                                      const setup_state_t *setup) {
    log_secondary_t secondary = {
        .function = "TEMP",
        .unit = "C",
        .value = temp_tenths_c,
        .decimal_pos = 1u,
        .has_value = true,
        .overrange = false,
    };

    if (mode_has_dbm_setup(mode) && setup->dbm_enabled) {
        secondary.function = "dBm";
        secondary.unit = "dBm";
        secondary.decimal_pos = 3u;
        secondary.has_value = !main_overrange &&
                              voltage_dbm_milli(main_value,
                                                dmm_p10_decimal_position(mode, range),
                                                &secondary.value);
        secondary.overrange = main_overrange || !secondary.has_value;
        return secondary;
    }

    if (mode_has_voltage_secondary_setup(mode)) {
        secondary.function = mode == DMM_MODE_MV_AC ? "ACmV" : "ACV";
        secondary.unit = mode == DMM_MODE_MV_AC ? "mV" : "V";
        secondary.value = dmm_p10_voltage_alt_secondary_value() / 10;
        if (secondary.value == 0 && !main_overrange) {
            secondary.value = main_value;
        }
        secondary.decimal_pos = stock_voltage_secondary_decimal(mode, range);
        secondary.has_value = !main_overrange;
        secondary.overrange = main_overrange;
        return secondary;
    }

    if (mode_has_burden_voltage_setup(mode) && setup->burden_voltage_enabled) {
        bool bank1 = dmm_p10_current_dual_bank_active();
        secondary.function = bank1 ? "AUX1" : "AUX0";
        secondary.unit = "A";
        if (!dmm_p10_current_dual_display_ready()) {
            secondary.has_value = false;
            return secondary;
        }
        secondary.value = bank1 ? dmm_p10_current_dual_bank1_value()
                                : dmm_p10_current_dual_bank0_value();
        secondary.decimal_pos = stock_current_secondary_decimal(mode, bank1);
        secondary.has_value = true;
        return secondary;
    }

    if (mode_has_single_current_secondary_b(mode, setup)) {
        secondary.function = "B";
        secondary.unit = "V";
        secondary.value = dmm_p10_current_single_aux_value();
        secondary.decimal_pos = 1u;
        secondary.has_value = !dmm_p10_current_single_aux_overrange();
        secondary.overrange = dmm_p10_current_single_aux_overrange();
        return secondary;
    }

    return secondary;
}

static void log_build_measurement_csv(uint32_t sample, uint8_t mode, uint8_t range,
                                      bool has_value, bool overrange, int32_t value,
                                      int16_t temp_tenths_c,
                                      const setup_state_t *setup,
                                      char line[160]) {
    char *p = line;
    char *end = line + 159u;
    const char *function;
    const char *unit;
    log_mode_function_unit(mode, range, &function, &unit);
    log_secondary_t secondary =
        log_secondary_from_stock_state(mode, range, overrange, value, temp_tenths_c, setup);

    p = log_append_u32(p, end, sample);
    p = log_append_char(p, end, ',');
    p = log_append_text(p, end, function);
    p = log_append_char(p, end, ',');
    if (overrange) {
        p = log_append_text(p, end, "OFL");
    } else if (has_value) {
        p = log_append_fixed_value(p, end, value, dmm_p10_decimal_position(mode, range));
    }
    p = log_append_char(p, end, ',');
    p = log_append_text(p, end, unit);
    p = log_append_char(p, end, ',');
    p = log_append_text(p, end, secondary.function);
    p = log_append_char(p, end, ',');
    if (secondary.overrange) {
        p = log_append_text(p, end, "OFL");
    } else if (secondary.has_value) {
        p = log_append_fixed_value(p, end, secondary.value, secondary.decimal_pos);
    }
    p = log_append_char(p, end, ',');
    p = log_append_text(p, end, secondary.unit);
    p = log_append_text(p, end, "\r\n");
    *p = '\0';
}

static sd_fat_result_t log_append_measurement_csv(uint32_t sample, uint8_t mode, uint8_t range,
                                                  bool has_value, bool overrange, int32_t value,
                                                  int16_t temp_tenths_c,
                                                  const setup_state_t *setup,
                                                  const char name83[11]) {
    char line[160];
    log_build_measurement_csv(sample, mode, range, has_value, overrange, value,
                              temp_tenths_c, setup, line);
    return sd_fat_append_named_83_with_header(name83, 0, line);
}

static bool log_queue_measurement_csv(uint32_t sample, uint8_t mode, uint8_t range,
                                      bool has_value, bool overrange, int32_t value,
                                      int16_t temp_tenths_c,
                                      const setup_state_t *setup,
                                      const char name83[11]) {
    char line[160];
    log_build_measurement_csv(sample, mode, range, has_value, overrange, value,
                              temp_tenths_c, setup, line);
    return sd_card_task_append_named_83_with_header(name83, 0, line);
}

static void log_test_build_name83(uint8_t suffix, char name83[11]) {
    name83[0] = 'L';
    name83[1] = 'O';
    name83[2] = 'G';
    name83[3] = 'T';
    name83[4] = 'S';
    name83[5] = 'T';
    name83[6] = (char)('0' + suffix / 10u);
    name83[7] = (char)('0' + suffix % 10u);
    name83[8] = 'C';
    name83[9] = 'S';
    name83[10] = 'V';
}

static sd_fat_result_t log_test_select_name83(char name83[11]) {
    sd_fat_result_t last_rc = SD_FAT_ERR_FULL;
    for (uint8_t suffix = 0u; suffix < 100u; ++suffix) {
        log_test_build_name83(suffix, name83);
        bool exists = true;
        sd_fat_result_t rc = sd_fat_file_exists_83(name83, &exists);
        if (rc != SD_FAT_OK) {
            return rc;
        }
        last_rc = rc;
        if (!exists) {
            return SD_FAT_OK;
        }
    }
    log_test_build_name83(99u, name83);
    return last_rc == SD_FAT_OK ? SD_FAT_ERR_FULL : last_rc;
}

static void log_test_setup_from_cal(setup_state_t *setup) {
    *setup = (setup_state_t){
        .temp_fahrenheit = false,
        .apo_enabled = true,
        .buzzer_enabled = true,
        .burden_voltage_enabled = false,
        .dbm_enabled = false,
        .frontend_timing_enabled = true,
        .continuity_option = 0u,
        .lcd_contrast = 3u,
        .year = 2026u,
        .month = 5u,
        .day = 5u,
        .hour = 0u,
        .minute = 0u,
        .second = 0u,
        .meter_id = 1u,
        .logging_interval_s = 1u,
    };
    uint8_t saved_temp_unit = calibration_read_u8(CAL_OFFSET_TEMP_UNIT);
    if (saved_temp_unit < 2u) {
        setup->temp_fahrenheit = saved_temp_unit == 0u;
    }
    uint8_t saved_apo = calibration_read_u8(CAL_OFFSET_APO);
    if (saved_apo < 2u) {
        setup->apo_enabled = saved_apo == 0u;
    }
    uint8_t saved_buzzer = calibration_read_u8(CAL_OFFSET_BUZZER);
    if (saved_buzzer < 2u) {
        setup->buzzer_enabled = saved_buzzer != 0u;
    }
    uint16_t saved_log_interval = calibration_read_u16_be(CAL_OFFSET_LOG_INTERVAL);
    if (saved_log_interval < 1000u) {
        setup->logging_interval_s = saved_log_interval != 0u ? saved_log_interval : 1u;
    }
    (void)calibration_read_u8(CAL_OFFSET_BURDEN_VOLTAGE);
    (void)calibration_read_u8(CAL_OFFSET_DBM_MODE);
    setup->burden_voltage_enabled = false;
    setup->dbm_enabled = false;
}

bool app_sd_logging_test_all_modes(char name83[11], uint8_t *rows, uint8_t *rc_out) {
    static const uint8_t modes[] = {
        DMM_MODE_LOZ,
        DMM_MODE_V_DC, DMM_MODE_V_AC,
        DMM_MODE_MV_DC, DMM_MODE_MV_AC, DMM_MODE_TEMP,
        DMM_MODE_FREQ, DMM_MODE_PERIOD, DMM_MODE_DUTY,
        DMM_MODE_P10_OHMS, DMM_MODE_P10_CONT, DMM_MODE_P10_DIODE, DMM_MODE_P10_CAP,
        DMM_MODE_MVA_DC, DMM_MODE_MVA_AC,
        DMM_MODE_MA_DC, DMM_MODE_MA_AC, DMM_MODE_MA_BURDEN, DMM_MODE_MA_AUX,
        DMM_MODE_A_DC, DMM_MODE_A_AC, DMM_MODE_A_DC_PERSIST, DMM_MODE_A_AC_PERSIST,
    };

    if (name83 == 0 || rows == 0 || rc_out == 0) {
        return false;
    }
    *rows = 0;
    *rc_out = SD_FAT_ERR_IO;

    sd_fat_result_t rc = log_test_select_name83(name83);
    if (rc != SD_FAT_OK) {
        *rc_out = (uint8_t)rc;
        return false;
    }
    rc = sd_fat_append_named_83_with_header(name83, k_log_csv_header, "");
    if (rc != SD_FAT_OK) {
        *rc_out = (uint8_t)rc;
        return false;
    }

    setup_state_t setup;
    log_test_setup_from_cal(&setup);
    for (uint8_t i = 0u; i < sizeof(modes); ++i) {
        uint8_t mode = modes[i];
        uint8_t range = dmm_p10_default_range(mode);
        int32_t value = (int32_t)(10000 + (uint32_t)i * 137u);
        bool overrange = false;
        if (mode == DMM_MODE_P10_CAP) {
            value = 38;
        } else if (mode == DMM_MODE_P10_OHMS || mode == DMM_MODE_P10_CONT) {
            value = 1080;
        } else if (mode == DMM_MODE_P10_DIODE) {
            value = 6123;
        } else if (mode == DMM_MODE_TEMP) {
            value = 238;
        } else if (mode == DMM_MODE_PERIOD) {
            value = 8333;
        } else if (mode == DMM_MODE_DUTY) {
            value = 500;
        }
        rc = log_append_measurement_csv((uint32_t)i + 1u, mode, range, true, overrange,
                                        value, 238, &setup, name83);
        if (rc != SD_FAT_OK) {
            *rc_out = (uint8_t)rc;
            return false;
        }
        *rows = (uint8_t)(i + 1u);
    }
    *rc_out = SD_FAT_OK;
    return true;
}

static void lcd_write_mem_logging_count(uint32_t next_sample) {
    uint32_t logged = next_sample > 0u ? next_sample - 1u : 0u;
    if (logged >= 100000u) {
        lcd_write_upper_signed_numeric((int32_t)(logged / 10u), 2u);
    } else {
        lcd_write_upper_signed_numeric((int32_t)logged, 0u);
    }
}

static uint8_t p10_lcd_mode(uint8_t mode) {
    if (mode == DMM_MODE_V_AC && dmm_p10_voltage_alt_active()) {
        return DMM_MODE_V_ACDC;
    }
    return mode;
}

static uint8_t p10_bluetooth_mode(uint8_t mode) {
    if (mode == DMM_MODE_V_AC && dmm_p10_voltage_alt_active()) {
        return DMM_MODE_V_AC;
    }
    if (mode == DMM_MODE_MA_AUX) {
        return 0x0Du;
    }
    if (mode == DMM_MODE_MVA_AC) {
        return 0x0Fu;
    }
    if (mode == DMM_MODE_MA_AC) {
        return 0x10u;
    }
    if (mode == DMM_MODE_MA_DC) {
        return 0x11u;
    }
    if (mode == DMM_MODE_LEGACY_CURRENT_AC) {
        return 0x14u;
    }
    if (mode == DMM_MODE_MA_ALT_AC) {
        return 0x14u;
    }
    if (mode == DMM_MODE_A_AC) {
        return 0x14u;
    }
    if (mode == DMM_MODE_A_DC) {
        return 0x15u;
    }
    if (mode == DMM_MODE_MA_BURDEN) {
        return 0x16u;
    }
    if (mode == DMM_MODE_MVA_DC) {
        return 0x18u;
    }
    if (mode == DMM_MODE_A_DC_PERSIST) {
        return 0x15u;
    }
    if (mode == DMM_MODE_A_AC_PERSIST) {
        return 0x14u;
    }
    return mode & 0x1Fu;
}

static uint8_t p10_bluetooth_unit_scale(uint8_t mode, uint8_t range, int32_t value) {
    uint8_t base = (uint8_t)(range & 0x0Fu);
    uint8_t scale = base;
    if ((mode >= DMM_MODE_FREQ && mode <= DMM_MODE_DUTY) ||
        mode == DMM_MODE_P10_CAP || mode == DMM_MODE_TEMP) {
        scale = (uint8_t)(scale + 0x10u);
    }
    if (value < 0) {
        scale = (uint8_t)(scale + 0x04u);
    }
    if (base == 3u) {
        scale = (uint8_t)(scale + 0x08u);
    }
    return scale;
}

static uint8_t p10_bluetooth_bargraph_count(uint8_t mode, int32_t value, bool overrange) {
    uint8_t max_count = mode == DMM_MODE_P10_DIODE ? 15u : 25u;
    if (overrange) {
        return max_count;
    }
    uint32_t magnitude = value < 0 ? (uint32_t)(-(value + 1)) + 1u : (uint32_t)value;
    uint8_t count = (uint8_t)(magnitude / 2000u);
    if (count > 25u) {
        count = 25u;
    }
    return count > max_count ? max_count : count;
}

static bool p10_mode_has_live_bargraph(uint8_t mode) {
    return mode != DMM_MODE_P10_CAP &&
           mode != DMM_MODE_TEMP &&
           mode != DMM_MODE_FREQ &&
           mode != DMM_MODE_PERIOD &&
           mode != DMM_MODE_DUTY;
}

static uint8_t p10_bargraph_max_for_mode(uint8_t mode) {
    return mode == DMM_MODE_P10_DIODE ? 15u : 25u;
}

typedef struct {
    bool have_value;
    int32_t display_value;
    bool display_overrange;
    bool have_bargraph;
    bool bargraph_overrange;
    int32_t bargraph_value;
    uint32_t ohms_initial_open_until;
    uint32_t ohms_range_blank_until;
    bool ohms_waiting_for_range_value;
} p10_entry_display_state_t;

static bool p10_mode_seeds_zero_on_entry(uint8_t mode) {
    return mode == DMM_MODE_P10_CAP ||
           mode == DMM_MODE_FREQ ||
           mode == DMM_MODE_PERIOD ||
           mode == DMM_MODE_DUTY;
}

static p10_entry_display_state_t p10_make_entry_display_state(uint8_t mode, uint32_t now_ms) {
    p10_entry_display_state_t state = {
        .have_value = false,
        .display_value = 0,
        .display_overrange = false,
        .have_bargraph = p10_mode_has_live_bargraph(mode),
        .bargraph_overrange = false,
        .bargraph_value = 0,
        .ohms_initial_open_until = 0,
        .ohms_range_blank_until = 0,
        .ohms_waiting_for_range_value = false,
    };

    if (mode == DMM_MODE_P10_OHMS) {
        state.display_overrange = true;
        state.have_bargraph = true;
        state.bargraph_overrange = true;
        state.bargraph_value = 50000;
        state.ohms_initial_open_until = now_ms + OHMS_INITIAL_OPEN_HOLDOFF_MS;
        state.ohms_range_blank_until = now_ms + OHMS_RANGE_BLANK_MS;
    } else if (p10_mode_seeds_zero_on_entry(mode)) {
        state.have_value = true;
        state.display_value = 0;
        state.display_overrange = false;
        state.have_bargraph = false;
    }

    return state;
}

typedef struct {
    uint8_t lcd_mode;
    uint8_t lcd_range;
    uint8_t measurement_mode;
    uint8_t minmax_state;
    bool temperature_fahrenheit;
    bool autorange_enabled;
    bool rel_active;
    bool hold_active;
    bool peak_active;
    bool apo_enabled;
    bool battery_low;
    bool threshold_enabled;
    bool relative_delta;
    bool mem_annunciator;
    bool bt_annunciator;
} p10_lcd_status_t;

static void p10_render_common_lcd_status(const p10_lcd_status_t *status) {
    lcd_set_p10_mode_annunciators(status->lcd_mode, status->lcd_range);
    lcd_set_temperature_unit_annunciator(status->lcd_mode == DMM_MODE_TEMP,
                                         status->temperature_fahrenheit);
    lcd_set_auto_annunciator(status->autorange_enabled &&
                             !status->rel_active &&
                             !status->hold_active &&
                             !status->peak_active &&
                             status->measurement_mode != DMM_MODE_LOZ &&
                             status->measurement_mode != DMM_MODE_TEMP &&
                             status->measurement_mode != DMM_MODE_P10_CONT &&
                             status->measurement_mode != DMM_MODE_P10_DIODE &&
                             status->measurement_mode != DMM_MODE_DUTY);
    lcd_set_apo_annunciator(status->apo_enabled && status->minmax_state == 0u);
    lcd_set_low_battery_annunciator(status->battery_low);
    lcd_set_threshold_annunciator(status->threshold_enabled);
    lcd_set_relative_delta_annunciator(status->relative_delta);
    lcd_set_rel_hold_annunciator(status->hold_active ? 2u : 0u);
    lcd_set_minmax_annunciator(status->minmax_state);
    lcd_set_mem_annunciator(status->mem_annunciator);
    lcd_set_bt_annunciator(status->bt_annunciator);
}

static uint8_t p10_bluetooth_icon0(uint8_t mode, bool auto_range, bool apo_enabled) {
    uint8_t icons = 0u;
    if (mode == DMM_MODE_V_AC && dmm_p10_voltage_alt_active()) {
        icons = (uint8_t)(icons + 0x18u);
    } else if (mode == DMM_MODE_V_AC || mode == DMM_MODE_MV_AC ||
               mode == DMM_MODE_MA_AUX || mode == DMM_MODE_LEGACY_CURRENT_AC ||
               mode == DMM_MODE_MVA_AC || mode == DMM_MODE_MA_AC ||
               mode == DMM_MODE_A_AC || mode == DMM_MODE_A_AC_PERSIST ||
               mode == DMM_MODE_MA_ALT_AC) {
        icons = (uint8_t)(icons + 0x10u);
    } else if (mode == DMM_MODE_V_DC || mode == DMM_MODE_MV_DC ||
               mode == DMM_MODE_MA_DC || mode == DMM_MODE_A_DC ||
               mode == DMM_MODE_A_DC_PERSIST || mode == DMM_MODE_MA_BURDEN ||
               mode == DMM_MODE_MVA_DC) {
        icons = (uint8_t)(icons + 0x08u);
    }
    if (auto_range) {
        icons = (uint8_t)(icons + 0x04u);
    }
    if (apo_enabled) {
        icons = (uint8_t)(icons + 0x02u);
    }
    return icons;
}

static bool p10_mode_default_autorange(uint8_t mode) {
    return mode != DMM_MODE_P10_CONT && mode != DMM_MODE_P10_DIODE &&
           mode != DMM_MODE_DUTY && mode != DMM_MODE_LOZ && mode != DMM_MODE_TEMP;
}

static bool p10_manual_range_allowed(uint8_t mode) {
    return mode != DMM_MODE_P10_CONT && mode != DMM_MODE_DUTY &&
           mode != DMM_MODE_LOZ && mode != DMM_MODE_TEMP;
}

static bool p10_preserve_display_on_range_change(uint8_t mode) {
    return mode != DMM_MODE_P10_OHMS &&
           mode != DMM_MODE_P10_CONT &&
           mode != DMM_MODE_P10_DIODE &&
           mode != DMM_MODE_P10_CAP;
}

static bool p10_minmax_allowed(uint8_t mode, bool rel_active, bool hold_active,
                               bool voltage_alt_path) {
    return mode != DMM_MODE_LOZ && !rel_active &&
           !(hold_active && mode != DMM_MODE_P10_CAP) && !voltage_alt_path;
}

static bool abs_reaches_threshold(int32_t value, int32_t threshold) {
    return value >= threshold || value <= -threshold;
}

static bool p10_stock_threshold_annunciator(uint8_t mode, uint8_t range,
                                            int32_t value, bool overrange) {
    if (mode == DMM_MODE_V_DC || mode == DMM_MODE_V_AC) {
        if (range == 1u) {
            return overrange || abs_reaches_threshold(value, 30000);
        }
        if (range == 2u) {
            return overrange || abs_reaches_threshold(value, 3000);
        }
        if (range == 3u) {
            return overrange || abs_reaches_threshold(value, 300);
        }
    } else if (mode == DMM_MODE_LOZ) {
        return overrange || abs_reaches_threshold(value, 300);
    } else if ((mode == DMM_MODE_MA_AUX ||
                mode == DMM_MODE_LEGACY_CURRENT_AC ||
                mode == DMM_MODE_MVA_AC ||
                mode == DMM_MODE_MA_BURDEN ||
                mode == DMM_MODE_MA_ALT_AC ||
                mode == DMM_MODE_MVA_DC) &&
               (range == 1u || range == 3u)) {
        return overrange || abs_reaches_threshold(value, 30000);
    }
    return false;
}

static int32_t abs_i32_sat(int32_t value) {
    if (value == INT32_MIN) {
        return INT32_MAX;
    }
    return value < 0 ? -value : value;
}

static bool p10_hold_low_zone(uint8_t mode, int32_t value, bool overrange) {
    if (mode == DMM_MODE_P10_OHMS || mode == DMM_MODE_P10_CONT) {
        return value < HOLD_LOW_ZONE_COUNTS || overrange;
    }
    return value < HOLD_LOW_ZONE_COUNTS && value > -HOLD_LOW_ZONE_COUNTS;
}

static bool p10_hold_stable_sample(uint8_t mode, int32_t sample, bool overrange,
                                   bool previous_overrange, int32_t previous_value,
                                   bool *low_zone_seen, bool *candidate_active,
                                   int32_t *candidate_value) {
    if (p10_hold_low_zone(mode, sample, overrange)) {
        *low_zone_seen = true;
        *candidate_active = false;
        return false;
    }

    if (*low_zone_seen) {
        *low_zone_seen = false;
        *candidate_active = false;
    }

    if (overrange && (mode == DMM_MODE_P10_OHMS || mode == DMM_MODE_P10_CONT)) {
        *candidate_active = false;
        return !previous_overrange;
    }
    if (overrange) {
        *candidate_active = false;
        return false;
    }

    if (!previous_overrange &&
        abs_i32_sat(sample - previous_value) < HOLD_RECAPTURE_DELTA_COUNTS) {
        *candidate_active = false;
        return false;
    }

    if (*candidate_active) {
        if (abs_i32_sat(sample - *candidate_value) <= HOLD_STABLE_DELTA_COUNTS) {
            *candidate_active = false;
            return true;
        }
        *candidate_active = false;
        return false;
    }

    *candidate_value = sample;
    *candidate_active = true;
    return false;
}

static bool p10_stock_lead_valid(uint8_t mode, uint8_t jack_state) {
    if (jack_state != 1u && jack_state != 2u) {
        return true;
    }
    if (mode == DMM_MODE_A_AC || mode == DMM_MODE_A_DC ||
        mode == DMM_MODE_A_AC_PERSIST || mode == DMM_MODE_A_DC_PERSIST) {
        return true;
    }
    if (mode == DMM_MODE_LEGACY_CURRENT_AC || mode == DMM_MODE_MVA_AC ||
        mode == DMM_MODE_MA_ALT_AC || mode == DMM_MODE_MVA_DC) {
        return true;
    }
    if ((mode == DMM_MODE_MA_AC || mode == DMM_MODE_MA_DC ||
         mode == DMM_MODE_MA_AUX || mode == DMM_MODE_MA_BURDEN) &&
        jack_state != 2u) {
        return true;
    }
    return false;
}

static bool p10_stock_lead_warning(uint8_t mode, uint8_t jack_state) {
    return !p10_stock_lead_valid(mode, jack_state);
}

static bool p10_stock_update_jack_variant(uint8_t *mode, bool jack_state1_active) {
    uint8_t next = *mode;
    if (*mode == DMM_MODE_A_DC_PERSIST && jack_state1_active) {
        next = DMM_MODE_A_AC;
    } else if (*mode == DMM_MODE_A_AC_PERSIST && jack_state1_active) {
        next = DMM_MODE_A_DC;
    } else if (*mode == DMM_MODE_MVA_AC && jack_state1_active) {
        next = DMM_MODE_LEGACY_CURRENT_AC;
    } else if (*mode == DMM_MODE_MVA_DC && jack_state1_active) {
        next = DMM_MODE_MA_ALT_AC;
    } else if (*mode == DMM_MODE_A_AC && !jack_state1_active) {
        next = DMM_MODE_A_DC_PERSIST;
    } else if (*mode == DMM_MODE_A_DC && !jack_state1_active) {
        next = DMM_MODE_A_AC_PERSIST;
    } else if (*mode == DMM_MODE_LEGACY_CURRENT_AC && !jack_state1_active) {
        next = DMM_MODE_MVA_AC;
    } else if (*mode == DMM_MODE_MA_ALT_AC && !jack_state1_active) {
        next = DMM_MODE_MVA_DC;
    }
    if (next == *mode) {
        return false;
    }
    *mode = next;
    return true;
}

static bool rotary_position_is_onehot(uint8_t rotary) {
    return rotary == LOWZ_ROTARY_MASK || rotary == V_ROTARY_MASK ||
           rotary == MV_TEMP_ROTARY_MASK || rotary == FREQ_ROTARY_MASK ||
           rotary == P10_ROTARY_MASK || rotary == UA_ROTARY_MASK ||
           rotary == MA_ROTARY_MASK || rotary == A_ROTARY_MASK;
}

static uint8_t p10_default_mode_for_rotary(uint8_t rotary) {
    return rotary_mode_task_default(rotary);
}

static void p10_persist_mode_for_rotary(uint8_t rotary, uint8_t mode) {
    (void)rotary_mode_task_persist(rotary, mode);
}

static void apoff_prepare_final_low_power(void) {
    sdio_sd_end_session();
    analog_power_down();
    frontend_power_down();
    bluetooth_power_down();
    lcd_power_down();

    REG32(TIM_CR1(TIM2_BASE)) = 0;
    REG32(TIM_CR1(TIM3_BASE)) = 0;
    REG32(RCC_APB1ENR) &= ~(BIT(0) | BIT(1));

    REG32(RCC_APB1ENR) |= PWR_APB1EN;
    (void)REG32(RCC_APB1ENR);
    REG32(PWR_CR) |= PWR_CR_CWUF;
    REG32(PWR_CR) = (REG32(PWR_CR) & ~PWR_CR_VOS_MASK) | PWR_CR_VOS_MASK;

    REG32(SYST_CSR) = 0;
    REG32(SCB_SCR) |= SCB_SCR_SLEEPDEEP;
    cpu_dsb_isb();
}

int main(void) {
    (void)clock_init_stock_msi_4mhz();
    clock_init_systick_1ms();
    cpu_enable_irq();
    task_runtime_init(clock_millis());

    gpio_enable_all_clocks();
    uint8_t startup_lcd_contrast = setup_load_lcd_contrast();
    lcd_init_blank(startup_lcd_contrast);
    buzzer_init();
    lcd_show_startup_version();
    input_init();
    input_task_poll(clock_millis());
    backlight_init();

#if EEV_LCD_ONLY_TEST
    while (1) {
        buzzer_off();
        lcd_blank();
        __asm__ volatile("wfi");
    }
#elif EEV_LCD_TEXT_TEST
    lcd_clear_shadow();
    lcd_write_5chars(true, "HELLO");
    lcd_commit();
    while (1) {
        buzzer_off();
        __asm__ volatile("wfi");
    }
#elif EEV_INPUT_TEST
    uint32_t next_input_update = 0;
    uint8_t last_rotary = 0xFFu;
    uint8_t last_jack = 0xFFu;
    uint8_t last_buttons = 0xFFu;
    while (1) {
        buzzer_off();
        uint32_t now = clock_millis();
        if ((int32_t)(now - next_input_update) >= 0) {
            uint8_t rotary = input_rotary_sample_active_low();
            uint8_t jack = input_jack_sample_active_low();
            uint8_t buttons = input_button_sample_active_low();
            if (rotary != last_rotary || jack != last_jack || buttons != last_buttons) {
                char upper[5];
                char lower[5];
                format_hex_line('P', rotary, upper);
                if (buttons != 0u) {
                    format_hex_line('B', buttons, lower);
                } else {
                    format_jack_label(jack, lower);
                }
                lcd_clear_shadow();
                lcd_write_5chars(true, upper);
                lcd_write_5chars(false, lower);
                lcd_commit();
                last_rotary = rotary;
                last_jack = jack;
                last_buttons = buttons;
            }
            next_input_update = now + 100u;
        }
        __asm__ volatile("wfi");
    }
#elif EEV_UART_PIN_TEST
    bluetooth_init_uart(clock_apb1_hz(), 115200u, false);

    lcd_clear_shadow();
    lcd_write_5chars(true, "U115 ");
    lcd_write_5chars(false, "BLEOF");
    lcd_set_bt_annunciator(false);
    lcd_commit();
    buzzer_on_ms(30u);

    uint32_t next_uart_pin_tx = 0;
    uint32_t uart_pin_seq = 0;
    while (1) {
        buzzer_poll();
        uint32_t now = clock_millis();
        uint8_t rx = 0;

        while (bluetooth_receive_byte(&rx)) {
            bluetooth_send_uart_pin_rx(rx);
            if (rx == (uint8_t)'?' || rx == (uint8_t)'p' || rx == (uint8_t)'P') {
                bluetooth_send_text("UART115 PONG\r\n");
            }
        }

        if ((int32_t)(now - next_uart_pin_tx) >= 0) {
            bluetooth_send_text("UART115 seq=");
            bluetooth_send_dec_u32(uart_pin_seq++);
            bluetooth_send_text(" ms=");
            bluetooth_send_dec_u32(now);
            bluetooth_send_text(" baud=115200 8N1 tx=PD5 rx=PD6 ble=off\r\n");
            next_uart_pin_tx = now + 1000u;
        }

        __asm__ volatile("wfi");
    }
#elif EEV_BLE_LINK_TEST
    bluetooth_init_19200(clock_apb1_hz());

    lcd_clear_shadow();
    lcd_write_5chars(true, "BLE19");
    lcd_write_5chars(false, "19200");
    lcd_set_bt_annunciator(true);
    lcd_commit();
    buzzer_on_ms(30u);

    uint32_t next_ble_link_tx = 0;
    uint8_t ble_link_frame[19];
    while (1) {
        buzzer_poll();
        uint32_t now = clock_millis();

        if ((int32_t)(now - next_ble_link_tx) >= 0 && !bluetooth_tx_busy()) {
            make_ble_link_frame_19(ble_link_frame);
            (void)bluetooth_send_bytes_irq(ble_link_frame, sizeof(ble_link_frame));
            next_ble_link_tx = now + 100u;
        }

        __asm__ volatile("wfi");
    }
#elif EEV_DIODE_MODE_TEST
    frontend_init_pins();
    bluetooth_init_uart(clock_apb1_hz(), 115200u, false);
    uart_update_init();
    analog_aux_adc_init();
#if EEV_BLE_BAUD_SWEEP || EEV_BLE_FIXED_38400 || EEV_BLE_FIXED_19200
#if EEV_BLE_FIXED_19200
    bluetooth_set_baud(32000000u, 38400u);
    lcd_clear_shadow();
    lcd_write_5chars(true, "F2IRQ");
    lcd_write_5chars(false, clock_ok ? "MSI4 " : "CLKER");
    lcd_commit();

    uint32_t seq = 0;
    uint8_t tx_frame[53];
    while (1) {
        make_ble_app_ascii_53(seq++, tx_frame);
        while (!bluetooth_send_bytes_irq(tx_frame, sizeof(tx_frame))) {
        }
        while (bluetooth_tx_busy()) {
            __asm__ volatile("wfi");
        }
        delay_ms(250u);
    }
#else
#if EEV_BLE_FIXED_38400
    static const ble_baud_candidate_t candidates[] = {
        {"P32_B38400", 32000000u, 38400u},
    };
#else
    static const ble_baud_candidate_t candidates[] = {
        {"P32_B19200", 32000000u, 19200u},
        {"P24_B19200", 24000000u, 19200u},
        {"P16_B19200", 16000000u, 19200u},
        {"P8_B19200", 8000000u, 19200u},
        {"P16_B9600", 16000000u, 9600u},
        {"P24_B38400", 24000000u, 38400u},
        {"P16_B38400", 16000000u, 38400u},
        {"P32_B38400", 32000000u, 38400u},
        {"P32_B9600", 32000000u, 9600u},
        {"P24_B57600", 24000000u, 57600u},
        {"P16_B57600", 16000000u, 57600u},
        {"P32_B57600", 32000000u, 57600u},
        {"P16_B76800", 16000000u, 76800u},
        {"P32_B76800", 32000000u, 76800u},
        {"P16_B115200", 16000000u, 115200u},
        {"P32_B115200", 32000000u, 115200u},
        {"P16_B230400", 16000000u, 230400u},
        {"P32_B230400", 32000000u, 230400u},
        {"P16_B460800", 16000000u, 460800u},
        {"P32_B460800", 32000000u, 460800u},
    };
#endif
    uint32_t seq = 0;
    uint8_t candidate_index = 0;
    uint8_t tx_frame[50];
    while (1) {
        const ble_baud_candidate_t *candidate = &candidates[candidate_index];
        while (bluetooth_tx_busy()) {
        }
        bluetooth_set_baud(candidate->pclk_hz, candidate->baud);
        lcd_clear_shadow();
        lcd_write_5chars(true, "BAUD ");
        lcd_write_5chars(false, candidate->name);
        lcd_commit();
        for (uint8_t repeat = 0; repeat < 4u; ++repeat) {
            make_baud_test_50(seq++, candidate, tx_frame);
            while (!bluetooth_send_bytes_irq(tx_frame, sizeof(tx_frame))) {
            }
            while (bluetooth_tx_busy()) {
                __asm__ volatile("wfi");
            }
            delay_ms(250u);
        }
        candidate_index++;
        if (candidate_index >= (uint8_t)(sizeof(candidates) / sizeof(candidates[0]))) {
            candidate_index = 0;
        }
    }
#endif
#endif

    measurement_controller_t measurement;
    measurement_controller_init(&measurement, DMM_MODE_P10_OHMS,
                                dmm_p10_default_range(DMM_MODE_P10_OHMS));
    uint8_t last_rotary = 0xFFu;
    uint8_t last_display_mode = 0xFFu;
    uint8_t last_display_range = 0xFFu;
    uint8_t applied_p10_override_cookie = 0xFFu;
    bool have_value = false;
    bool display_overrange = false;
    int32_t display_value = 0;
    bool have_bargraph = false;
    bool bargraph_overrange = false;
    int32_t bargraph_value = 0;
    bool bargraph_display_dirty = true;
    bool bargraph_primary_numeric_visible = false;
    bool range_display_settling = false;
    bool measurement_display_dirty = true;
    bool secondary_display_dirty = true;
    int16_t last_secondary_temperature_display = INT16_MIN;
    uint8_t last_setup_battery_percent = 0xFFu;
    uint32_t lcdtest_next_step = 0;
    uint16_t lcdtest_last_index = 0xFFFFu;
    bool lcdtest_was_active = false;
    uint32_t ohms_initial_open_until = 0;
    uint32_t ohms_range_blank_until = 0;
    bool ohms_waiting_for_range_value = false;
    uint32_t bt_show_until = 0;
    uint8_t bt_last_rx = 0;
    bool bt_has_recent_rx = false;
    bool p10_position_active = false;
    bool diode_voltage_label_visible = false;
    bool rel_active = false;
    bool rel_capture_pending = false;
    bool rel_restore_autorange = true;
    int32_t rel_baseline = 0;
    bool hold_active = false;
    bool hold_has_value = false;
    bool hold_overrange = false;
    int32_t hold_value = 0;
    bool peak_active = false;
    bool peak_has_value = false;
    bool peak_overrange = false;
    int32_t peak_value = 0;
    bool hold_low_zone_seen = false;
    bool hold_candidate_active = false;
    int32_t hold_candidate_value = 0;
    uint8_t minmax_state = 0;
    bool minmax_restore_autorange = true;
    int32_t minmax_max = INT32_MIN;
    int32_t minmax_min = INT32_MAX;
    int32_t minmax_avg = 0;
    int32_t minmax_avg_remainder = 0;
    uint32_t minmax_count = 0;
    bool minmax_max_overrange = false;
    bool minmax_min_overrange = false;
    bool mem_logging_restore_autorange = true;
    uint8_t mem_log_mode = 0;
    uint8_t mem_log_range = 0;
    bool mem_log_have_value = false;
    bool mem_log_overrange = false;
    int32_t mem_log_value = 0;
    uint32_t mode_press_started = 0;
    bool mode_long_handled = false;
    bool mode_button_down = false;
    uint32_t minmax_press_started = 0;
    bool minmax_long_handled = false;
    uint32_t mem_press_started = 0;
    bool mem_long_handled = false;
    uint32_t range_press_started = 0;
    bool range_long_handled = false;
    uint32_t down_press_started = 0;
    bool down_long_handled = false;
    uint32_t setup_press_started = 0;
    bool setup_long_handled = false;
    bool setup_active = false;
    bool setup_page_visible = false;
    bool setup_editing = false;
    uint8_t setup_edit_subfield = 0u;
    setup_page_t setup_page = SETUP_PAGE_TEMP;
    uint32_t setup_timeout_at = 0;
    setup_state_t setup_edit_snapshot;
    bool setup_edit_snapshot_valid = false;
    int16_t temperature_centideg_c = 2500;
    int16_t temperature_tenths_c =
        stock_aux_temperature_display_tenths_from_centideg(temperature_centideg_c, false);
    dmm_p10_set_cold_junction_temperature((int16_t)(temperature_centideg_c / 10));
    uint16_t battery_decivolts = 0;
    uint8_t battery_percent = 0;
    bool battery_ok = false;
    bool battery_low = false;
    bool battery_low_message_seen = false;
    uint32_t rtc_last_second_tick = clock_millis();
    uint8_t active_measure_rotary = 0xFFu;
    uint8_t rotary_stable = input_task_rotary();
    if (!rotary_position_is_onehot(rotary_stable)) {
        rotary_stable = 0u;
    }
    uint8_t pending_mode_rotary = 0xFFu;
    uint8_t pending_mode_value = 0xFFu;
    bool pending_mode_dirty = false;
    uint8_t jack_debounced_state = input_task_jack() & 0x03u;
    bool jack_state1_active = jack_debounced_state == 1u;
    bool lead_warning_active = false;

#define P10_CLEAR_INTERACTION_STATE() do { \
    diode_voltage_label_visible = false; \
    rel_active = false; \
    rel_capture_pending = false; \
    hold_active = false; \
    peak_active = false; \
    peak_has_value = false; \
    minmax_state = 0; \
} while (0)

#define P10_INVALIDATE_MEASUREMENT_STATE() do { \
    measurement_controller_invalidate_configuration(&measurement); \
    have_value = false; \
    display_overrange = false; \
    have_bargraph = false; \
    bargraph_display_dirty = true; \
    bargraph_primary_numeric_visible = false; \
    ohms_range_blank_until = 0; \
} while (0)

#define P10_SET_DEFAULT_RANGE_FOR_MODE() do { \
    measurement_controller_set_range(&measurement, dmm_p10_default_range(measurement.mode), true); \
    dmm_p10_set_autorange(p10_mode_default_autorange(measurement.mode)); \
} while (0)

#define P10_MARK_LCD_IDENTITY_DIRTY() do { \
    last_display_mode = 0xFFu; \
    last_display_range = 0xFFu; \
} while (0)

    setup_state_t setup = {
        .temp_fahrenheit = false,
        .apo_enabled = true,
        .buzzer_enabled = true,
        .burden_voltage_enabled = false,
        .dbm_enabled = false,
        .frontend_timing_enabled = true,
        .continuity_option = 0u,
        .lcd_contrast = startup_lcd_contrast,
        .year = 2026u,
        .month = 4u,
        .day = 30u,
        .hour = 0u,
        .minute = 0u,
        .second = 0u,
        .meter_id = 1u,
        .logging_interval_s = 0u,
    };
    uint8_t continuity_option = calibration_read_u8(CAL_OFFSET_CONTINUITY_OPTION);
    if (continuity_option <= 3u) {
        setup.continuity_option = continuity_option;
    }
    uint8_t saved_temp_unit = calibration_read_u8(CAL_OFFSET_TEMP_UNIT);
    if (saved_temp_unit < 2u) {
        setup.temp_fahrenheit = saved_temp_unit == 0u;
    }
    uint8_t saved_apo = calibration_read_u8(CAL_OFFSET_APO);
    if (saved_apo < 2u) {
        setup.apo_enabled = saved_apo == 0u;
    }
    apo_task_set_enabled(setup.apo_enabled, clock_millis());
    uint8_t saved_buzzer = calibration_read_u8(CAL_OFFSET_BUZZER);
    if (saved_buzzer < 2u) {
        setup.buzzer_enabled = saved_buzzer != 0u;
    }
    uint32_t saved_meter_id = calibration_read_u32_be(CAL_OFFSET_METER_ID);
    if (saved_meter_id < 100000u) {
        setup.meter_id = saved_meter_id;
    }
    uint16_t saved_log_interval = calibration_read_u16_be(CAL_OFFSET_LOG_INTERVAL);
    if (saved_log_interval < 1000u) {
        setup.logging_interval_s = saved_log_interval;
    } else {
        setup.logging_interval_s = 1u;
    }
    setup.burden_voltage_enabled = calibration_read_u8(CAL_OFFSET_BURDEN_VOLTAGE) == 1u;
    setup.dbm_enabled = calibration_read_u8(CAL_OFFSET_DBM_MODE) == 1u;
    dmm_p10_set_continuity_option(setup.continuity_option);
    dmm_p10_set_burden_voltage_enabled(setup.burden_voltage_enabled);
    dmm_p10_set_frontend_timing_toggle_enabled(setup.frontend_timing_enabled);
    buzzer_set_enabled(setup.buzzer_enabled);
    uint8_t saved_diode_voltage = calibration_read_u8(CAL_OFFSET_DIODE_VOLTAGE);
    if (saved_diode_voltage == 1u && !dmm_p10_diode_option_15()) {
        dmm_p10_toggle_diode_option();
    }
    bluetooth_app_task_init();

    lcd_blank();

    while (1) {
        uint32_t now = clock_millis();
        task_runtime_poll(now);
        now = clock_millis();
        bool measurement_boundary_seen = false;
        input_task_snapshot_t input_snapshot;
        input_task_get_snapshot(&input_snapshot);
        while ((uint32_t)(now - rtc_last_second_tick) >= 1000u) {
            rtc_last_second_tick += 1000u;
            if (!(setup_active && setup_editing &&
                  (setup_page == SETUP_PAGE_YEAR ||
                   setup_page == SETUP_PAGE_DATE ||
                   setup_page == SETUP_PAGE_TIME))) {
                setup_rtc_tick_one_second(&setup);
            }
        }
        bluetooth_app_event_t bt_event;
        while (bluetooth_app_task_take_event(&bt_event)) {
            if (bt_event.kind == BLUETOOTH_APP_EVENT_UART_COMMAND) {
                bt_last_rx = bt_event.value;
            } else if (bt_event.kind == BLUETOOTH_APP_EVENT_BUTTON) {
                input_task_inject_button_pulse(bt_event.value, now, 80u);
                bt_last_rx = bt_event.value;
                bt_has_recent_rx = true;
                bt_show_until = now + 400u;
            } else if (bt_event.kind == BLUETOOTH_APP_EVENT_DATETIME) {
                bt_app_apply_f8_datetime(&setup, bt_event.datetime);
                rtc_last_second_tick = now;
                bt_last_rx = 0xF8u;
                bt_has_recent_rx = true;
                bt_show_until = now + 400u;
            }
        }

        lead_warning_task_set_active(lead_warning_active, now);

        if (uart_update_lcdtest_active()) {
            p10_measure_task_set_active(false);
            p10_measure_task_flush();
            uint16_t period = uart_update_lcdtest_period_ms();
            if (!lcdtest_was_active) {
                lcdtest_was_active = true;
                lcdtest_last_index = 0xFFFFu;
                lcdtest_next_step = now + period;
            }
            if (period != 0u && (int32_t)(now - lcdtest_next_step) >= 0) {
                do {
                    uart_update_lcdtest_advance();
                    lcdtest_next_step += period;
                } while ((int32_t)(now - lcdtest_next_step) >= 0);
            }
            uint16_t lcdtest_index = uart_update_lcdtest_index();
            if (lcdtest_index != lcdtest_last_index) {
                lcd_show_raw_segment_test(lcdtest_index);
                lcdtest_last_index = lcdtest_index;
            }
            __asm__ volatile("wfi");
            continue;
        }
        if (lcdtest_was_active) {
            lcdtest_was_active = false;
            lcdtest_last_index = 0xFFFFu;
            lcd_clear_shadow();
            last_display_mode = 0xFFu;
        }

        uint8_t rotary_sample = input_snapshot.raw_rotary;
        g_app_diag_rotary = rotary_sample;
        g_app_diag_p10_mode = measurement.mode;
        g_app_diag_p10_range = measurement.range;
        if (pending_mode_dirty && !rotary_position_is_onehot(rotary_sample)) {
            p10_persist_mode_for_rotary(pending_mode_rotary, pending_mode_value);
            pending_mode_dirty = false;
            pending_mode_rotary = 0xFFu;
            pending_mode_value = 0xFFu;
        }
        if (input_snapshot.rotary_changed) {
            rotary_stable = input_snapshot.rotary;
        }
        uint8_t rotary = rotary_stable;
        if (bt_has_recent_rx && (int32_t)(now - bt_show_until) >= 0) {
            bt_has_recent_rx = false;
            last_display_mode = 0xFFu;
        }
        apo_task_set_enabled(setup.apo_enabled, now);
        apo_task_set_suspended(setup_active || rel_active || hold_active, now);
        apo_task_event_t apo_event;
        while (apo_task_take_event(&apo_event)) {
            if (apo_event == APO_TASK_EVENT_WARNING_BEEP) {
                buzzer_on_ms(80u);
            } else if (apo_event == APO_TASK_EVENT_TIMEOUT) {
                if (pending_mode_dirty) {
                    p10_persist_mode_for_rotary(pending_mode_rotary, pending_mode_value);
                    pending_mode_dirty = false;
                    pending_mode_rotary = 0xFFu;
                    pending_mode_value = 0xFFu;
                }
                lcd_clear_shadow();
                lcd_write_5chars(true, "APOFF");
                lcd_write_5chars(false, "     ");
                lcd_set_apo_annunciator(false);
                lcd_commit();
                backlight_set(false);
                buzzer_on_ms(160u);
                uint32_t apoff_powerdown_at = now + 220u;
                while ((int32_t)(clock_millis() - apoff_powerdown_at) < 0) {
                    buzzer_poll();
                    __asm__ volatile("wfi");
                }
                apoff_prepare_final_low_power();
                while (1) {
                    __asm__ volatile("wfi");
                }
            }
        }
        if (status_task_take_expired()) {
            last_display_mode = 0xFFu;
        }
        if (setup_active && (int32_t)(now - setup_timeout_at) >= 0) {
            if (setup_editing && setup_edit_snapshot_valid) {
                setup = setup_edit_snapshot;
                setup_apply_runtime_options(&setup);
                setup_edit_snapshot_valid = false;
            }
            setup_active = false;
            setup_editing = false;
            setup_edit_subfield = 0u;
            last_display_mode = 0xFFu;
        }
        dual_secondary_task_set_active(mode_has_dual_current_voltage_secondary(measurement.mode), now);
        if (dual_secondary_task_take_changed()) {
            last_display_mode = 0xFFu;
        }

        bool measure_position = rotary_position_is_onehot(rotary);
        if (pending_mode_dirty &&
            (!measure_position || active_measure_rotary != rotary)) {
            p10_persist_mode_for_rotary(pending_mode_rotary, pending_mode_value);
            pending_mode_dirty = false;
            pending_mode_rotary = 0xFFu;
            pending_mode_value = 0xFFu;
        }
        if (!measure_position) {
            p10_measure_task_set_active(false);
            p10_measure_task_flush();
            p10_position_active = false;
            active_measure_rotary = 0xFFu;
            P10_CLEAR_INTERACTION_STATE();
            setup_active = false;
            setup_editing = false;
            setup_edit_subfield = 0u;
            if (setup_edit_snapshot_valid) {
                setup = setup_edit_snapshot;
                setup_apply_runtime_options(&setup);
                setup_edit_snapshot_valid = false;
            }
            P10_INVALIDATE_MEASUREMENT_STATE();
            range_display_settling = false;
            if (rotary != last_rotary || display_task_update_due(now)) {
                char upper[5];
                format_hex_line('P', rotary, upper);
                lcd_clear_shadow();
                lcd_write_5chars(true, upper);
                lcd_write_5chars(false, "DMM? ");
                lcd_set_bt_annunciator(false);
                lcd_commit();
                last_rotary = rotary;
                display_task_schedule_after(now, 250u);
            }
            __asm__ volatile("wfi");
            continue;
        }

        if (!p10_position_active || active_measure_rotary != rotary) {
            p10_position_active = true;
            active_measure_rotary = rotary;
            measurement.mode = p10_default_mode_for_rotary(rotary);
            P10_INVALIDATE_MEASUREMENT_STATE();
            P10_CLEAR_INTERACTION_STATE();
            (void)p10_stock_update_jack_variant(&measurement.mode, jack_state1_active);
            P10_SET_DEFAULT_RANGE_FOR_MODE();
            lead_warning_active = p10_stock_lead_warning(measurement.mode, jack_debounced_state);
            apo_task_reset(false, now);
            P10_MARK_LCD_IDENTITY_DIRTY();
        }

        if (uart_update_p10_override_active()) {
            uint8_t override_mode = uart_update_p10_override_mode();
            uint8_t override_range = uart_update_p10_override_range();
            uint8_t override_cookie = uart_update_p10_override_cookie();
            bool new_override = override_cookie != applied_p10_override_cookie;
            if (measurement.mode != override_mode || new_override) {
                measurement.mode = override_mode;
                if (new_override) {
                    measurement_controller_set_range(&measurement, override_range, true);
                    applied_p10_override_cookie = override_cookie;
                }
                dmm_p10_set_autorange(p10_mode_default_autorange(measurement.mode));
                P10_INVALIDATE_MEASUREMENT_STATE();
                P10_CLEAR_INTERACTION_STATE();
                lead_warning_active = p10_stock_lead_warning(measurement.mode, jack_debounced_state);
                last_display_mode = 0xFFu;
            }
        }

        {
            uint8_t buttons = input_snapshot.buttons;
            uint8_t button_pressed = input_snapshot.button_pressed;
            uint8_t button_released = input_snapshot.button_released;
            g_app_diag_buttons = buttons;
            bool setup_changed =
                ((button_pressed | button_released) & BUTTON_SETUP_MASK) != 0u;
            bool mode_pressed = (buttons & BUTTON_MODE_MASK) != 0u;
            bool setup_pressed = (buttons & BUTTON_SETUP_MASK) != 0u;
            bool setup_was_pressed = (button_released & BUTTON_SETUP_MASK) != 0u;
            bool minmax_pressed = (button_pressed & BUTTON_MINMAX_MASK) != 0u;
            bool minmax_held = (buttons & BUTTON_MINMAX_MASK) != 0u;
            bool mem_pressed = (button_pressed & BUTTON_MEM_MASK) != 0u;
            bool mem_held = (buttons & BUTTON_MEM_MASK) != 0u;
            bool range_pressed = (button_pressed & BUTTON_RANGE_MASK) != 0u;
            bool range_held = (buttons & BUTTON_RANGE_MASK) != 0u;
            bool hold_pressed = (button_pressed & BUTTON_HOLD_MASK) != 0u;
            bool up_pressed = (button_pressed & BUTTON_UP_MASK) != 0u;
            bool down_pressed = (button_pressed & BUTTON_DOWN_MASK) != 0u;
            bool down_held = (buttons & BUTTON_DOWN_MASK) != 0u;
            bool down_released = (button_released & BUTTON_DOWN_MASK) != 0u;
            if (buttons != 0u) {
                apo_task_reset(minmax_state != 0u, now);
            }
            uint8_t jack_sample = input_snapshot.jack & 0x03u;
            bool jack_stable_changed =
                input_snapshot.jack_changed || jack_debounced_state != jack_sample;
            if (jack_stable_changed) {
                jack_debounced_state = jack_sample;
                jack_state1_active = jack_sample == 1u;
            }
            if (jack_stable_changed) {
                if (p10_stock_update_jack_variant(&measurement.mode, jack_state1_active)) {
                    P10_SET_DEFAULT_RANGE_FOR_MODE();
                    P10_INVALIDATE_MEASUREMENT_STATE();
                    P10_CLEAR_INTERACTION_STATE();
                }
                apo_task_reset(false, now);
                lead_warning_active = p10_stock_lead_warning(measurement.mode, jack_debounced_state);
                last_display_mode = 0xFFu;
            }

            if (mode_pressed && !mode_button_down) {
                mode_button_down = true;
                mode_press_started = now;
                mode_long_handled = false;
            }
            if (setup_changed && setup_pressed && !setup_was_pressed) {
                setup_press_started = now;
                setup_long_handled = false;
            }
            if (setup_pressed && !setup_long_handled &&
                (uint32_t)(now - setup_press_started) >= SETUP_LONG_PRESS_MS) {
                bool was_editing = setup_editing;
                if (!setup_page_visible) {
                    setup_active = true;
                    setup_page_visible = true;
                    setup_page = SETUP_PAGE_TEMP;
                    setup_editing = false;
                    setup_edit_subfield = 0u;
                } else {
                    setup_active = true;
                    if (setup_editing && setup_page_has_subfields(setup_page) &&
                        setup_edit_subfield == 0u) {
                        setup_edit_subfield = 1u;
                    } else {
                        if (!setup_editing &&
                            !setup_page_editable_for_mode(setup_page, measurement.mode)) {
                            setup_editing = false;
                        } else {
                            setup_editing = !setup_editing;
                            if (setup_editing) {
                                setup_edit_snapshot = setup;
                                setup_edit_snapshot_valid = true;
                            }
                        }
                        setup_edit_subfield = 0u;
                    }
                }
                bool persisted = true;
                if (was_editing && !setup_editing) {
                    persisted = setup_persist_page(&setup, setup_page, measurement.mode);
                    setup_edit_snapshot_valid = false;
                }
                setup_timeout_at = now + SETUP_TIMEOUT_MS;
                setup_long_handled = true;
                last_display_mode = 0xFFu;
                buzzer_on_ms(setup_editing ? 40u : (persisted ? 20u : 80u));
            }
            if (setup_changed && !setup_pressed && setup_was_pressed && !setup_long_handled) {
                bool was_visible = setup_page_visible;
                setup_active = true;
                setup_page_visible = true;
                if (was_visible && setup_editing && setup_page == SETUP_PAGE_ID) {
                    setup_edit_subfield = (uint8_t)((setup_edit_subfield + 1u) % 5u);
                } else if (was_visible && !setup_editing) {
                    setup_page = setup_next_page_for_mode(setup_page, measurement.mode, &setup);
                    setup_edit_subfield = 0u;
                } else if (!was_visible) {
                    setup_page = setup_next_page_for_mode(SETUP_PAGE_TEMP, measurement.mode, &setup);
                    setup_editing = false;
                    setup_edit_subfield = 0u;
                }
                setup_timeout_at = now + SETUP_TIMEOUT_MS;
                last_display_mode = 0xFFu;
                buzzer_on_ms(12u);
            }
            if (setup_active && (up_pressed || down_pressed)) {
                setup_timeout_at = now + SETUP_TIMEOUT_MS;
                if (setup_editing) {
                    setup_adjust_value(&setup, setup_page, up_pressed ? 1 : -1, measurement.mode,
                                       setup_edit_subfield);
                    last_display_mode = 0xFFu;
                    buzzer_on_ms(12u);
                }
            }
            if (down_pressed) {
                down_press_started = now;
                down_long_handled = false;
            }
            if (!setup_active && down_held && !down_long_handled &&
                (uint32_t)(now - down_press_started) >= PEAK_BT_LONG_PRESS_MS) {
                bool bt_enabled = !uart_update_bt_stream_enabled();
                uart_update_set_bt_stream_enabled(bt_enabled);
                if (bt_enabled) {
                    bluetooth_init_uart(clock_apb1_hz(), 19200u, true);
                    bluetooth_meter_task_set_active(true, now);
                } else {
                    bluetooth_init_uart(clock_apb1_hz(), 115200u, false);
                    bluetooth_meter_task_set_active(false, now);
                }
                status_task_show_text(bt_enabled ? "btOn " : "btOFF", now, 1200u);
                bt_has_recent_rx = false;
                down_long_handled = true;
                last_display_mode = 0xFFu;
                buzzer_on_ms(30u);
            }
            if (!setup_active && down_released && !down_long_handled) {
                if (measurement.mode == DMM_MODE_V_AC || measurement.mode == DMM_MODE_MV_AC) {
                    peak_active = !peak_active;
                    peak_has_value = have_value && !display_overrange;
                    peak_overrange = display_overrange;
                    peak_value = peak_has_value ? display_value : 0;
                    if (peak_active) {
                        rel_active = false;
                        rel_capture_pending = false;
                        hold_active = false;
                        minmax_state = 0;
                    }
                    status_task_show_text(peak_active ? "PEAK " : "PEOFF", now, 900u);
                    last_display_mode = 0xFFu;
                    buzzer_on_ms(12u);
                } else {
                    buzzer_on_ms(80u);
                }
            }
            if (mode_button_down && mode_pressed && !mode_long_handled &&
                (uint32_t)(now - mode_press_started) >= MODE_LONG_PRESS_MS) {
                bool persisted = backlight_toggle_persist();
                status_task_show_text(backlight_enabled() ? "bLOn " : "bLOFF", now,
                                      persisted ? 900u : 1600u);
                mode_long_handled = true;
                last_display_mode = 0xFFu;
                buzzer_on_ms(persisted ? 20u : 80u);
            }
            if (mode_button_down && !mode_pressed) {
                if (!mode_long_handled &&
                    (uint32_t)(now - mode_press_started) >= MODE_SHORT_PRESS_MIN_MS) {
                    uint8_t next_mode = dmm_p10_cycle_mode(measurement.mode);
                    (void)p10_stock_update_jack_variant(&next_mode, jack_state1_active);
                    if (next_mode != measurement.mode) {
                        measurement.mode = next_mode;
                        pending_mode_rotary = active_measure_rotary;
                        pending_mode_value = measurement.mode;
                        pending_mode_dirty = true;
                        P10_SET_DEFAULT_RANGE_FOR_MODE();
                        P10_INVALIDATE_MEASUREMENT_STATE();
                        P10_CLEAR_INTERACTION_STATE();
                        lead_warning_active =
                            p10_stock_lead_warning(measurement.mode, jack_debounced_state);
                        last_display_mode = 0xFFu;
                    }
                    buzzer_on_ms(20u);
                }
                mode_button_down = false;
            }
            if (minmax_pressed) {
                minmax_press_started = now;
                minmax_long_handled = false;
            }
            if (minmax_held && !minmax_long_handled &&
                (uint32_t)(now - minmax_press_started) >= MINMAX_LONG_PRESS_MS) {
                if (minmax_state != 0u) {
                    minmax_state = 0;
                    dmm_p10_set_autorange(minmax_restore_autorange);
                    last_display_mode = 0xFFu;
                    buzzer_on_ms(40u);
                }
                minmax_long_handled = true;
            }
            if (mem_pressed) {
                mem_press_started = now;
                mem_long_handled = false;
            }
            if (mem_held && !mem_long_handled &&
                (uint32_t)(now - mem_press_started) >= MINMAX_LONG_PRESS_MS) {
                if (mem_logging_task_active()) {
                    mem_logging_task_stop(now);
                    dmm_p10_set_autorange(mem_logging_restore_autorange);
                    status_task_show_text("LOGOF", now, 1200u);
                    buzzer_on_ms(30u);
                } else {
                    char mem_logging_name83[11];
                    sd_fat_result_t sd_rc = battery_low ?
                        SD_FAT_ERR_CARD :
                        log_select_stock_name83(&setup, mem_logging_name83);
                    if (sd_rc == SD_FAT_OK) {
                        sd_rc = sd_fat_append_named_83_with_header(mem_logging_name83,
                                                                   k_log_csv_header, "");
                    }
                    if (sd_rc == SD_FAT_OK) {
                        mem_logging_task_start(mem_logging_name83, now);
                        mem_logging_restore_autorange = dmm_p10_autorange_enabled();
                        minmax_state = 0;
                        hold_active = false;
                        peak_active = false;
                        peak_has_value = false;
                        dmm_p10_set_autorange(false);
                    } else {
                        mem_logging_task_stop(now);
                    }
                    status_task_show_text(mem_logging_task_active() ? "LOGOn" :
                                          (battery_low ? "LoBAt" : "SdErr"), now, 1800u);
                    buzzer_on_ms(mem_logging_task_active() ? 30u : 80u);
                }
                last_display_mode = 0xFFu;
                mem_long_handled = true;
            }
            if (minmax_pressed) {
                if (setup_active ||
                    !p10_minmax_allowed(measurement.mode, rel_active, hold_active,
                                        dmm_p10_voltage_alt_active())) {
                    buzzer_on_ms(80u);
                } else if (minmax_state == 0u) {
                    minmax_state = 1u;
                    minmax_restore_autorange = dmm_p10_autorange_enabled();
                    dmm_p10_set_autorange(false);
                    minmax_max = INT32_MIN;
                    minmax_min = INT32_MAX;
                    minmax_avg = 0;
                    minmax_avg_remainder = 0;
                    minmax_count = 0;
                    minmax_max_overrange = false;
                    minmax_min_overrange = false;
                    hold_active = false;
                    rel_active = false;
                    rel_capture_pending = false;
                    apo_task_reset(true, now);
                    last_display_mode = 0xFFu;
                    buzzer_on_ms(20u);
                } else {
                    ++minmax_state;
                    if (minmax_state >= 5u) {
                        minmax_state = 1u;
                    }
                    last_display_mode = 0xFFu;
                    buzzer_on_ms(12u);
                }
            }
            if (range_pressed) {
                range_press_started = now;
                range_long_handled = false;
            }
            if (range_held && !range_long_handled &&
                (uint32_t)(now - range_press_started) >= RANGE_LONG_PRESS_MS) {
                if (minmax_state == 0u && measurement.mode != DMM_MODE_P10_DIODE &&
                    measurement.mode != DMM_MODE_P10_CONT && measurement.mode != DMM_MODE_DUTY &&
                    measurement.mode != DMM_MODE_LOZ && measurement.mode != DMM_MODE_TEMP) {
                    dmm_p10_set_autorange(true);
                    measurement_controller_set_range(&measurement, 0u, true);
                    measurement_controller_invalidate_range_configuration(&measurement);
                    if (!p10_preserve_display_on_range_change(measurement.mode)) {
                        have_value = false;
                        display_overrange = false;
                    }
                    hold_active = false;
                    rel_active = false;
                    rel_capture_pending = false;
                    last_display_mode = 0xFFu;
                    buzzer_on_ms(40u);
                } else {
                    buzzer_on_ms(80u);
                }
                range_long_handled = true;
            }
            if (range_pressed) {
                if (minmax_state != 0u) {
                    buzzer_on_ms(80u);
                } else if (measurement.mode == DMM_MODE_P10_DIODE) {
                    dmm_p10_toggle_diode_option();
                    (void)setup_write_u8_if_changed(CAL_OFFSET_DIODE_VOLTAGE,
                                                     dmm_p10_diode_option_15() ? 1u : 0u);
                    diode_voltage_label_visible = true;
                    have_value = false;
                    display_overrange = false;
                    have_bargraph = false;
                    last_display_mode = 0xFFu;
                    hold_active = false;
                    rel_active = false;
                    rel_capture_pending = false;
                } else if (p10_manual_range_allowed(measurement.mode)) {
                    if (dmm_p10_autorange_enabled()) {
                        dmm_p10_set_autorange(false);
                    } else {
                        measurement_controller_set_range(
                            &measurement,
                            dmm_p10_next_range(measurement.mode, measurement.range), true);
                    }
                    measurement_controller_invalidate_range_configuration(&measurement);
                    if (!p10_preserve_display_on_range_change(measurement.mode)) {
                        have_value = false;
                        display_overrange = false;
                    }
                    rel_active = false;
                    rel_capture_pending = false;
                    hold_active = false;
                }
                if (minmax_state == 0u) {
                    buzzer_on_ms(12u);
                }
            }
            if (!setup_active && up_pressed) {
                if (rel_active) {
                    rel_active = false;
                    rel_capture_pending = false;
                    hold_active = false;
                    dmm_p10_set_autorange(rel_restore_autorange);
                    last_display_mode = 0xFFu;
                    buzzer_on_ms(12u);
                } else if (minmax_state != 0u || hold_active || lead_warning_active) {
                    buzzer_on_ms(80u);
                } else {
                    if (dmm_p10_cal_reject_unstable_alt_secondary_band()) {
                        buzzer_on_ms(80u);
                    } else {
                        rel_restore_autorange = dmm_p10_autorange_enabled();
                        dmm_p10_set_autorange(false);
                        rel_capture_pending = true;
                        rel_active = true;
                        last_display_mode = 0xFFu;
                        buzzer_on_ms(12u);
                    }
                }
            }
            if (!setup_active && hold_pressed) {
                if (hold_active) {
                    hold_active = false;
                    if (!rel_active) {
                        dmm_p10_set_autorange(rel_restore_autorange);
                    }
                    last_display_mode = 0xFFu;
                    buzzer_on_ms(12u);
                } else if (rel_active || minmax_state != 0u) {
                    buzzer_on_ms(80u);
                } else {
                    hold_value = display_value;
                    hold_has_value = have_value;
                    hold_overrange = display_overrange;
                    hold_low_zone_seen = true;
                    hold_candidate_active = false;
                    hold_candidate_value = display_value;
                    rel_restore_autorange = dmm_p10_autorange_enabled();
                    dmm_p10_set_autorange(false);
                    hold_active = true;
                    rel_capture_pending = false;
                    last_display_mode = 0xFFu;
                    buzzer_on_ms(12u);
                }
            }
        }

        uint8_t adc_channel;
        uint16_t adc_raw;
        if (aux_adc_task_take_sample(&adc_channel, &adc_raw)) {
            if (adc_channel == 5u) {
                battery_ok = battery_sample_raw_stock(adc_raw, &battery_decivolts, &battery_low);
                if (battery_ok) {
                    battery_percent = (uint8_t)(battery_decivolts > 99u ? 99u : battery_decivolts);
                    if (battery_low) {
                        if (!battery_low_message_seen && !setup_active) {
                            status_task_show_text("LoBAt", now, 1600u);
                            last_display_mode = 0xFFu;
                        }
                        battery_low_message_seen = true;
                    } else {
                        battery_low_message_seen = false;
                    }
                    if (setup_active && setup_page == SETUP_PAGE_BATTERY &&
                        battery_percent != last_setup_battery_percent) {
                        last_setup_battery_percent = battery_percent;
                        secondary_display_dirty = true;
                    }
                }
            } else if (adc_channel == 4u) {
                int16_t sampled_temperature_centideg;
                if (temperature_update_from_raw_centideg(adc_raw,
                                                         &sampled_temperature_centideg)) {
                    bool display_temp_fahrenheit = setup.temp_fahrenheit;
                    if (uart_update_temperature_unit_override_active()) {
                        display_temp_fahrenheit = uart_update_temperature_unit_override_fahrenheit();
                    }
                    int16_t sampled_secondary_temperature =
                        stock_aux_temperature_display_tenths_from_centideg(
                            sampled_temperature_centideg, display_temp_fahrenheit);
                    temperature_centideg_c = sampled_temperature_centideg;
                    temperature_tenths_c =
                        stock_aux_temperature_display_tenths_from_centideg(
                            temperature_centideg_c, false);
                    dmm_p10_set_cold_junction_temperature(
                        (int16_t)(temperature_centideg_c / 10));
                    if (!setup_active && !status_task_active() && !bt_has_recent_rx &&
                        !mem_logging_task_active() &&
                        sampled_secondary_temperature != last_secondary_temperature_display) {
                        last_secondary_temperature_display = sampled_secondary_temperature;
                        secondary_display_dirty = true;
                    }
                }
            }
        }

        if (uart_update_temperature_override_active()) {
            temperature_centideg_c =
                (int16_t)(uart_update_temperature_override_tenths_c() * 10);
            temperature_tenths_c =
                stock_aux_temperature_display_tenths_from_centideg(temperature_centideg_c,
                                                                   false);
            dmm_p10_set_cold_junction_temperature((int16_t)(temperature_centideg_c / 10));
            bool display_temp_fahrenheit = setup.temp_fahrenheit;
            if (uart_update_temperature_unit_override_active()) {
                display_temp_fahrenheit = uart_update_temperature_unit_override_fahrenheit();
            }
            int16_t override_secondary_temperature =
                stock_aux_temperature_display_tenths_from_centideg(temperature_centideg_c,
                                                                   display_temp_fahrenheit);
            if (override_secondary_temperature != last_secondary_temperature_display) {
                last_secondary_temperature_display = override_secondary_temperature;
                secondary_display_dirty = true;
            }
        }

        if (uart_update_battery_override_active()) {
            battery_ok = true;
            battery_percent = uart_update_battery_override_percent();
            battery_decivolts = battery_percent;
            battery_low = uart_update_battery_override_low();
            if (!battery_low) {
                battery_low_message_seen = false;
            }
            if (setup_active && setup_page == SETUP_PAGE_BATTERY &&
                battery_percent != last_setup_battery_percent) {
                last_setup_battery_percent = battery_percent;
                secondary_display_dirty = true;
            }
        }

        if (measurement_controller_needs_configuration(&measurement)) {
            bool same_mode_range_reconfigure =
                measurement_controller_is_same_mode_reconfigure(&measurement);
            bool preserve_reconfigure_display =
                same_mode_range_reconfigure && p10_preserve_display_on_range_change(measurement.mode);
            g_app_diag_p10_mode = measurement.mode;
            g_app_diag_p10_range = measurement.range;
            p10_measure_task_set_active(false);
            p10_measure_task_flush();
            dmm_p10_enter_mode(measurement.mode, measurement.range);
            measurement_controller_mark_configured(&measurement);
            range_display_settling = preserve_reconfigure_display;
            last_display_mode = 0xFFu;
            last_display_range = 0xFFu;
            measurement_display_dirty = true;
            secondary_display_dirty = true;
            bargraph_display_dirty = true;
            bargraph_primary_numeric_visible = false;
            dual_secondary_task_set_active(mode_has_dual_current_voltage_secondary(measurement.mode), now);
            setup_page_visible = false;
            setup_active = false;
            setup_editing = false;
            setup_edit_subfield = 0u;
            p10_entry_display_state_t entry_state =
                p10_make_entry_display_state(measurement.mode, now);
            have_value = entry_state.have_value;
            display_value = entry_state.display_value;
            display_overrange = entry_state.display_overrange;
            have_bargraph = entry_state.have_bargraph;
            bargraph_overrange = entry_state.bargraph_overrange;
            bargraph_value = entry_state.bargraph_value;
            ohms_initial_open_until = entry_state.ohms_initial_open_until;
            ohms_range_blank_until = entry_state.ohms_range_blank_until;
            ohms_waiting_for_range_value = entry_state.ohms_waiting_for_range_value;
            if (!preserve_reconfigure_display) {
                bool early_temp_fahrenheit = setup.temp_fahrenheit;
                if (uart_update_temperature_unit_override_active()) {
                    early_temp_fahrenheit = uart_update_temperature_unit_override_fahrenheit();
                }
                int16_t early_secondary_temperature =
                    stock_aux_temperature_display_tenths_from_centideg(temperature_centideg_c,
                                                                       early_temp_fahrenheit);

                lcd_clear_shadow();
                if (measurement.mode == DMM_MODE_P10_OHMS) {
                    lcd_write_upper_temperature_tenths(early_secondary_temperature,
                                                       early_temp_fahrenheit);
                    lcd_write_lower_ohms_open(dmm_p10_decimal_position(measurement.mode, measurement.range));
                    lcd_set_bargraph_limited(0, false, measurement.mode, measurement.range, 25u);
                    lcd_set_auto_annunciator(dmm_p10_autorange_enabled());
                    lcd_set_apo_annunciator(setup.apo_enabled);
                } else if (mode_has_dual_current_voltage_secondary(measurement.mode)) {
                    (void)render_dual_current_voltage_secondary(measurement.mode,
                                                                dual_secondary_task_bank1());
                    lcd_clear_main_numeric_marks();
                    lcd_write_5chars(false, "-----");
                } else {
                    lcd_write_upper_temperature_tenths(early_secondary_temperature,
                                                       early_temp_fahrenheit);
                    if (p10_mode_seeds_zero_on_entry(measurement.mode)) {
                        lcd_write_lower_numeric(0, dmm_p10_decimal_position(measurement.mode, measurement.range));
                        bargraph_primary_numeric_visible = true;
                    } else {
                        lcd_write_5chars(false, "     ");
                    }
                }
                if (p10_mode_has_live_bargraph(measurement.mode)) {
                    lcd_set_bargraph_limited(0, false, measurement.mode, measurement.range,
                                             p10_bargraph_max_for_mode(measurement.mode));
                } else {
                    lcd_clear_bargraph();
                }
                p10_lcd_status_t early_status = {
                    .lcd_mode = p10_lcd_mode(measurement.mode),
                    .lcd_range = measurement.range,
                    .measurement_mode = measurement.mode,
                    .minmax_state = minmax_state,
                    .temperature_fahrenheit = early_temp_fahrenheit,
                    .autorange_enabled = dmm_p10_autorange_enabled(),
                    .rel_active = rel_active,
                    .hold_active = hold_active,
                    .peak_active = peak_active,
                    .apo_enabled = setup.apo_enabled,
                    .battery_low = battery_low,
                    .threshold_enabled = false,
                    .relative_delta = rel_active && !mem_logging_task_active(),
                    .mem_annunciator = mem_logging_task_active() || mem_logging_task_icon_gate(),
                    .bt_annunciator = uart_update_bt_stream_allowed(),
                };
                p10_render_common_lcd_status(&early_status);
                lcd_commit();
                bargraph_display_dirty = false;
                display_task_schedule_bargraph_after(now, P10_BARGRAPH_UPDATE_MS);
                display_task_schedule_after(now,
                                            measurement.mode == DMM_MODE_P10_OHMS ? 0u : P10_LCD_UPDATE_MS);
            }
        }

        p10_measure_task_set_active(true);
        dmm_reading_t reading;
        if (p10_measure_task_take_reading(&reading) && reading.has_sample) {
            measurement_boundary_seen = true;
            bool hold_initial_ohms_open = false;
            if (measurement.mode == DMM_MODE_P10_OHMS && measurement.range == dmm_p10_default_range(DMM_MODE_P10_OHMS) &&
                (int32_t)(now - ohms_initial_open_until) < 0 && !reading.overrange) {
                hold_initial_ohms_open = true;
            }

            if (hold_initial_ohms_open) {
                display_overrange = true;
                have_value = false;
                bargraph_value = 50000;
                bargraph_overrange = true;
                have_bargraph = true;
                bargraph_display_dirty = true;
                measurement_display_dirty = true;
            } else {
                if (p10_mode_has_live_bargraph(measurement.mode) && reading.display_ready) {
                    int32_t next_bargraph_value = reading.display_value;
                    bool next_bargraph_overrange = reading.overrange;
                    if (!have_bargraph ||
                        bargraph_value != next_bargraph_value ||
                        bargraph_overrange != next_bargraph_overrange) {
                        bargraph_display_dirty = true;
                    }
                    bargraph_value = next_bargraph_value;
                    bargraph_overrange = next_bargraph_overrange;
                    have_bargraph = true;
                }
                if (reading.range_changed) {
                    bool ohms_range_change = measurement.mode == DMM_MODE_P10_OHMS;
                    bool deferred_range_reconfigure = dmm_p10_range_reconfigure_pending();
                    bool preserve_range_display = p10_preserve_display_on_range_change(measurement.mode);
                    /*
                     * In the ohms range-change path reading.overrange carries the
                     * search direction. Upshift means the current range overflowed,
                     * so OFL should appear immediately; downshift blanks while the
                     * controller searches for the lower range.
                     */
                    bool keep_open_display = ohms_range_change && reading.overrange;
                    bool cap_timeout_range_change = measurement.mode == DMM_MODE_P10_CAP &&
                                                    !reading.display_ready && have_value;
                    measurement_controller_accept_range_change(
                        &measurement, reading.range, deferred_range_reconfigure,
                        !cap_timeout_range_change);
                    last_display_mode = 0xFFu;
                    last_display_range = 0xFFu;
                    measurement_display_dirty = true;
                    bargraph_display_dirty = true;
                    if (!preserve_range_display && !cap_timeout_range_change) {
                        have_value = false;
                        display_overrange = keep_open_display;
                    }
                    if (ohms_range_change) {
                        ohms_range_blank_until = now + OHMS_RANGE_BLANK_MS;
                    }
                    ohms_waiting_for_range_value = ohms_range_change && !keep_open_display;
                    if (measurement.mode == DMM_MODE_P10_CAP && reading.display_ready) {
                        measurement_controller_set_display_range(&measurement, reading.range);
                        display_value = reading.display_value;
                        display_overrange = reading.overrange;
                        have_value = true;
                        range_display_settling = false;
                        measurement_display_dirty = true;
                        ohms_waiting_for_range_value = false;
                        ohms_range_blank_until = 0;
                    }
                    if (keep_open_display) {
                        bargraph_value = 50000;
                        bargraph_overrange = true;
                        have_bargraph = true;
                        bargraph_display_dirty = true;
                    }
                    rel_active = false;
                    rel_capture_pending = false;
                    hold_active = false;
                    minmax_state = 0;
                    ohms_initial_open_until = 0;
                } else if (measurement.mode == DMM_MODE_P10_OHMS && !reading.display_ready) {
                    if (ohms_waiting_for_range_value) {
                        have_value = false;
                        display_overrange = reading.overrange;
                        bargraph_value = reading.overrange ? 50000 : reading.instant_value;
                        bargraph_overrange = reading.overrange;
                        have_bargraph = true;
                        bargraph_display_dirty = true;
                        if (reading.overrange) {
                            ohms_waiting_for_range_value = false;
                        }
                        measurement_display_dirty = true;
                    } else if (!have_value) {
                        have_value = false;
                        display_overrange = true;
                        bargraph_value = 50000;
                        bargraph_overrange = true;
                        have_bargraph = true;
                        bargraph_display_dirty = true;
                        measurement_display_dirty = true;
                    }
                } else {
                    display_overrange = reading.overrange;
                }
                if (reading.display_ready &&
                    !(measurement.mode == DMM_MODE_P10_OHMS &&
                      (int32_t)(now - ohms_range_blank_until) < 0)) {
                    bool display_state_changed =
                        !have_value ||
                        display_value != reading.display_value ||
                        display_overrange != reading.overrange ||
                        measurement.display_range != reading.range;
                    measurement_controller_accept_display_sample(&measurement, reading.range);
                    display_value = reading.display_value;
                    display_overrange = reading.overrange;
                    range_display_settling = false;
                    if (display_state_changed) {
                        measurement_display_dirty = true;
                    }
                    ohms_waiting_for_range_value = false;
                    ohms_range_blank_until = 0;
                    if (rel_active && rel_capture_pending && !reading.overrange &&
                        !dmm_p10_cal_reject_unstable_alt_secondary_band()) {
                        rel_baseline = reading.display_value;
                        rel_capture_pending = false;
                    }
                    if (minmax_state != 0u) {
                        if (reading.overrange) {
                            if (reading.display_value < 0) {
                                minmax_min = -100000;
                                minmax_min_overrange = true;
                            } else {
                                minmax_max = 100000;
                                minmax_max_overrange = true;
                            }
                        } else {
                            int32_t sample = reading.display_value;
                            if (sample > minmax_max) {
                                minmax_max = sample;
                                minmax_max_overrange = false;
                            }
                            if (sample < minmax_min) {
                                minmax_min = sample;
                                minmax_min_overrange = false;
                            }
                            if (minmax_count < 1000000u) {
                                ++minmax_count;
                            }
                            int32_t delta = sample - minmax_avg;
                            minmax_avg_remainder += delta;
                            minmax_avg += minmax_avg_remainder / (int32_t)minmax_count;
                            minmax_avg_remainder %= (int32_t)minmax_count;
                        }
                    }
                    if (peak_active) {
                        if (reading.overrange) {
                            peak_overrange = true;
                        } else {
                            int32_t sample = reading.display_value;
                            int32_t sample_abs = sample < 0 ? -sample : sample;
                            int32_t peak_abs = peak_value < 0 ? -peak_value : peak_value;
                            if (!peak_has_value || sample_abs > peak_abs) {
                                peak_value = sample;
                                peak_overrange = false;
                                peak_has_value = true;
                            }
                        }
                    }
                    if (hold_active && !rel_active && hold_has_value) {
                        if (p10_hold_stable_sample(measurement.mode, reading.display_value,
                                                   reading.overrange,
                                                   hold_overrange, hold_value,
                                                   &hold_low_zone_seen,
                                                   &hold_candidate_active,
                                                   &hold_candidate_value)) {
                            hold_value = reading.display_value;
                            hold_overrange = reading.overrange;
                            hold_has_value = true;
                            last_display_mode = 0xFFu;
                            buzzer_on_ms(50u);
                        }
                    }
                    have_value = true;
                }
            }
        }

        if (mem_logging_task_active() && battery_low) {
            mem_logging_task_stop(now);
            dmm_p10_set_autorange(mem_logging_restore_autorange);
            status_task_show_text("LoBAt", now, 1800u);
            last_display_mode = 0xFFu;
            buzzer_on_ms(80u);
        }

        if (mem_logging_task_active() && sd_card_task_error_pending()) {
            sd_card_task_clear_error();
            mem_logging_task_stop(now);
            dmm_p10_set_autorange(mem_logging_restore_autorange);
            status_task_show_text("SdErr", now, 1800u);
            last_display_mode = 0xFFu;
            buzzer_on_ms(80u);
        }

        mem_logging_event_t mem_logging_event;
        while (mem_logging_task_take_event(&mem_logging_event)) {
            bool queued =
                log_queue_measurement_csv(mem_logging_task_sample(), mem_log_mode, mem_log_range,
                                          mem_log_have_value, mem_log_overrange, mem_log_value,
                                          temperature_tenths_c, &setup,
                                          mem_logging_task_name83());
            mem_logging_task_finish_sample(queued, setup.logging_interval_s, now);
            if (!queued) {
                dmm_p10_set_autorange(mem_logging_restore_autorange);
                status_task_show_text("SdErr", now, 1800u);
                last_display_mode = 0xFFu;
                buzzer_on_ms(80u);
            }
        }
        if (measurement_boundary_seen) {
            sd_card_task_notify_measurement_boundary();
        }

        display_model_input_t presentation_model = {
            .value = display_value,
            .have_value = have_value,
            .overrange = display_overrange,
            .hold_active = hold_active,
            .hold_value = hold_value,
            .hold_has_value = hold_has_value,
            .hold_overrange = hold_overrange,
            .peak_active = peak_active,
            .peak_value = peak_value,
            .peak_has_value = peak_has_value,
            .peak_overrange = peak_overrange,
            .minmax_state = minmax_state,
            .minmax_max = minmax_max,
            .minmax_min = minmax_min,
            .minmax_avg = minmax_avg,
            .minmax_count = minmax_count,
            .minmax_max_overrange = minmax_max_overrange,
            .minmax_min_overrange = minmax_min_overrange,
            .relative_active = rel_active,
            .relative_capture_pending = rel_capture_pending,
            .relative_baseline = rel_baseline,
        };

        bluetooth_meter_task_set_active(uart_update_bt_stream_allowed(), now);
        if (!bluetooth_tx_busy() && bluetooth_meter_task_take_frame_due()) {
            display_model_input_t frame_model = presentation_model;
            frame_model.overrange |= lead_warning_active;
            display_model_output_t frame_output =
                display_model_select(&frame_model);
            int32_t frame_value = frame_output.value;
            bool frame_overrange = frame_output.overrange;
            uint8_t bt_mode = p10_bluetooth_mode(measurement.mode);
            uint8_t bt_range = measurement.range;
            if (measurement.mode == DMM_MODE_P10_DIODE && dmm_p10_diode_option_15()) {
                bt_range = 1u;
            }
            uint8_t bt_unit_scale = p10_bluetooth_unit_scale(measurement.mode, measurement.range, frame_value);
            uint8_t bt_bargraph = have_bargraph ?
                                   p10_bluetooth_bargraph_count(measurement.mode, bargraph_value,
                                                                bargraph_overrange) : 0u;
            bool bt_auto = dmm_p10_autorange_enabled() &&
                           !rel_active && !hold_active &&
                           !peak_active &&
                           measurement.mode != DMM_MODE_LOZ &&
                           measurement.mode != DMM_MODE_TEMP &&
                           measurement.mode != DMM_MODE_P10_CONT &&
                           measurement.mode != DMM_MODE_P10_DIODE &&
                           measurement.mode != DMM_MODE_DUTY;
            uint8_t bt_icon0 = p10_bluetooth_icon0(measurement.mode, bt_auto, setup.apo_enabled);
            uint8_t bt_icon1 = 0x40u;
            uint8_t bt_icon2 = (uint8_t)(rel_active ? 4u : (hold_active ? 8u : 0u));
            if (setup_active) {
                bt_icon2 = (uint8_t)(bt_icon2 + 0x10u);
            }
            bool bt_temp_fahrenheit = setup.temp_fahrenheit;
            if (uart_update_temperature_unit_override_active()) {
                bt_temp_fahrenheit = uart_update_temperature_unit_override_fahrenheit();
            }
            uint8_t frame[19];
            bluetooth_make_meter_frame(frame, setup.meter_id, bt_mode, bt_range,
                                       frame_overrange, frame_value, temperature_tenths_c,
                                       bt_temp_fahrenheit,
                                       bt_unit_scale, bt_bargraph,
                                       bt_icon0, bt_icon1, bt_icon2);
            (void)bluetooth_send_bytes_irq(frame, sizeof(frame));
            bluetooth_meter_task_frame_sent(now);
        }

        uint8_t lcd_mode = p10_lcd_mode(measurement.mode);
        uint8_t lcd_range = measurement.mode == DMM_MODE_P10_CAP ? measurement.display_range : measurement.range;
        bool display_update_due = display_task_update_due(now);
        bool periodic_display_service =
            setup_active || setup_page_visible || status_task_active() || bt_has_recent_rx ||
            mem_logging_task_active() || mem_logging_task_icon_gate() || lead_warning_active ||
            mode_has_dual_current_voltage_secondary(measurement.mode);
        bool measurement_lcd_due = measurement_display_dirty && display_update_due;
        bool secondary_lcd_due = secondary_display_dirty && display_update_due;
        bool display_identity_dirty =
            last_display_mode != lcd_mode || last_display_range != lcd_range;
        bool lcd_dirty =
            measurement_lcd_due || secondary_lcd_due || display_identity_dirty;
        bool full_lcd_rendered = false;
        if (lcd_dirty || (periodic_display_service && display_update_due)) {
            char upper[5];
            char lower[5];
            bool display_temp_fahrenheit = setup.temp_fahrenheit;
            if (uart_update_temperature_unit_override_active()) {
                display_temp_fahrenheit = uart_update_temperature_unit_override_fahrenheit();
            }
            bool setup_secondary_active = false;
            if (setup_active || setup_page_visible) {
                setup_state_t display_setup = setup;
                display_setup.temp_fahrenheit = display_temp_fahrenheit;
                format_setup_page(setup_page, &display_setup, measurement.mode, setup_editing,
                                  temperature_tenths_c, battery_percent, battery_ok,
                                  setup_edit_subfield,
                                  ((now / SETUP_EDIT_BLINK_MS) & 1u) != 0u,
                                  upper, lower);
                setup_secondary_active = true;
            }
            display_model_input_t lcd_model = presentation_model;
            if (measurement.mode == DMM_MODE_P10_OHMS &&
                (int32_t)(now - ohms_range_blank_until) < 0) {
                lcd_model.have_value = false;
            }
            display_model_output_t rendered = display_model_select(&lcd_model);
            int32_t rendered_value = rendered.value;
            bool rendered_have_value = rendered.have_value;
            bool rendered_overrange = rendered.overrange;
            mem_log_mode = measurement.mode;
            mem_log_range = measurement.range;
            mem_log_have_value = rendered_have_value;
            mem_log_overrange = rendered_overrange;
            mem_log_value = rendered_value;
            bool show_text = !lead_warning_active &&
                             format_p10_status_text(measurement.mode, rendered_have_value,
                                                    rendered_overrange, rendered_value, lower);
            if (!status_task_active() && !mem_logging_task_active()) {
                mem_logging_task_clear_icon_gate();
            }
            lcd_clear_shadow();
            int32_t dbm_value = 0;
            bool dbm_secondary_active = !setup_secondary_active &&
                                        !status_task_active() &&
                                        !bt_has_recent_rx &&
                                        measurement.mode == DMM_MODE_V_AC &&
                                        setup.dbm_enabled &&
                                        !dmm_p10_voltage_alt_active();
            bool dbm_valid = dbm_secondary_active && rendered_have_value &&
                             !rendered_overrange &&
                             voltage_dbm_milli(rendered_value,
                                               dmm_p10_decimal_position(measurement.mode, lcd_range),
                                               &dbm_value);
            int32_t rendered_temperature_value =
                temperature_display_tenths(rendered_value, display_temp_fahrenheit);
            int16_t secondary_temperature_value =
                stock_aux_temperature_display_tenths_from_centideg(temperature_centideg_c,
                                                                   display_temp_fahrenheit);
            if (setup_secondary_active) {
                if (render_stock_setup_secondary_page(setup_page, &setup, measurement.mode, measurement.range,
                                                      rendered_value, rendered_overrange)) {
                } else if (setup_page == SETUP_PAGE_TEMP) {
                    lcd_write_upper_temperature_tenths(secondary_temperature_value,
                                                       display_temp_fahrenheit);
                } else if (setup_page == SETUP_PAGE_BATTERY && battery_ok) {
                    lcd_write_upper_battery_percent(battery_percent);
                    lcd_set_secondary_voltage_annunciator(true);
                } else {
                    lcd_set_secondary_decimal(setup_secondary_decimal_position(setup_page, measurement.mode));
                    lcd_write_5chars(true, upper);
                }
                if (measurement.mode == DMM_MODE_P10_DIODE && setup_page == SETUP_PAGE_FUNCTION) {
                    lcd_set_secondary_voltage_annunciator(true);
                }
                lcd_set_dbm_annunciator(setup_page == SETUP_PAGE_DBM &&
                                        mode_has_dbm_setup(measurement.mode));
            } else if (status_task_active()) {
                lcd_write_5chars(true, status_task_text());
            } else if (bt_has_recent_rx) {
                format_hex_line('b', bt_last_rx, upper);
                lcd_write_5chars(true, upper);
            } else if (mem_logging_task_active()) {
                lcd_write_mem_logging_count(mem_logging_task_sample());
            } else if (measurement.mode == DMM_MODE_P10_DIODE && diode_voltage_label_visible) {
                lcd_write_5chars(true, dmm_p10_diode_option_15() ? "   15" : "    3");
                lcd_set_secondary_voltage_annunciator(true);
            } else if (dbm_secondary_active) {
                if (dbm_valid) {
                    lcd_write_upper_signed_numeric(dbm_value, 3u);
                } else {
                    lcd_set_secondary_decimal(0u);
                    lcd_write_5chars(true, " OFL ");
                }
                lcd_set_dbm_annunciator(true);
            } else if (render_dual_current_voltage_secondary(measurement.mode,
                                                             dual_secondary_task_bank1())) {
            } else if (render_voltage_frequency_secondary(measurement.mode)) {
            } else if (render_current_timing_default_secondary(measurement.mode, &setup)) {
            } else {
                lcd_write_upper_temperature_tenths(secondary_temperature_value,
                                                   display_temp_fahrenheit);
            }
            if (lead_warning_active) {
                lcd_clear_main_numeric_marks();
                lcd_write_5chars(false, " LEAD");
            } else if (mode_has_dual_current_voltage_secondary(measurement.mode) &&
                       !dmm_p10_current_dual_display_ready()) {
                lcd_clear_main_numeric_marks();
                lcd_write_5chars(false, "-----");
            } else if (show_text) {
                if (measurement.mode == DMM_MODE_P10_DIODE && rendered_overrange) {
                    lcd_write_lower_diode_open(dmm_p10_decimal_position(measurement.mode, measurement.range));
                } else if (measurement.mode == DMM_MODE_P10_OHMS && rendered_overrange) {
                    lcd_write_lower_ohms_open(dmm_p10_decimal_position(measurement.mode, measurement.range));
                } else if (measurement.mode == DMM_MODE_P10_CONT && rendered_overrange) {
                    lcd_write_lower_continuity_open(dmm_p10_decimal_position(measurement.mode, measurement.range));
                } else if (rendered_overrange) {
                    lcd_write_lower_overflow(dmm_p10_decimal_position(measurement.mode, lcd_range));
                } else if (measurement.mode == DMM_MODE_P10_OHMS && !rendered_have_value) {
                    lcd_write_lower_blank_with_decimal(dmm_p10_decimal_position(measurement.mode, measurement.range));
                } else if (measurement.mode == DMM_MODE_TEMP) {
                    lcd_write_lower_temperature_c_tenths(rendered_temperature_value,
                                                         dmm_p10_decimal_position(measurement.mode, measurement.range));
                } else {
                    lcd_clear_main_numeric_marks();
                    lcd_write_5chars(false, lower);
                }
            } else {
                if (measurement.mode == DMM_MODE_TEMP) {
                    lcd_write_lower_temperature_c_tenths(rendered_temperature_value,
                                                         dmm_p10_decimal_position(measurement.mode, measurement.range));
                } else {
                    lcd_write_lower_numeric(rendered_value, dmm_p10_decimal_position(measurement.mode, lcd_range));
                }
            }
            p10_lcd_status_t status = {
                .lcd_mode = lcd_mode,
                .lcd_range = lcd_range,
                .measurement_mode = measurement.mode,
                .minmax_state = minmax_state,
                .temperature_fahrenheit = display_temp_fahrenheit,
                .autorange_enabled = dmm_p10_autorange_enabled(),
                .rel_active = rel_active,
                .hold_active = hold_active,
                .peak_active = peak_active,
                .apo_enabled = setup.apo_enabled,
                .battery_low = battery_low,
                .threshold_enabled = !range_display_settling &&
                                     p10_stock_threshold_annunciator(measurement.mode, lcd_range,
                                                                    rendered_value,
                                                                    rendered_overrange),
                .relative_delta = rel_active && !mem_logging_task_active(),
                .mem_annunciator = mem_logging_task_active() || mem_logging_task_icon_gate(),
                .bt_annunciator = uart_update_bt_stream_allowed(),
            };
            p10_render_common_lcd_status(&status);
            uint8_t bargraph_max = p10_bargraph_max_for_mode(measurement.mode);
            bool primary_numeric_visible =
                !lead_warning_active &&
                !(mode_has_dual_current_voltage_secondary(measurement.mode) &&
                  !dmm_p10_current_dual_display_ready()) &&
                !show_text;
            bargraph_primary_numeric_visible = primary_numeric_visible;
            if (!p10_mode_has_live_bargraph(measurement.mode) || !have_bargraph) {
                lcd_clear_bargraph();
            } else if (!primary_numeric_visible) {
                lcd_set_bargraph_limited(0, false, measurement.mode, measurement.range, bargraph_max);
            } else {
                lcd_set_bargraph_limited(bargraph_value, bargraph_overrange,
                                         measurement.mode, measurement.range, bargraph_max);
            }
            lcd_commit();
            full_lcd_rendered = true;
            measurement_display_dirty = false;
            secondary_display_dirty = false;
            bargraph_display_dirty = false;
            last_display_mode = lcd_mode;
            last_display_range = lcd_range;
            last_rotary = rotary;
            display_task_schedule_bargraph_after(now, P10_BARGRAPH_UPDATE_MS);
            display_task_schedule_after(now, P10_LCD_UPDATE_MS);
        }

        if (!full_lcd_rendered &&
            bargraph_display_dirty &&
            display_task_bargraph_update_due(now)) {
            uint8_t bargraph_max = p10_bargraph_max_for_mode(measurement.mode);
            if (!p10_mode_has_live_bargraph(measurement.mode) || !have_bargraph) {
                lcd_clear_bargraph();
            } else if (!bargraph_primary_numeric_visible) {
                lcd_set_bargraph_limited(0, false, measurement.mode, measurement.range, bargraph_max);
            } else {
                lcd_set_bargraph_limited(bargraph_value, bargraph_overrange,
                                         measurement.mode, measurement.range, bargraph_max);
            }
            lcd_commit();
            bargraph_display_dirty = false;
            display_task_schedule_bargraph_after(now, P10_BARGRAPH_UPDATE_MS);
        }

        __asm__ volatile("wfi");
    }
#undef P10_MARK_LCD_IDENTITY_DIRTY
#undef P10_SET_DEFAULT_RANGE_FOR_MODE
#undef P10_INVALIDATE_MEASUREMENT_STATE
#undef P10_CLEAR_INTERACTION_STATE
#elif EEV_DIODE_FE_TEST
    frontend_init_pins();

    static uint8_t fe_status[FRONTEND_STATUS_SIZE];
    bool diode_configured = false;
    uint32_t next_fe_update = 0;
    uint8_t last_rotary = 0xFFu;
    uint8_t last_status = 0xFFu;

    while (1) {
        buzzer_poll();
        uint32_t now = clock_millis();
        uint8_t rotary = input_rotary_sample_active_low();

        if (rotary != 0x10u) {
            if (rotary != last_rotary) {
                char upper[5];
                format_hex_line('P', rotary, upper);
                lcd_clear_shadow();
                lcd_write_5chars(true, upper);
                lcd_write_5chars(false, "DIOD?");
                lcd_commit();
                last_rotary = rotary;
            }
            diode_configured = false;
            next_fe_update = now + 100u;
            __asm__ volatile("wfi");
            continue;
        }

        if (!diode_configured) {
            frontend_configure_diode(0x8000u);
            diode_configured = true;
            last_status = 0xFFu;
            lcd_clear_shadow();
            lcd_write_5chars(true, "DIOD ");
            lcd_write_5chars(false, "INIT ");
            lcd_commit();
        }

        if ((int32_t)(now - next_fe_update) >= 0) {
            uint8_t status = 0;
            if (frontend_ready()) {
                frontend_read_status(fe_status);
                status = (uint8_t)(fe_status[0x1Eu] & fe_status[0x1Fu]);
                if ((status & 0x04u) != 0u) {
                    int32_t sample = frontend_decode_signed24_sample(fe_status);
                    if (sample < 1000 && sample >= -999) {
                        buzzer_on_ms(8);
                    }
                }
            }

            if (status != last_status || last_rotary != rotary) {
                char lower[5];
                format_hex_line('S', status, lower);
                lcd_clear_shadow();
                lcd_write_5chars(true, "DIOD ");
                lcd_write_5chars(false, lower);
                lcd_commit();
                last_status = status;
                last_rotary = rotary;
            }
            next_fe_update = now + 50u;
        }

        __asm__ volatile("wfi");
    }
#else
    frontend_init_pins();
    bluetooth_init_19200(clock_apb1_hz());

    bool cal_present = calibration_export_area_present();
    uint32_t cal_hash = calibration_checksum_export_area();

    dmm_init();

    uint32_t next_bt = 0;
    uint32_t next_lcd_blank = 0;
    while (1) {
        buzzer_poll();
        bluetooth_poll_rx();
        dmm_poll();

        uint32_t now = clock_millis();
        task_runtime_poll(now);
        if (uart_update_debug_allowed() && (int32_t)(now - next_bt) >= 0) {
            bluetooth_send_heartbeat(now, cal_hash, cal_present);
            next_bt = now + 1000u;
        }
        if ((int32_t)(now - next_lcd_blank) >= 0) {
            lcd_blank();
            next_lcd_blank = now + 250u;
        }

        __asm__ volatile("wfi");
    }
#endif
}
