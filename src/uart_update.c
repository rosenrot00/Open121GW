#include "uart_update.h"

#include "analog.h"
#include "app_diag.h"
#include "battery.h"
#include "bluetooth.h"
#include "calibration.h"
#include "clock.h"
#include "dmm.h"
#include "frontend.h"
#include "gpio.h"
#include "hw.h"
#include "input.h"
#include "lcd.h"
#include "sd_fat.h"
#include "sdio_sd.h"
#include "temperature.h"

#define UPD_LINE_MAX 280u
#define UPD_APP_MAX_SIZE 0x0005A000u
#define UPD_FLASH_PAGE_SIZE 256u
#define UPD_FLASH_HALF_PAGE_SIZE 128u
#define UPD_STREAM_CHUNK_MAX 128u
#define UPD_RAM_MIN 0x20000000u
#define UPD_RAM_MAX 0x20014000u
#define UPD_FLASH_READ_MIN 0x08000000u
#define UPD_FLASH_READ_MAX CAL_FLASH_BASE
#define UPD_READ_MAX 256u
#define UPD_READ_REPLY_MAX 8u
#ifndef EEV_UART_UPDATE_TRACE
#define EEV_UART_UPDATE_TRACE 0
#endif

#define FLASH_PECR_PELOCK BIT(0)
#define FLASH_PECR_PRGLOCK BIT(1)
#define FLASH_PECR_PROG BIT(3)
#define FLASH_PECR_ERASE BIT(9)
#define FLASH_PECR_FPRG BIT(10)
#define FLASH_SR_BSY BIT(0)
#define FLASH_SR_EOP BIT(1)
#define FLASH_SR_WRPERR BIT(8)
#define FLASH_SR_PGAERR BIT(9)
#define FLASH_SR_SIZERR BIT(10)
#define FLASH_SR_OPTVERR BIT(11)
#define FLASH_SR_ERRORS (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_OPTVERR)

#define USART2_BRR_STOCK_19200 0x00DAu
#define USART2_BRR_FLASH_38400 0x006Du
#define USART2_BRR_FLASH_57600 0x0049u
#define USART2_BRR_FLASH_115200 0x0024u
#define USART2_BRR_FLASH_230400 0x0012u
#define USART2_BRR_FLASH_460800 0x0009u
#define USART2_BRR_FLASH_921600 0x0005u
#define CAL_OFFSET_TEMP_UNIT 16u

typedef struct {
    bool receiving;
    bool complete;
    bool overflow;
    uint32_t expected_size;
    uint32_t expected_crc;
    uint32_t received_size;
    uint32_t crc;
    uint8_t first_bytes[16];
    uint8_t first_count;
    char line[UPD_LINE_MAX];
    uint16_t line_len;
} uart_update_state_t;

static uart_update_state_t g_upd;
static volatile uint32_t g_upd_fault_active;
static volatile uint32_t g_upd_fault_resume_pc;
static uint32_t g_upd_quiet_until;
static bool g_upd_debug_enabled = false;
static bool g_upd_bt_stream_enabled = false;
static bool g_upd_temperature_override_enabled = false;
static int16_t g_upd_temperature_override_tenths_c = 250;
static bool g_upd_temperature_unit_override_enabled = false;
static bool g_upd_temperature_unit_override_fahrenheit = false;
static bool g_upd_battery_override_enabled = false;
static uint8_t g_upd_battery_override_percent = 88;
static bool g_upd_battery_override_low = false;

#define UPD_LCDTEST_ENABLED g_upd.first_bytes[8]
#define UPD_LCDTEST_INDEX_LO g_upd.first_bytes[9]
#define UPD_LCDTEST_INDEX_HI g_upd.first_bytes[10]
#define UPD_LCDTEST_PERIOD_LO g_upd.first_bytes[11]
#define UPD_LCDTEST_PERIOD_HI g_upd.first_bytes[12]
#define UPD_P10_OVERRIDE_COOKIE g_upd.first_bytes[13]
#define UPD_P10_OVERRIDE_MODE g_upd.first_bytes[14]
#define UPD_P10_OVERRIDE_RANGE g_upd.first_bytes[15]

static bool upd_temperature_fahrenheit_enabled(void) {
    if (g_upd_temperature_unit_override_enabled) {
        return g_upd_temperature_unit_override_fahrenheit;
    }
    uint8_t saved = calibration_read_u8(CAL_OFFSET_TEMP_UNIT);
    return saved < 2u && saved == 0u;
}

static int32_t upd_temperature_display_tenths(int32_t c_tenths, bool fahrenheit) {
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

bool uart_update_temperature_override_active(void) {
    return g_upd_temperature_override_enabled;
}

int16_t uart_update_temperature_override_tenths_c(void) {
    return g_upd_temperature_override_tenths_c;
}

bool uart_update_temperature_unit_override_active(void) {
    return g_upd_temperature_unit_override_enabled;
}

bool uart_update_temperature_unit_override_fahrenheit(void) {
    return g_upd_temperature_unit_override_fahrenheit;
}

bool uart_update_battery_override_active(void) {
    return g_upd_battery_override_enabled;
}

uint8_t uart_update_battery_override_percent(void) {
    return g_upd_battery_override_percent;
}

bool uart_update_battery_override_low(void) {
    return g_upd_battery_override_low;
}

bool uart_update_lcdtest_active(void) {
    return UPD_LCDTEST_ENABLED != 0u && !g_upd.receiving;
}

uint16_t uart_update_lcdtest_index(void) {
    return (uint16_t)UPD_LCDTEST_INDEX_LO | ((uint16_t)UPD_LCDTEST_INDEX_HI << 8);
}

uint16_t uart_update_lcdtest_period_ms(void) {
    return (uint16_t)UPD_LCDTEST_PERIOD_LO | ((uint16_t)UPD_LCDTEST_PERIOD_HI << 8);
}

static void uart_update_lcdtest_set_index(uint16_t index) {
    UPD_LCDTEST_INDEX_LO = (uint8_t)index;
    UPD_LCDTEST_INDEX_HI = (uint8_t)(index >> 8);
}

static void uart_update_lcdtest_set_period(uint16_t period) {
    UPD_LCDTEST_PERIOD_LO = (uint8_t)period;
    UPD_LCDTEST_PERIOD_HI = (uint8_t)(period >> 8);
}

void uart_update_lcdtest_advance(void) {
    uint16_t index = (uint16_t)(uart_update_lcdtest_index() + 1u);
    if (index >= 512u) {
        index = 0;
    }
    uart_update_lcdtest_set_index(index);
}

bool uart_update_p10_override_active(void) {
    return UPD_P10_OVERRIDE_MODE != 0xFFu;
}

uint8_t uart_update_p10_override_mode(void) {
    return UPD_P10_OVERRIDE_MODE;
}

uint8_t uart_update_p10_override_range(void) {
    return UPD_P10_OVERRIDE_RANGE;
}

uint8_t uart_update_p10_override_cookie(void) {
    return UPD_P10_OVERRIDE_COOKIE;
}

__attribute__((naked, noinline))
static uint32_t upd_try_read8(uint32_t addr, uint8_t *out) {
    __asm__ volatile(
        "ldr r3, =g_upd_fault_resume_pc\n"
        "ldr r2, =1f\n"
        "str r2, [r3]\n"
        "ldr r3, =g_upd_fault_active\n"
        "movs r2, #1\n"
        "str r2, [r3]\n"
        "ldrb r2, [r0]\n"
        "strb r2, [r1]\n"
        "movs r2, #0\n"
        "ldr r3, =g_upd_fault_active\n"
        "str r2, [r3]\n"
        "movs r0, #0\n"
        "bx lr\n"
        "1:\n"
        "movs r2, #0\n"
        "ldr r3, =g_upd_fault_active\n"
        "str r2, [r3]\n"
        "movs r0, #1\n"
        "bx lr\n"
    );
}

bool uart_update_hardfault_recover(uint32_t *stacked_regs) {
    if (g_upd_fault_active == 0u || g_upd_fault_resume_pc == 0u || stacked_regs == 0) {
        return false;
    }
    stacked_regs[6] = g_upd_fault_resume_pc;
    return true;
}

static char upd_hex_digit(uint8_t value) {
    value &= 0x0Fu;
    return (char)(value < 10u ? ('0' + value) : ('A' + value - 10u));
}

static void upd_send_char(char ch) {
    (void)bluetooth_send_byte((uint8_t)ch);
}

static void upd_send_text(const char *text) {
    while (*text != '\0') {
        upd_send_char(*text++);
    }
}

static void upd_send_hex8(uint8_t value) {
    upd_send_char(upd_hex_digit((uint8_t)(value >> 4)));
    upd_send_char(upd_hex_digit(value));
}

static void upd_send_hex16(uint16_t value) {
    upd_send_hex8((uint8_t)(value >> 8));
    upd_send_hex8((uint8_t)value);
}

static void upd_send_hex32(uint32_t value) {
    upd_send_hex8((uint8_t)(value >> 24));
    upd_send_hex8((uint8_t)(value >> 16));
    upd_send_hex8((uint8_t)(value >> 8));
    upd_send_hex8((uint8_t)value);
}

static void upd_send_lcd_shadow_hex(void) {
    upd_send_text(" lcd=");
    for (uint8_t i = 0; i < 16u; ++i) {
        if (i != 0u) {
            upd_send_char(',');
        }
        upd_send_hex32(lcd_debug_shadow_word(i));
    }
}

static void upd_send_gpio_odr_snapshot(void) {
    static const uint32_t ports[] = {
        GPIOA_BASE, GPIOC_BASE, GPIOD_BASE, GPIOE_BASE, GPIOF_BASE, GPIOG_BASE,
    };

    upd_send_text(" mux=");
    for (uint8_t i = 0; i < (uint8_t)(sizeof(ports) / sizeof(ports[0])); ++i) {
        if (i != 0u) {
            upd_send_char(',');
        }
        upd_send_hex16((uint16_t)REG32(GPIO_ODR(ports[i])));
    }
}

static uint8_t upd_hex_value(char ch, bool *ok) {
    if (ch >= '0' && ch <= '9') {
        return (uint8_t)(ch - '0');
    }
    if (ch >= 'A' && ch <= 'F') {
        return (uint8_t)(ch - 'A' + 10);
    }
    if (ch >= 'a' && ch <= 'f') {
        return (uint8_t)(ch - 'a' + 10);
    }
    *ok = false;
    return 0;
}

static bool upd_parse_hex32(const char **cursor, uint32_t *value) {
    bool ok = true;
    uint32_t out = 0;
    uint8_t count = 0;
    const char *p = *cursor;

    while ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')) {
        if (count >= 8u) {
            return false;
        }
        out = (out << 4) | upd_hex_value(*p, &ok);
        ++p;
        ++count;
    }

    if (!ok || count == 0u) {
        return false;
    }
    *cursor = p;
    *value = out;
    return true;
}

static bool upd_expect_space(const char **cursor) {
    if (**cursor != ' ') {
        return false;
    }
    while (**cursor == ' ') {
        ++*cursor;
    }
    return true;
}

static bool upd_baud_supported(uint32_t baud) {
    return baud == 19200u || baud == 38400u || baud == 57600u || baud == 115200u ||
           baud == 230400u || baud == 460800u || baud == 921600u;
}

static bool upd_starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return false;
        }
    }
    return true;
}

static char upd_upper_name_char(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - 'a' + 'A');
    }
    return ch;
}

static bool upd_parse_name83(const char **cursor, char name83[11]) {
    if (!upd_expect_space(cursor)) {
        return false;
    }
    for (uint8_t i = 0u; i < 11u; ++i) {
        name83[i] = ' ';
    }

    const char *p = *cursor;
    uint8_t base = 0u;
    while (*p != '\0' && *p != ' ' && *p != '.' && base < 8u) {
        name83[base++] = upd_upper_name_char(*p++);
    }
    if (*p == '.') {
        ++p;
        uint8_t ext = 0u;
        while (*p != '\0' && *p != ' ' && ext < 3u) {
            name83[8u + ext++] = upd_upper_name_char(*p++);
        }
    } else if (*p != ' ' && *p != '\0') {
        return false;
    }
    if (*p != '\0' && *p != ' ') {
        return false;
    }
    *cursor = p;
    return base != 0u;
}

static void upd_send_name83(const char name83[11]) {
    for (uint8_t i = 0u; i < 8u; ++i) {
        if (name83[i] != ' ') {
            upd_send_char(name83[i]);
        }
    }
    if (name83[8] != ' ' || name83[9] != ' ' || name83[10] != ' ') {
        upd_send_char('.');
        for (uint8_t i = 8u; i < 11u; ++i) {
            if (name83[i] != ' ') {
                upd_send_char(name83[i]);
            }
        }
    }
}

static uint32_t upd_crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (uint8_t i = 0; i < 8u; ++i) {
        if ((crc & 1u) != 0u) {
            crc = (crc >> 1) ^ 0xEDB88320u;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

static void upd_reset_transfer(void) {
    g_upd.receiving = false;
    g_upd.complete = false;
    g_upd.overflow = false;
    g_upd.expected_size = 0;
    g_upd.expected_crc = 0;
    g_upd.received_size = 0;
    g_upd.crc = 0xFFFFFFFFu;
    g_upd.first_count = 0;
    for (uint8_t i = 0; i < (uint8_t)sizeof(g_upd.first_bytes); ++i) {
        g_upd.first_bytes[i] = 0;
    }
}

#define RAMFUNC __attribute__((section(".ramfunc"), noinline))

static uint32_t RAMFUNC upd_ram_crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (uint8_t i = 0; i < 8u; ++i) {
        crc = (crc & 1u) != 0u ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return crc;
}

static char RAMFUNC upd_ram_hex_digit(uint8_t value) {
    value &= 0x0Fu;
    return (char)(value < 10u ? ('0' + value) : ('A' + value - 10u));
}

static uint8_t RAMFUNC upd_ram_hex_value(char ch, bool *ok) {
    if (ch >= '0' && ch <= '9') {
        return (uint8_t)(ch - '0');
    }
    if (ch >= 'A' && ch <= 'F') {
        return (uint8_t)(ch - 'A' + 10);
    }
    if (ch >= 'a' && ch <= 'f') {
        return (uint8_t)(ch - 'a' + 10);
    }
    *ok = false;
    return 0;
}

static bool RAMFUNC upd_ram_parse_hex32(const char **cursor, uint32_t *value) {
    bool ok = true;
    uint32_t out = 0;
    uint8_t count = 0;
    const char *p = *cursor;
    while ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')) {
        if (count >= 8u) {
            return false;
        }
        out = (out << 4) | upd_ram_hex_value(*p, &ok);
        ++p;
        ++count;
    }
    if (!ok || count == 0u) {
        return false;
    }
    *cursor = p;
    *value = out;
    return true;
}

static void RAMFUNC upd_ram_send_char(char ch) {
    while ((REG32(USART_SR(USART2_BASE)) & BIT(7)) == 0u) {
    }
    REG32(USART_DR(USART2_BASE)) = (uint8_t)ch;
}

static void RAMFUNC upd_ram_send_hex32(uint32_t value) {
    for (uint8_t i = 0; i < 8u; ++i) {
        upd_ram_send_char(upd_ram_hex_digit((uint8_t)(value >> (28u - i * 4u))));
    }
}

static void RAMFUNC upd_ram_send_crlf(void) {
    upd_ram_send_char('\r');
    upd_ram_send_char('\n');
}

static void RAMFUNC upd_ram_delay_cycles(uint32_t count) {
    while (count-- != 0u) {
        __asm__ volatile("nop");
    }
}

static void RAMFUNC upd_ram_send_code(uint32_t code, uint8_t len) {
    for (uint8_t i = 0; i < len; ++i) {
        upd_ram_send_char((char)(code >> (i * 8u)));
    }
}

static bool RAMFUNC upd_ram_baud_supported(uint32_t baud) {
    return baud == 19200u || baud == 38400u || baud == 57600u || baud == 115200u ||
           baud == 230400u || baud == 460800u || baud == 921600u;
}

#define UPD_RAM_CODE4(a, b, c, d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

static void RAMFUNC upd_ram_reply_err_code(uint32_t code, uint8_t len) {
    upd_ram_send_char('U');
    upd_ram_send_char('E');
    upd_ram_send_char('R');
    upd_ram_send_char('R');
    upd_ram_send_char(' ');
    upd_ram_send_code(code, len);
    upd_ram_send_crlf();
}

static void RAMFUNC upd_ram_send_ok_code(uint32_t code, uint8_t len) {
    upd_ram_send_char('U');
    upd_ram_send_char('O');
    upd_ram_send_char('K');
    upd_ram_send_char(' ');
    upd_ram_send_code(code, len);
    upd_ram_send_crlf();
}

static void RAMFUNC upd_ram_send_header(bool flash_mode, uint32_t expected_size, uint32_t expected_crc) {
    upd_ram_send_char('U');
    upd_ram_send_char('R');
    upd_ram_send_char('A');
    upd_ram_send_char('M');
    upd_ram_send_char(' ');
    if (flash_mode) {
        upd_ram_send_code(UPD_RAM_CODE4('F', 'L', 'A', 'S'), 4);
        upd_ram_send_char('H');
    } else {
        upd_ram_send_code(UPD_RAM_CODE4('S', 'I', 'M', ' '), 3);
    }
    upd_ram_send_char(' ');
    upd_ram_send_code(UPD_RAM_CODE4('s', 'i', 'z', 'e'), 4);
    upd_ram_send_char('=');
    upd_ram_send_hex32(expected_size);
    upd_ram_send_char(' ');
    upd_ram_send_code(UPD_RAM_CODE4('c', 'r', 'c', '='), 4);
    upd_ram_send_hex32(expected_crc);
    upd_ram_send_char(' ');
    upd_ram_send_code(UPD_RAM_CODE4('b', 'o', 'o', 't'), 4);
    upd_ram_send_code(UPD_RAM_CODE4('l', 'o', 'c', 'k'), 4);
    upd_ram_send_char('=');
    upd_ram_send_code(UPD_RAM_CODE4('0', '8', '0', '0'), 4);
    upd_ram_send_code(UPD_RAM_CODE4('0', '0', '0', '0'), 4);
    upd_ram_send_char('-');
    upd_ram_send_code(UPD_RAM_CODE4('0', '8', '0', '0'), 4);
    upd_ram_send_code(UPD_RAM_CODE4('5', 'F', 'F', 'F'), 4);
    upd_ram_send_crlf();
}

static uint32_t RAMFUNC upd_ram_brr_for_baud(uint32_t baud) {
    if (baud == 921600u) {
        return USART2_BRR_FLASH_921600;
    }
    if (baud == 460800u) {
        return USART2_BRR_FLASH_460800;
    }
    if (baud == 230400u) {
        return USART2_BRR_FLASH_230400;
    }
    if (baud == 115200u) {
        return USART2_BRR_FLASH_115200;
    }
    if (baud == 57600u) {
        return USART2_BRR_FLASH_57600;
    }
    if (baud == 38400u) {
        return USART2_BRR_FLASH_38400;
    }
    return USART2_BRR_STOCK_19200;
}

static void RAMFUNC upd_ram_uart_set_baud(uint32_t baud) {
    if (baud == 19200u) {
        return;
    }
    while ((REG32(USART_SR(USART2_BASE)) & BIT(6)) == 0u) {
    }
    uint32_t cr1 = REG32(USART_CR1(USART2_BASE));
    REG32(USART_CR1(USART2_BASE)) = cr1 & ~BIT(13);
    REG32(USART_BRR(USART2_BASE)) = upd_ram_brr_for_baud(baud);
    REG32(USART_CR1(USART2_BASE)) = cr1;
    upd_ram_delay_cycles(600000u);
}

static void RAMFUNC upd_ram_send_switch_baud(uint32_t data_baud) {
    if (data_baud == 19200u) {
        return;
    }
    upd_ram_send_code(UPD_RAM_CODE4('U', 'S', 'W', 'I'), 4);
    upd_ram_send_code(UPD_RAM_CODE4('T', 'C', 'H', ' '), 4);
    upd_ram_send_code(UPD_RAM_CODE4('b', 'a', 'u', 'd'), 4);
    upd_ram_send_char('=');
    upd_ram_send_hex32(data_baud);
    upd_ram_send_crlf();
    while ((REG32(USART_SR(USART2_BASE)) & BIT(6)) == 0u) {
    }
    upd_ram_delay_cycles(80000u);
}

static void RAMFUNC upd_ram_send_ack(uint32_t received) {
    upd_ram_send_char('U');
    upd_ram_send_char('A');
    upd_ram_send_char('C');
    upd_ram_send_char('K');
    upd_ram_send_char(' ');
    upd_ram_send_hex32(received);
    upd_ram_send_crlf();
}

static void RAMFUNC upd_ram_send_sim_ok(uint32_t erased_pages, uint32_t expected_size, uint32_t final_crc) {
    upd_ram_send_char('U');
    upd_ram_send_char('F');
    upd_ram_send_char('S');
    upd_ram_send_char('I');
    upd_ram_send_char('M');
    upd_ram_send_char('O');
    upd_ram_send_char('K');
    upd_ram_send_char(' ');
    upd_ram_send_code(UPD_RAM_CODE4('p', 'a', 'g', 'e'), 4);
    upd_ram_send_char('s');
    upd_ram_send_char('=');
    upd_ram_send_hex32(erased_pages);
    upd_ram_send_char(' ');
    upd_ram_send_code(UPD_RAM_CODE4('s', 'i', 'z', 'e'), 4);
    upd_ram_send_char('=');
    upd_ram_send_hex32(expected_size);
    upd_ram_send_char(' ');
    upd_ram_send_code(UPD_RAM_CODE4('c', 'r', 'c', '='), 4);
    upd_ram_send_hex32(final_crc);
    upd_ram_send_char(' ');
    upd_ram_send_code(UPD_RAM_CODE4('r', 'e', 's', 't'), 4);
    upd_ram_send_code(UPD_RAM_CODE4('a', 'r', 't', '='), 4);
    upd_ram_send_char('U');
    upd_ram_send_char('B');
    upd_ram_send_crlf();
}

static void RAMFUNC upd_ram_send_flash_ok(uint32_t final_crc) {
    upd_ram_send_char('U');
    upd_ram_send_char('F');
    upd_ram_send_char('L');
    upd_ram_send_char('A');
    upd_ram_send_char('S');
    upd_ram_send_char('H');
    upd_ram_send_char('O');
    upd_ram_send_char('K');
    upd_ram_send_char(' ');
    upd_ram_send_code(UPD_RAM_CODE4('r', 'e', 's', 'e'), 4);
    upd_ram_send_char('t');
    upd_ram_send_char(' ');
    upd_ram_send_code(UPD_RAM_CODE4('c', 'r', 'c', '='), 4);
    upd_ram_send_hex32(final_crc);
    upd_ram_send_crlf();
}

static uint8_t RAMFUNC upd_ram_recv_byte(void) {
    while ((REG32(USART_SR(USART2_BASE)) & BIT(5)) == 0u) {
    }
    return (uint8_t)REG32(USART_DR(USART2_BASE));
}

static bool RAMFUNC upd_ram_read_line(char *line, uint16_t max_len) {
    uint16_t len = 0;
    while (1) {
        uint8_t byte = upd_ram_recv_byte();
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            line[len] = '\0';
            return true;
        }
        if (byte < 0x20u || byte > 0x7Eu || len >= (uint16_t)(max_len - 1u)) {
            line[0] = '\0';
            return false;
        }
        line[len++] = (char)byte;
    }
}

static bool RAMFUNC upd_ram_vector_plausible(const uint8_t first[16], uint8_t first_count) {
    if (first_count < 8u) {
        return false;
    }
    uint32_t stack = first[0] | ((uint32_t)first[1] << 8) | ((uint32_t)first[2] << 16) | ((uint32_t)first[3] << 24);
    uint32_t reset = first[4] | ((uint32_t)first[5] << 8) | ((uint32_t)first[6] << 16) | ((uint32_t)first[7] << 24);
    return stack >= UPD_RAM_MIN && stack <= UPD_RAM_MAX && (stack & 3u) == 0u &&
           reset >= APP_FLASH_BASE && reset < (APP_FLASH_BASE + UPD_APP_MAX_SIZE) && (reset & 1u) != 0u;
}

static uint32_t RAMFUNC upd_ram_flash_unlock(void) {
    uint32_t errors = FLASH_SR_ERRORS;
    while ((REG32(FLASH_SR) & FLASH_SR_BSY) != 0u) {
    }
    if ((REG32(FLASH_PECR) & FLASH_PECR_PELOCK) != 0u) {
        REG32(FLASH_PEKEYR) = 0x89ABCDEFu;
        REG32(FLASH_PEKEYR) = 0x02030405u;
    }
    if ((REG32(FLASH_PECR) & FLASH_PECR_PRGLOCK) != 0u) {
        REG32(FLASH_PRGKEYR) = 0x8C9DAEBFu;
        REG32(FLASH_PRGKEYR) = 0x13141516u;
    }
    REG32(FLASH_SR) = FLASH_SR_EOP | errors;
    return (REG32(FLASH_PECR) & (FLASH_PECR_PELOCK | FLASH_PECR_PRGLOCK)) == 0u ? 0u : 1u;
}

static uint32_t RAMFUNC upd_ram_erase_page(uint32_t addr) {
    uint32_t errors = FLASH_SR_ERRORS;
    if (addr < APP_FLASH_BASE || addr >= (APP_FLASH_BASE + UPD_APP_MAX_SIZE) || (addr & (UPD_FLASH_PAGE_SIZE - 1u)) != 0u) {
        return 1u;
    }
    REG32(FLASH_SR) = FLASH_SR_EOP | errors;
    REG32(FLASH_PECR) |= FLASH_PECR_ERASE | FLASH_PECR_PROG;
    REG32(addr) = 0u;
    while ((REG32(FLASH_SR) & FLASH_SR_BSY) != 0u) {
    }
    REG32(FLASH_PECR) &= ~(FLASH_PECR_ERASE | FLASH_PECR_PROG);
    return (REG32(FLASH_SR) & errors) == 0u ? 0u : 2u;
}

static uint32_t RAMFUNC upd_ram_flash_wait(void) {
    uint32_t guard = 0x8000u;
    while ((REG32(FLASH_SR) & FLASH_SR_BSY) != 0u && guard != 0u) {
        --guard;
    }
    if (guard == 0u) {
        return 5u;
    }
    if ((REG32(FLASH_SR) & FLASH_SR_WRPERR) != 0u) {
        return 2u;
    }
    if ((REG32(FLASH_SR) & FLASH_SR_ERRORS) != 0u) {
        return 3u;
    }
    return 4u;
}

static uint32_t RAMFUNC upd_ram_program_half_page(uint32_t offset, const uint8_t half[UPD_FLASH_HALF_PAGE_SIZE]) {
    if ((offset & (UPD_FLASH_HALF_PAGE_SIZE - 1u)) != 0u || offset > UPD_APP_MAX_SIZE ||
        UPD_FLASH_HALF_PAGE_SIZE > (UPD_APP_MAX_SIZE - offset)) {
        return 1u;
    }
    if (upd_ram_flash_wait() != 4u) {
        return 2u;
    }

    uint32_t actlr = REG32(SCB_ACTLR);
    REG32(SCB_ACTLR) = actlr | 1u;
    REG32(FLASH_SR) = FLASH_SR_EOP | FLASH_SR_ERRORS;
    REG32(FLASH_PECR) |= FLASH_PECR_FPRG;
    REG32(FLASH_PECR) |= FLASH_PECR_PROG;

    for (uint8_t i = 0; i < UPD_FLASH_HALF_PAGE_SIZE; i += 4u) {
        uint32_t word = half[i] | ((uint32_t)half[i + 1u] << 8) |
                        ((uint32_t)half[i + 2u] << 16) | ((uint32_t)half[i + 3u] << 24);
        REG32(APP_FLASH_BASE + offset + i) = word;
    }

    uint32_t status = upd_ram_flash_wait();
    REG32(FLASH_PECR) &= ~FLASH_PECR_PROG;
    REG32(FLASH_PECR) &= ~FLASH_PECR_FPRG;
    REG32(SCB_ACTLR) = actlr;
    return status == 4u ? 0u : 2u;
}

static uint32_t RAMFUNC upd_ram_verify_chunk(uint32_t offset, const uint8_t *chunk, uint8_t len) {
    for (uint8_t i = 0; i < len; ++i) {
        if (REG8(APP_FLASH_BASE + offset + i) != chunk[i]) {
            return 1u;
        }
    }
    return 0u;
}

static void RAMFUNC upd_stream_session(bool flash_mode, uint32_t initial_size, uint32_t initial_crc,
                                       uint32_t initial_data_baud) {
    char line[UPD_LINE_MAX];
    uint8_t chunk[UPD_STREAM_CHUNK_MAX];
    uint8_t half_page[UPD_FLASH_HALF_PAGE_SIZE];
    uint8_t first[16];
    uint8_t first_count = 0;
    uint32_t expected_size = initial_size;
    uint32_t expected_crc = initial_crc;
    uint32_t data_baud = initial_data_baud;
    uint32_t received = 0;
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t next_page = APP_FLASH_BASE;
    uint32_t next_half_offset = 0;
    uint8_t half_fill = 0;
    uint32_t erased_pages = 0;
    bool active = true;

    REG32(SYST_CSR) = 0;
    for (uint32_t i = 0; i < 8u; ++i) {
        REG32(NVIC_ICER0 + i * 4u) = 0xFFFFFFFFu;
        REG32(NVIC_ICPR0 + i * 4u) = 0xFFFFFFFFu;
    }
    if (flash_mode && upd_ram_flash_unlock() != 0u) {
        upd_ram_reply_err_code(UPD_RAM_CODE4('U', 'N', 'L', 'K'), 4);
        flash_mode = false;
    }

    upd_ram_send_switch_baud(data_baud);
    upd_ram_uart_set_baud(data_baud);
    upd_ram_send_header(flash_mode, expected_size, expected_crc);

    while (1) {
        if (!upd_ram_read_line(line, sizeof(line))) {
            upd_ram_reply_err_code(UPD_RAM_CODE4('L', 'I', 'N', 'E'), 4);
            continue;
        }
        if (line[0] != 'U') {
            continue;
        }
        if (line[1] == 'A' && line[2] == '\0') {
            received = 0;
            crc = 0xFFFFFFFFu;
            first_count = 0;
            next_page = APP_FLASH_BASE;
            next_half_offset = 0;
            half_fill = 0;
            erased_pages = 0;
            active = false;
            upd_ram_send_ok_code(UPD_RAM_CODE4('A', 'B', 'O', 'R'), 4);
            continue;
        }
        bool restart_cmd = false;
        bool restart_flash_mode = flash_mode;
        const char *restart_args = 0;
        if (line[1] == 'B') {
            restart_cmd = true;
            restart_args = &line[2];
        } else if (line[1] == 'F' && line[2] == 'B' && line[3] == 'E' && line[4] == 'G' &&
                   line[5] == 'I' && line[6] == 'N') {
            restart_cmd = true;
            restart_flash_mode = true;
            restart_args = &line[7];
        } else if (line[1] == 'S' && line[2] == 'I' && line[3] == 'M') {
            restart_cmd = true;
            restart_flash_mode = false;
            restart_args = &line[4];
        }
        if (restart_cmd) {
            const char *p = restart_args;
            while (*p == ' ') {
                ++p;
            }
            if (!upd_ram_parse_hex32(&p, &expected_size)) {
                upd_ram_reply_err_code(UPD_RAM_CODE4('B', 'E', 'G', 'I'), 4);
                continue;
            }
            while (*p == ' ') {
                ++p;
            }
            if (!upd_ram_parse_hex32(&p, &expected_crc) || expected_size == 0u ||
                expected_size > UPD_APP_MAX_SIZE || (expected_size & 3u) != 0u) {
                upd_ram_reply_err_code(UPD_RAM_CODE4('B', 'E', 'G', 'I'), 4);
                continue;
            }
            while (*p == ' ') {
                ++p;
            }
            uint32_t new_data_baud = data_baud;
            if (*p != '\0') {
                uint32_t parsed_baud = 0;
                if (!upd_ram_parse_hex32(&p, &parsed_baud) || !upd_ram_baud_supported(parsed_baud)) {
                    upd_ram_reply_err_code(UPD_RAM_CODE4('B', 'A', 'U', 'D'), 4);
                    continue;
                }
                new_data_baud = parsed_baud;
            }
            received = 0;
            crc = 0xFFFFFFFFu;
            first_count = 0;
            next_page = APP_FLASH_BASE;
            next_half_offset = 0;
            half_fill = 0;
            erased_pages = 0;
            flash_mode = restart_flash_mode;
            if (flash_mode && upd_ram_flash_unlock() != 0u) {
                upd_ram_reply_err_code(UPD_RAM_CODE4('U', 'N', 'L', 'K'), 4);
                flash_mode = false;
                active = false;
                continue;
            }
            active = true;
            data_baud = new_data_baud;
            upd_ram_send_switch_baud(data_baud);
            upd_ram_uart_set_baud(data_baud);
            if (line[1] == 'B') {
                upd_ram_send_ok_code(UPD_RAM_CODE4('R', 'E', 'S', 'T'), 4);
            } else {
                upd_ram_send_header(flash_mode, expected_size, expected_crc);
            }
            continue;
        }
        if ((line[1] == 'E' && line[2] == '\0') || (line[1] == 'F' && line[2] == '\0')) {
            uint32_t final_crc = crc ^ 0xFFFFFFFFu;
            if (!active || received != expected_size) {
                upd_ram_reply_err_code(UPD_RAM_CODE4('C', 'O', 'U', 'N'), 4);
                continue;
            }
            if (final_crc != expected_crc) {
                upd_ram_reply_err_code(UPD_RAM_CODE4('C', 'R', 'C', ' '), 3);
                continue;
            }
            if (!upd_ram_vector_plausible(first, first_count)) {
                upd_ram_reply_err_code(UPD_RAM_CODE4('V', 'E', 'C', 'T'), 4);
                continue;
            }
            if (flash_mode) {
                if (half_fill != 0u) {
                    while (half_fill < UPD_FLASH_HALF_PAGE_SIZE) {
                        half_page[half_fill++] = 0xFFu;
                    }
                    if (upd_ram_program_half_page(next_half_offset, half_page) != 0u ||
                        upd_ram_verify_chunk(next_half_offset, half_page, UPD_FLASH_HALF_PAGE_SIZE) != 0u) {
                        upd_ram_reply_err_code(UPD_RAM_CODE4('W', 'R', 'I', 'T'), 4);
                        active = false;
                        half_fill = 0;
                        continue;
                    }
                    next_half_offset += UPD_FLASH_HALF_PAGE_SIZE;
                    half_fill = 0;
                }
                upd_ram_send_flash_ok(final_crc);
                while ((REG32(USART_SR(USART2_BASE)) & BIT(6)) == 0u) {
                }
                REG32(SCB_AIRCR) = 0x05FA0004u;
                while (1) {
                }
            }
            upd_ram_send_sim_ok(erased_pages, expected_size, final_crc);
            continue;
        }
        if (line[1] != 'D') {
            upd_ram_reply_err_code(UPD_RAM_CODE4('C', 'M', 'D', ' '), 3);
            continue;
        }
        if (!active) {
            upd_ram_reply_err_code(UPD_RAM_CODE4('I', 'D', 'L', 'E'), 4);
            continue;
        }

        const char *p = &line[2];
        while (*p == ' ') {
            ++p;
        }
        uint8_t chunk_len = 0;
        bool ok = true;
        while (*p != '\0') {
            if (*p == ' ') {
                ++p;
                continue;
            }
            if (p[1] == '\0' || chunk_len >= UPD_STREAM_CHUNK_MAX) {
                ok = false;
                break;
            }
            chunk[chunk_len++] = (uint8_t)((upd_ram_hex_value(p[0], &ok) << 4) | upd_ram_hex_value(p[1], &ok));
            if (!ok) {
                break;
            }
            p += 2;
        }
        if (!ok || chunk_len == 0u || (chunk_len & 3u) != 0u ||
            received > expected_size || chunk_len > (expected_size - received)) {
            upd_ram_reply_err_code(UPD_RAM_CODE4('D', 'A', 'T', 'A'), 4);
            continue;
        }
        for (uint8_t i = 0; i < chunk_len; ++i) {
            if (first_count < sizeof(first)) {
                first[first_count++] = chunk[i];
            }
            crc = upd_ram_crc32_byte(crc, chunk[i]);
        }
        if (received == 0u && !upd_ram_vector_plausible(first, first_count)) {
            upd_ram_reply_err_code(UPD_RAM_CODE4('V', 'E', 'C', 'T'), 4);
            continue;
        }
        if (flash_mode) {
            uint32_t chunk_start = APP_FLASH_BASE + received;
            uint32_t chunk_end = chunk_start + chunk_len;
            while (next_page < chunk_end) {
                if (upd_ram_erase_page(next_page) != 0u) {
                    upd_ram_reply_err_code(UPD_RAM_CODE4('E', 'R', 'A', 'S'), 4);
                    active = false;
                    break;
                }
                next_page += UPD_FLASH_PAGE_SIZE;
                ++erased_pages;
            }
            if (!active) {
                continue;
            }
            for (uint8_t i = 0; i < chunk_len; ++i) {
                half_page[half_fill++] = chunk[i];
                if (half_fill == UPD_FLASH_HALF_PAGE_SIZE) {
                    if (upd_ram_program_half_page(next_half_offset, half_page) != 0u ||
                        upd_ram_verify_chunk(next_half_offset, half_page, UPD_FLASH_HALF_PAGE_SIZE) != 0u) {
                        upd_ram_reply_err_code(UPD_RAM_CODE4('W', 'R', 'I', 'T'), 4);
                        active = false;
                        break;
                    }
                    next_half_offset += UPD_FLASH_HALF_PAGE_SIZE;
                    half_fill = 0;
                }
            }
            if (!active) {
                continue;
            }
        } else {
            uint32_t last = received + chunk_len;
            uint32_t page_count = (last + (UPD_FLASH_PAGE_SIZE - 1u)) / UPD_FLASH_PAGE_SIZE;
            if (page_count > erased_pages) {
                erased_pages = page_count;
            }
        }
        received += chunk_len;
        upd_ram_send_ack(received);
    }
}

static uint32_t upd_le32_at(uint8_t offset) {
    return ((uint32_t)g_upd.first_bytes[offset]) |
           ((uint32_t)g_upd.first_bytes[offset + 1u] << 8) |
           ((uint32_t)g_upd.first_bytes[offset + 2u] << 16) |
           ((uint32_t)g_upd.first_bytes[offset + 3u] << 24);
}

static bool upd_image_header_plausible(void) {
    if (g_upd.first_count < 8u) {
        return false;
    }

    uint32_t stack = upd_le32_at(0);
    uint32_t reset = upd_le32_at(4);
    if (stack < UPD_RAM_MIN || stack > UPD_RAM_MAX || (stack & 3u) != 0u) {
        return false;
    }
    if (reset < APP_FLASH_BASE || reset >= (APP_FLASH_BASE + UPD_APP_MAX_SIZE) || (reset & 1u) == 0u) {
        return false;
    }
    return true;
}

static void upd_reply_ok(void) {
    upd_send_text("UOK\r\n");
}

static void upd_reply_err(const char *reason) {
    upd_send_text("UERR ");
    upd_send_text(reason);
    upd_send_text("\r\n");
}

static void upd_reply_state(void) {
    upd_send_text("USTATE recv=");
    upd_send_hex32(g_upd.received_size);
    upd_send_text(" size=");
    upd_send_hex32(g_upd.expected_size);
    upd_send_text(" crc=");
    upd_send_hex32(g_upd.crc ^ 0xFFFFFFFFu);
    upd_send_text(g_upd.receiving ? " active=1" : " active=0");
    upd_send_text(g_upd.complete ? " complete=1" : " complete=0");
    upd_send_text("\r\n");
}

static void upd_reply_rx_line(void) {
#if EEV_UART_UPDATE_TRACE
    upd_send_text("URX ");
    upd_send_hex16(g_upd.line_len);
    upd_send_char(' ');
    for (uint16_t i = 0; i < g_upd.line_len; ++i) {
        upd_send_hex8((uint8_t)g_upd.line[i]);
    }
    upd_send_text("\r\n");
#endif
}

static void upd_cmd_info(void) {
    upd_send_text("UINFO proto=1 safe=1 app=");
    upd_send_hex32(APP_FLASH_BASE);
    upd_send_text(" max=");
    upd_send_hex32(UPD_APP_MAX_SIZE);
    upd_send_text(" stream=1 chunk=");
    upd_send_hex32(UPD_STREAM_CHUNK_MAX);
    upd_send_text(" cal=");
    upd_send_hex32(CAL_FLASH_BASE);
    upd_send_text(" transport=uart-ble debug=");
    upd_send_char(g_upd_debug_enabled ? '1' : '0');
    upd_send_text(" bt=");
    upd_send_char(g_upd_bt_stream_enabled ? '1' : '0');
    upd_send_text("\r\n");
}

static void upd_cmd_clock_diag(void) {
    uint32_t cfgr = REG32(RCC_CFGR);
    uint32_t icscr = REG32(RCC_ICSCR);

    upd_send_text("UCLK core=");
    upd_send_hex32(clock_core_hz());
    upd_send_text(" apb1=");
    upd_send_hex32(clock_apb1_hz());
    upd_send_text(" cr=");
    upd_send_hex32(REG32(RCC_CR));
    upd_send_text(" icscr=");
    upd_send_hex32(icscr);
    upd_send_text(" cfgr=");
    upd_send_hex32(cfgr);
    upd_send_text(" sw=");
    upd_send_hex8((uint8_t)(cfgr & 0x3u));
    upd_send_text(" sws=");
    upd_send_hex8((uint8_t)((cfgr >> 2) & 0x3u));
    upd_send_text(" hpre=");
    upd_send_hex8((uint8_t)((cfgr >> 4) & 0xFu));
    upd_send_text(" ppre1=");
    upd_send_hex8((uint8_t)((cfgr >> 8) & 0x7u));
    upd_send_text(" ppre2=");
    upd_send_hex8((uint8_t)((cfgr >> 11) & 0x7u));
    upd_send_text(" msir=");
    upd_send_hex8((uint8_t)((icscr >> 13) & 0x7u));
    upd_send_text(" syst=");
    upd_send_hex32(REG32(SYST_RVR));
    upd_send_text(" brr=");
    upd_send_hex32(REG32(USART_BRR(USART2_BASE)));
    upd_send_text("\r\n");
}

static void upd_cmd_temperature_diag(void) {
    uint16_t raw = 0;
    int16_t temp_tenths = 0;
    int16_t temp_centideg = 0;
    bool raw_ok = analog_read_adc_channel(4u, &raw);
    analog_power_down();
    bool temp_ok = temperature_update_from_raw_centideg(raw, &temp_centideg);
    if (temp_ok) {
        temp_tenths = (int16_t)(temp_centideg / 10);
        if ((temp_tenths % 10) >= 5) {
            ++temp_tenths;
        }
    } else {
        temp_tenths = 10000;
    }

    upd_send_text("UTEMP raw_ok=");
    upd_send_hex8(raw_ok ? 1u : 0u);
    upd_send_text(" raw=");
    upd_send_hex16(raw);
    upd_send_text(" temp_ok=");
    upd_send_hex8(temp_ok ? 1u : 0u);
    upd_send_text(" tenths=");
    upd_send_hex16((uint16_t)temp_tenths);
    upd_send_text(" centi=");
    upd_send_hex16((uint16_t)temp_centideg);
    upd_send_text(" cal=");
    upd_send_hex16(calibration_read_u16_be(0x02E0u));
    upd_send_text("\r\n");
}

static void upd_cmd_temperature_override(const char *args) {
    const char *p = args;
    uint32_t parsed = 0;
    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &parsed)) {
        upd_reply_err("TOVR");
        return;
    }

    if (parsed == 0xFFFFFFFFu) {
        g_upd_temperature_override_enabled = false;
    } else {
        g_upd_temperature_override_tenths_c = (int16_t)(uint16_t)parsed;
        g_upd_temperature_override_enabled = true;
    }

    if (*p != '\0') {
        uint32_t battery = 0;
        if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &battery)) {
            upd_reply_err("TOVR");
            return;
        }
        if (battery == 0xFFu || battery == 0xFFFFFFFFu) {
            g_upd_battery_override_enabled = false;
        } else if (battery <= 99u) {
            g_upd_battery_override_enabled = true;
            g_upd_battery_override_percent = (uint8_t)battery;
            if (*p != '\0') {
                uint32_t low = 0;
                if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &low) || low > 1u) {
                    upd_reply_err("TOVR");
                    return;
                }
                g_upd_battery_override_low = low != 0u;
            }
        } else {
            upd_reply_err("TOVR");
            return;
        }
    }

    upd_send_text("UTOVR en=");
    upd_send_hex8(g_upd_temperature_override_enabled ? 1u : 0u);
    upd_send_text(" tenths=");
    upd_send_hex16((uint16_t)g_upd_temperature_override_tenths_c);
    upd_send_text(" bat_en=");
    upd_send_hex8(g_upd_battery_override_enabled ? 1u : 0u);
    upd_send_text(" bat_pct=");
    upd_send_hex8(g_upd_battery_override_percent);
    upd_send_text(" bat_low=");
    upd_send_hex8(g_upd_battery_override_low ? 1u : 0u);
    upd_send_text("\r\n");
}

static void upd_cmd_battery_override(const char *args) {
    const char *p = args;
    uint32_t parsed = 0;
    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &parsed)) {
        upd_reply_err("BOVR");
        return;
    }

    if (parsed == 0xFFu || parsed == 0xFFFFFFFFu) {
        g_upd_battery_override_enabled = false;
    } else if (parsed <= 99u) {
        g_upd_battery_override_enabled = true;
        g_upd_battery_override_percent = (uint8_t)parsed;
        if (*p != '\0') {
            uint32_t low = 0;
            if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &low) || low > 1u) {
                upd_reply_err("BOVR");
                return;
            }
            g_upd_battery_override_low = low != 0u;
        }
    } else {
        upd_reply_err("BOVR");
        return;
    }

    upd_send_text("UBOVR en=");
    upd_send_hex8(g_upd_battery_override_enabled ? 1u : 0u);
    upd_send_text(" pct=");
    upd_send_hex8(g_upd_battery_override_percent);
    upd_send_text(" low=");
    upd_send_hex8(g_upd_battery_override_low ? 1u : 0u);
    upd_send_text("\r\n");
}

static void upd_cmd_temperature_unit_override(const char *args) {
    const char *p = args;
    uint32_t parsed = 0;
    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &parsed)) {
        upd_reply_err("TUNIT");
        return;
    }

    if (parsed == 0xFFu || parsed == 0xFFFFFFFFu) {
        g_upd_temperature_unit_override_enabled = false;
    } else if (parsed == 0u || parsed == 1u) {
        g_upd_temperature_unit_override_enabled = true;
        g_upd_temperature_unit_override_fahrenheit = parsed == 0u;
    } else {
        upd_reply_err("TUNIT");
        return;
    }

    upd_send_text("UTUNIT en=");
    upd_send_hex8(g_upd_temperature_unit_override_enabled ? 1u : 0u);
    upd_send_text(" unit=");
    upd_send_text(upd_temperature_fahrenheit_enabled() ? "F" : "C");
    upd_send_text("\r\n");
}

static void upd_cmd_battery_diag(void) {
    uint16_t raw = 0;
    uint16_t decivolts = 0;
    bool low = false;
    bool raw_ok = analog_read_adc_channel(5u, &raw);
    analog_power_down();
    bool bat_ok = battery_sample_stock(&decivolts, &low);

    upd_send_text("UBAT raw_ok=");
    upd_send_hex8(raw_ok ? 1u : 0u);
    upd_send_text(" raw=");
    upd_send_hex16(raw);
    upd_send_text(" bat_ok=");
    upd_send_hex8(bat_ok ? 1u : 0u);
    upd_send_text(" deciv=");
    upd_send_hex16(decivolts);
    upd_send_text(" low=");
    upd_send_hex8(low ? 1u : 0u);
    upd_send_text(" ovr=");
    upd_send_hex8(g_upd_battery_override_enabled ? 1u : 0u);
    upd_send_text(" pct=");
    upd_send_hex8(g_upd_battery_override_percent);
    upd_send_text(" ovr_low=");
    upd_send_hex8(g_upd_battery_override_low ? 1u : 0u);
    upd_send_text("\r\n");
}

static void upd_reply_debug_state(void) {
    upd_send_text("UDBG ");
    upd_send_char(g_upd_debug_enabled ? '1' : '0');
    upd_send_text("\r\n");
}

static void upd_cmd_debug(const char *args) {
    const char *p = args;

    while (*p == ' ') {
        ++p;
    }

    if (*p == '\0' || *p == '?') {
        upd_reply_debug_state();
        return;
    }
    if (*p == '0' || *p == 'F' || *p == 'f' ||
        ((*p == 'O' || *p == 'o') && (p[1] == 'F' || p[1] == 'f')) ||
        ((*p == 'S' || *p == 's') && (p[1] == 'T' || p[1] == 't'))) {
        g_upd_debug_enabled = false;
        upd_reply_debug_state();
        return;
    }
    if (*p == '1' || *p == 'N' || *p == 'n' ||
        ((*p == 'O' || *p == 'o') && (p[1] == 'N' || p[1] == 'n'))) {
        g_upd_debug_enabled = true;
        upd_reply_debug_state();
        return;
    }

    upd_reply_err("DEBUG");
}

static void upd_reply_bt_stream_state(void) {
    upd_send_text("UBT ");
    upd_send_char(g_upd_bt_stream_enabled ? '1' : '0');
    upd_send_text("\r\n");
}

static void upd_reply_ble_power_state(void) {
    uint32_t fodr = REG32(GPIO_ODR(GPIOF_BASE));
    uint32_t fidr = REG32(GPIO_IDR(GPIOF_BASE));
    uint32_t godr = REG32(GPIO_ODR(GPIOG_BASE));
    uint32_t gidr = REG32(GPIO_IDR(GPIOG_BASE));
    upd_send_text("UBLEPWR ");
    upd_send_char((fodr & BIT(15)) != 0u ? '1' : '0');
    upd_send_text(" fODR=");
    upd_send_hex32(fodr);
    upd_send_text(" fIDR=");
    upd_send_hex32(fidr);
    upd_send_text(" gODR=");
    upd_send_hex32(godr);
    upd_send_text(" gIDR=");
    upd_send_hex32(gidr);
    upd_send_text("\r\n");
}

static void upd_cmd_ble_power(const char *args) {
    const char *p = args;

    while (*p == ' ') {
        ++p;
    }

    if (*p == '\0' || *p == '?') {
        upd_reply_ble_power_state();
        return;
    }
    if (*p == '0' || *p == 'F' || *p == 'f' ||
        ((*p == 'O' || *p == 'o') && (p[1] == 'F' || p[1] == 'f'))) {
        bluetooth_set_enable(false);
        upd_reply_ble_power_state();
        return;
    }
    if (*p == '1' || *p == 'N' || *p == 'n' ||
        ((*p == 'O' || *p == 'o') && (p[1] == 'N' || p[1] == 'n'))) {
        bluetooth_set_enable(true);
        upd_reply_ble_power_state();
        return;
    }

    upd_reply_err("BLEPWR");
}

static void upd_cmd_bt_stream(const char *args) {
    const char *p = args;

    while (*p == ' ') {
        ++p;
    }

    if (*p == '\0' || *p == '?') {
        upd_reply_bt_stream_state();
        return;
    }
    if (*p == '0' || *p == 'F' || *p == 'f' ||
        ((*p == 'O' || *p == 'o') && (p[1] == 'F' || p[1] == 'f'))) {
        g_upd_bt_stream_enabled = false;
        upd_reply_bt_stream_state();
        (void)bluetooth_wait_tx_complete(1000000u);
        bluetooth_init_uart(clock_apb1_hz(), 115200u, false);
        return;
    }
    if (*p == '1' || *p == 'N' || *p == 'n' ||
        ((*p == 'O' || *p == 'o') && (p[1] == 'N' || p[1] == 'n'))) {
        uart_update_set_bt_stream_enabled(true);
        upd_reply_bt_stream_state();
        (void)bluetooth_wait_tx_complete(1000000u);
        bluetooth_init_uart(clock_apb1_hz(), 19200u, true);
        return;
    }

    upd_reply_err("BT");
}

static void upd_cmd_cap_timeout(const char *args) {
    const char *p = args;

    while (*p == ' ') {
        ++p;
    }

    bool enabled = true;
    if (*p == '0' || *p == 'F' || *p == 'f' ||
        ((*p == 'O' || *p == 'o') && (p[1] == 'F' || p[1] == 'f'))) {
        enabled = false;
    } else if (*p == '1' || *p == 'N' || *p == 'n' ||
               ((*p == 'O' || *p == 'o') && (p[1] == 'N' || p[1] == 'n'))) {
        enabled = true;
    } else {
        upd_reply_err("CAPTO");
        return;
    }

    dmm_p10_set_cap_timeout_enabled(enabled);
    upd_send_text("UCAPTO ");
    upd_send_char(enabled ? '1' : '0');
    upd_send_text("\r\n");
}

static void upd_cmd_ohms_guard(const char *args) {
    const char *p = args;

    while (*p == ' ') {
        ++p;
    }

    if (*p == '\0' || *p == '?') {
        upd_send_text("UOHMG stock=");
        upd_send_char(dmm_p10_ohms_stock_guard_enabled() ? '1' : '0');
        upd_send_text("\r\n");
        return;
    }

    bool stock = true;
    if (*p == '0' || *p == 'L' || *p == 'l') {
        stock = false;
    } else if (*p == '1' || *p == 'S' || *p == 's') {
        stock = true;
    } else {
        upd_reply_err("OHMG");
        return;
    }

    dmm_p10_set_ohms_stock_guard(stock);
    upd_send_text("UOHMG stock=");
    upd_send_char(stock ? '1' : '0');
    upd_send_text("\r\n");
}

static void upd_cmd_read(const char *args) {
    uint32_t addr = 0;
    uint32_t len = 0;
    const char *p = args;

    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &addr) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &len)) {
        upd_reply_err("READ");
        return;
    }
    if (len == 0u || len > UPD_READ_MAX || addr < UPD_FLASH_READ_MIN ||
        addr > UPD_FLASH_READ_MAX || len > (UPD_FLASH_READ_MAX - addr)) {
        upd_reply_err("RANGE");
        return;
    }

    uint32_t done = 0;
    while (done < len) {
        uint32_t chunk_len = len - done;
        if (chunk_len > UPD_READ_REPLY_MAX) {
            chunk_len = UPD_READ_REPLY_MAX;
        }

        upd_send_text("URD ");
        upd_send_hex32(addr + done);
        upd_send_char(' ');
        upd_send_hex32(chunk_len);
        upd_send_char(' ');
        for (uint32_t i = 0; i < chunk_len; ++i) {
            uint8_t byte = 0;
            if (upd_try_read8(addr + done + i, &byte) != 0u) {
                upd_send_text("UERR FAULT ");
                upd_send_hex32(addr + done + i);
                upd_send_text("\r\n");
                return;
            }
            upd_send_hex8(byte);
        }
        upd_send_text("\r\n");
        done += chunk_len;
        if (done < len) {
            delay_ms(20u);
        }
    }
}

static void upd_cmd_mem_read(const char *args) {
    uint32_t addr = 0;
    uint32_t len = 0;
    const char *p = args;

    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &addr) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &len)) {
        upd_reply_err("MEM");
        return;
    }
    if (len == 0u || len > UPD_READ_MAX || addr < UPD_RAM_MIN ||
        addr > UPD_RAM_MAX || len > (UPD_RAM_MAX - addr)) {
        upd_reply_err("RANGE");
        return;
    }

    uint32_t done = 0;
    while (done < len) {
        uint32_t chunk_len = len - done;
        if (chunk_len > UPD_READ_REPLY_MAX) {
            chunk_len = UPD_READ_REPLY_MAX;
        }

        upd_send_text("UMEM ");
        upd_send_hex32(addr + done);
        upd_send_char(' ');
        upd_send_hex32(chunk_len);
        upd_send_char(' ');
        for (uint32_t i = 0; i < chunk_len; ++i) {
            uint8_t byte = 0;
            if (upd_try_read8(addr + done + i, &byte) != 0u) {
                upd_send_text("UERR FAULT ");
                upd_send_hex32(addr + done + i);
                upd_send_text("\r\n");
                return;
            }
            upd_send_hex8(byte);
        }
        upd_send_text("\r\n");
        done += chunk_len;
    }
}

static void upd_cmd_cal_read(const char *args) {
    uint32_t offset = 0;
    uint32_t len = 0;
    const char *p = args;

    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &offset) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &len) ||
        len == 0u || len > UPD_READ_MAX || offset >= CAL_EXPORT_SIZE ||
        len > (CAL_EXPORT_SIZE - offset)) {
        upd_reply_err("CALR");
        return;
    }

    upd_send_text("UCAL ");
    upd_send_hex32(offset);
    upd_send_char(' ');
    upd_send_hex32(len);
    upd_send_char(' ');
    for (uint32_t i = 0; i < len; ++i) {
        upd_send_hex8(calibration_read_u8(offset + i));
    }
    upd_send_text("\r\n");
}

static void upd_cmd_cal_status(void) {
    upd_send_text("UCALS present=");
    upd_send_hex8(calibration_export_area_present() ? 1u : 0u);
    upd_send_text(" size=");
    upd_send_hex32(CAL_EXPORT_SIZE);
    upd_send_text(" hash=");
    upd_send_hex32(calibration_checksum_export_area());
    upd_send_text("\r\n");
}

static void upd_cmd_cal_write(const char *args) {
    uint32_t offset = 0;
    uint32_t value = 0;
    const char *p = args;

    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &offset) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &value) ||
        offset >= CAL_EXPORT_SIZE || value > 0xFFu) {
        upd_reply_err("CALW");
        return;
    }

    uint8_t before = calibration_read_u8(offset);
    bool ok = before == (uint8_t)value ||
              calibration_write_u8(offset, (uint8_t)value);
    upd_send_text(ok ? "UCALW " : "UERR CALW ");
    upd_send_hex32(offset);
    upd_send_char(' ');
    upd_send_hex8(before);
    upd_send_char('>');
    upd_send_hex8(calibration_read_u8(offset));
    upd_send_text("\r\n");
}

static void upd_cmd_cal_mode_dump(const char *args) {
    uint32_t mode = 0;
    uint32_t range = 0;
    uint32_t flags = 0;
    const char *p = args;

    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &mode) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &range)) {
        upd_reply_err("CALM");
        return;
    }
    if (*p != '\0' && (!upd_expect_space(&p) || !upd_parse_hex32(&p, &flags))) {
        upd_reply_err("CALM");
        return;
    }
    if (mode >= CAL_MODE_COUNT || range > 15u || flags > 0x0Fu) {
        upd_reply_err("CALM");
        return;
    }

    bool measure_alt_path = (flags & 0x01u) != 0u;
    bool voltage_alt_bank = (flags & 0x02u) != 0u;
    bool voltage_alt_subpath = (flags & 0x04u) != 0u;
    bool lowz_alt_path = (flags & 0x08u) != 0u;
    upd_send_text("UCALM m=");
    upd_send_hex8((uint8_t)mode);
    upd_send_text(" r=");
    upd_send_hex8((uint8_t)range);
    upd_send_text(" fl=");
    upd_send_hex8((uint8_t)flags);
    for (uint8_t field = 0; field < 4u; ++field) {
        uint32_t offset = calibration_stock_value_offset((uint8_t)mode, (uint8_t)range,
                                                         field, measure_alt_path,
                                                         voltage_alt_bank,
                                                         voltage_alt_subpath,
                                                         lowz_alt_path);
        upd_send_text(" f");
        upd_send_hex8(field);
        upd_send_char('@');
        upd_send_hex32(offset);
        upd_send_char('=');
        if (offset <= CAL_EXPORT_SIZE - 4u) {
            upd_send_hex32(calibration_read_u32_be(offset));
        } else {
            upd_send_text("BAD");
        }
    }

    uint32_t threshold = calibration_stock_threshold_offset((uint8_t)mode,
                                                           (uint8_t)range,
                                                           lowz_alt_path);
    upd_send_text(" th@");
    upd_send_hex32(threshold);
    upd_send_char('=');
    if (threshold <= CAL_EXPORT_SIZE - 8u) {
        for (uint8_t i = 0; i < 4u; ++i) {
            if (i != 0u) {
                upd_send_char(',');
            }
            upd_send_hex16(calibration_read_u16_be(threshold + (uint32_t)i * 2u));
        }
    } else {
        upd_send_text("BAD");
    }
    upd_send_text("\r\n");
}

static void upd_cmd_cal_mode_write(const char *args) {
    uint32_t mode = 0;
    uint32_t range = 0;
    uint32_t field = 0;
    uint32_t flags = 0;
    uint32_t value = 0;
    const char *p = args;

    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &mode) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &range) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &field) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &flags) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &value) ||
        mode >= CAL_MODE_COUNT || range > 15u || field > 3u || flags > 0x0Fu) {
        upd_reply_err("CALMW");
        return;
    }

    uint32_t offset = calibration_stock_value_offset((uint8_t)mode, (uint8_t)range,
                                                     (uint8_t)field,
                                                     (flags & 0x01u) != 0u,
                                                     (flags & 0x02u) != 0u,
                                                     (flags & 0x04u) != 0u,
                                                     (flags & 0x08u) != 0u);
    if (offset > CAL_EXPORT_SIZE - 4u) {
        upd_reply_err("CALMW");
        return;
    }

    uint32_t before = calibration_read_u32_be(offset);
    bool ok = before == value || calibration_write_u32_be(offset, value);
    upd_send_text(ok ? "UCALMW " : "UERR CALMW ");
    upd_send_hex32(offset);
    upd_send_char(' ');
    upd_send_hex32(before);
    upd_send_char('>');
    upd_send_hex32(calibration_read_u32_be(offset));
    upd_send_text("\r\n");
}

static bool upd_parse_one_hex8_arg(const char *args, uint8_t *value) {
    uint32_t parsed = 0;
    const char *p = args;
    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &parsed) || parsed > 0xFFu) {
        return false;
    }
    *value = (uint8_t)parsed;
    return true;
}

static void upd_cmd_sim_state(void) {
    upd_send_text("UXIN en=");
    upd_send_char(input_debug_override_enabled() ? '1' : '0');
    upd_send_text(" re=");
    upd_send_char(input_debug_override_rotary_enabled() ? '1' : '0');
    upd_send_text(" rot=");
    upd_send_hex8(input_debug_override_rotary());
    upd_send_text(" je=");
    upd_send_char(input_debug_override_jack_enabled() ? '1' : '0');
    upd_send_text(" jack=");
    upd_send_hex8(input_debug_override_jack());
    upd_send_text(" be=");
    upd_send_char(input_debug_override_buttons_enabled() ? '1' : '0');
    upd_send_text(" btn=");
    upd_send_hex8(input_debug_override_buttons());
    upd_send_text("\r\n");
}

static void upd_cmd_sim_physical_state(void) {
    upd_send_text("UXPHYS rot=");
    upd_send_hex8(input_debug_physical_rotary());
    upd_send_text(" jack=");
    upd_send_hex8(input_debug_physical_jack());
    upd_send_text("\r\n");
}

extern volatile uint8_t g_app_diag_rotary;
extern volatile uint8_t g_app_diag_buttons;
extern volatile uint8_t g_app_diag_p10_mode;
extern volatile uint8_t g_app_diag_p10_range;

static void upd_cmd_app_diag(void) {
    upd_send_text("UAPP rot=");
    upd_send_hex8(g_app_diag_rotary);
    upd_send_text(" btn=");
    upd_send_hex8(g_app_diag_buttons);
    upd_send_text(" mode=");
    upd_send_hex8(g_app_diag_p10_mode);
    upd_send_text(" range=");
    upd_send_hex8(g_app_diag_p10_range);
    upd_send_text(" fODR=");
    upd_send_hex32(REG32(GPIO_ODR(GPIOF_BASE)));
    upd_send_text(" fIDR=");
    upd_send_hex32(REG32(GPIO_IDR(GPIOF_BASE)));
    upd_send_text(" gODR=");
    upd_send_hex32(REG32(GPIO_ODR(GPIOG_BASE)));
    upd_send_text(" gIDR=");
    upd_send_hex32(REG32(GPIO_IDR(GPIOG_BASE)));
    upd_send_text("\r\n");
}

static void upd_cmd_sim_buttons(const char *args) {
    uint8_t value = 0;
    if (!upd_parse_one_hex8_arg(args, &value)) {
        upd_reply_err("SIMBTN");
        return;
    }
    input_debug_override_set_buttons(true, value);
    upd_cmd_sim_state();
}

static void upd_cmd_sim_jack(const char *args) {
    uint8_t value = 0;
    if (!upd_parse_one_hex8_arg(args, &value)) {
        upd_reply_err("SIMJACK");
        return;
    }
    input_debug_override_set_jack(true, value);
    upd_cmd_sim_state();
}

static void upd_cmd_sim_clear(void) {
    UPD_P10_OVERRIDE_MODE = 0xFFu;
    input_debug_override_clear();
    upd_cmd_sim_state();
}

static void upd_cmd_lcd_dump(void) {
    upd_send_text("ULCD");
    for (uint8_t i = 0; i < 16u; ++i) {
        upd_send_char(' ');
        upd_send_hex32(lcd_debug_shadow_word(i));
    }
    upd_send_text("\r\n");
}

static void upd_cmd_lcd_ram_dump(void) {
    upd_send_text("ULCDR");
    for (uint8_t i = 0; i < 16u; ++i) {
        upd_send_char(' ');
        upd_send_hex32(REG32(LCD_RAM_BASE + (uint32_t)i * 4u));
    }
    upd_send_text("\r\n");
}

static void upd_send_lcdtest_state(void) {
    upd_send_text("ULCDT en=");
    upd_send_hex8(UPD_LCDTEST_ENABLED ? 1u : 0u);
    upd_send_text(" idx=");
    upd_send_hex16(uart_update_lcdtest_index());
    upd_send_text(" per=");
    upd_send_hex16(uart_update_lcdtest_period_ms());
    upd_send_text(" max=01FF\r\n");
}

static void upd_cmd_lcdtest(const char *args) {
    const char *p = args;
    uint32_t op = 0;
    uint32_t value = 0;
    if (*p == '\0') {
        upd_send_lcdtest_state();
        return;
    }
    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &op) || op > 5u) {
        upd_reply_err("LCDT");
        return;
    }
    if (op == 0u) {
        UPD_LCDTEST_ENABLED = 0;
        uart_update_lcdtest_set_period(0);
    } else if (op == 1u) {
        UPD_LCDTEST_ENABLED = 1;
        uart_update_lcdtest_set_period(0);
    } else if (op == 2u) {
        UPD_LCDTEST_ENABLED = 1;
        uart_update_lcdtest_set_period(0);
        uart_update_lcdtest_advance();
    } else if (op == 3u) {
        UPD_LCDTEST_ENABLED = 1;
        uart_update_lcdtest_set_period(0);
        uint16_t index = uart_update_lcdtest_index();
        uart_update_lcdtest_set_index(index == 0u ? 511u : (uint16_t)(index - 1u));
    } else {
        if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &value) ||
            (op == 4u && value > 511u) || (op == 5u && value > 60000u)) {
            upd_reply_err("LCDT");
            return;
        }
        UPD_LCDTEST_ENABLED = 1;
        if (op == 4u) {
            uart_update_lcdtest_set_period(0);
            uart_update_lcdtest_set_index((uint16_t)value);
        } else {
            uart_update_lcdtest_set_period((uint16_t)value);
        }
    }
    upd_send_lcdtest_state();
}

static void upd_send_scap_diag(char tag, const dmm_p10_diag_t *diag) {
    upd_send_text("SCAP ");
    upd_send_char(tag);
    upd_send_text(" m=");
    upd_send_hex8(diag->mode);
    upd_send_text(" r=");
    upd_send_hex8(diag->range);
    upd_send_text(" st=");
    upd_send_hex8(diag->status);
    upd_send_text(" r1e=");
    upd_send_hex8(diag->reg1e);
    upd_send_text(" r1f=");
    upd_send_hex8(diag->reg1f);
    upd_send_text(" r20=");
    upd_send_hex8(diag->reg20);
    upd_send_text(" r29=");
    upd_send_hex8(diag->reg29);
    upd_send_text(" cnt=");
    upd_send_hex32(diag->cap_count);
    upd_send_text(" ref=");
    upd_send_hex32(diag->cap_ref);
    upd_send_text(" wrap=");
    upd_send_hex32(diag->cap_wrap);
    upd_send_text(" el=");
    upd_send_hex32(diag->cap_elapsed);
    upd_send_text(" tw=");
    upd_send_hex16(diag->cap_timing_word);
    upd_send_text(" raw=");
    upd_send_hex32(diag->cap_raw);
    upd_send_text(" val=");
    upd_send_hex32((uint32_t)diag->display_value);
    upd_send_text(" zero=");
    upd_send_hex32(diag->zero_offset);
    upd_send_text(" gain=");
    upd_send_hex32(diag->gain);
    upd_send_text(" ticks=");
    upd_send_hex16(diag->cap_timeout_ticks);
    upd_send_text(" tc=");
    upd_send_hex16(diag->cap_timeout_counter);
    upd_send_text(" ts=");
    upd_send_hex8(diag->cap_timing_state);
    upd_send_text(" fl=");
    upd_send_hex8(diag->stock_flags);
    upd_send_text(" h=");
    upd_send_hex8(diag->cap_holdoff);
    upd_send_lcd_shadow_hex();
    upd_send_text("\r\n");
}

static void upd_cmd_cap_dump(void) {
    dmm_p10_diag_t diag;
    dmm_p10_get_diag(&diag);
    upd_send_scap_diag('D', &diag);
}

static void upd_cmd_p10_diag(void) {
    dmm_p10_diag_t diag;
    dmm_p10_get_diag(&diag);

    upd_send_text("UP10D mode=");
    upd_send_hex8(diag.mode);
    upd_send_text(" range=");
    upd_send_hex8(diag.range);
    upd_send_text(" status=");
    upd_send_hex8(diag.status);
    upd_send_text(" r1e=");
    upd_send_hex8(diag.reg1e);
    upd_send_text(" r1f=");
    upd_send_hex8(diag.reg1f);
    upd_send_text(" r20=");
    upd_send_hex8(diag.reg20);
    upd_send_text(" r29=");
    upd_send_hex8(diag.reg29);
    upd_send_text(" flags=");
    upd_send_hex8(diag.flags);
    upd_send_text(" sfl=");
    upd_send_hex8(diag.substate_flags);
    upd_send_text(" sf=");
    upd_send_hex8(diag.substate_flags);
    upd_send_text(" cap_ev=");
    upd_send_hex8(diag.cap_events);
    upd_send_text(" cap_to=");
    upd_send_hex8(diag.cap_timeouts);
    upd_send_text(" cap_count=");
    upd_send_hex32(diag.cap_count);
    upd_send_text(" cap_ref=");
    upd_send_hex32(diag.cap_ref);
    upd_send_text(" cap_wrap=");
    upd_send_hex32(diag.cap_wrap);
    upd_send_text(" cap_el=");
    upd_send_hex32(diag.cap_elapsed);
    upd_send_text(" tw=");
    upd_send_hex16(diag.frontend_timing_word);
    upd_send_text(" cap_tw=");
    upd_send_hex16(diag.cap_timing_word);
    upd_send_text(" ticks=");
    upd_send_hex16(diag.cap_timeout_ticks);
    upd_send_text(" tc=");
    upd_send_hex16(diag.cap_timeout_counter);
    upd_send_text(" ts=");
    upd_send_hex8(diag.cap_timing_state);
    upd_send_text(" h=");
    upd_send_hex8(diag.cap_holdoff);
    upd_send_text(" cap_raw=");
    upd_send_hex32(diag.cap_raw);
    upd_send_text(" zero=");
    upd_send_hex32(diag.zero_offset);
    upd_send_text(" gain=");
    upd_send_hex32(diag.gain);
    upd_send_text(" scale=");
    upd_send_hex32(diag.gain_scale);
    upd_send_text(" fref=");
    upd_send_hex32(dmm_p10_freq_reference_hz());
    upd_send_text(" fraw=");
    upd_send_hex32(diag.frequency_secondary_raw);
    upd_send_text(" fsec=");
    upd_send_hex32((uint32_t)diag.frequency_secondary_value);
    upd_send_text(" fr=");
    upd_send_hex8(diag.frequency_secondary_range);
    upd_send_text(" ff=");
    upd_send_hex8(diag.frequency_secondary_flags);
    upd_send_text(" raw=");
    upd_send_hex32((uint32_t)diag.raw_sample);
    upd_send_text(" avg=");
    upd_send_hex32((uint32_t)diag.avg_sample);
    upd_send_text(" inst=");
    upd_send_hex32((uint32_t)diag.instant_value);
    upd_send_text(" disp=");
    upd_send_hex32((uint32_t)diag.display_value);
    upd_send_text(" val=");
    upd_send_hex32((uint32_t)diag.last_value);
    if (diag.mode == DMM_MODE_TEMP) {
        bool fahrenheit = upd_temperature_fahrenheit_enabled();
        upd_send_text(" tval=");
        upd_send_hex32((uint32_t)upd_temperature_display_tenths(diag.last_value, fahrenheit));
        upd_send_text(" tu=");
        upd_send_text(fahrenheit ? "F" : "C");
        upd_send_text(" cj=");
        upd_send_hex32((uint32_t)diag.temp_cold_junction_emf);
        upd_send_text(" temf=");
        upd_send_hex32((uint32_t)diag.temp_typek_input_emf);
    }
    upd_send_text(" av=");
    upd_send_hex32((uint32_t)diag.autorange_value);
    upd_send_text(" aa=");
    upd_send_hex32(diag.autorange_abs);
    upd_send_text(" alo=");
    upd_send_hex32(diag.autorange_low);
    upd_send_text(" ahi=");
    upd_send_hex32(diag.autorange_high);
    upd_send_text(" af=");
    upd_send_hex8(diag.autorange_from);
    upd_send_text(" at=");
    upd_send_hex8(diag.autorange_to);
    upd_send_text(" ax=");
    upd_send_hex8(diag.autorange_decision);
    upd_send_text(" dc=");
    upd_send_hex8(diag.display_count);
    upd_send_text(" ss=");
    upd_send_hex8(diag.settle_samples);
    upd_send_text(" ad=");
    upd_send_hex8((uint8_t)diag.autorange_direction);
    upd_send_text(" ah=");
    upd_send_hex8(diag.autorange_holdoff);
    upd_send_text(" cfl=");
    upd_send_hex8(diag.stock_flags);
    upd_send_text(" db=");
    upd_send_hex8(dmm_p10_current_dual_bank_active() ? 1u : 0u);
    upd_send_text(" c6=");
    upd_send_hex8(dmm_p10_current_alt_range0());
    upd_send_text(" c7=");
    upd_send_hex8(dmm_p10_current_alt_range1());
    upd_send_text(" b0=");
    upd_send_hex32((uint32_t)dmm_p10_current_dual_bank0_value());
    upd_send_text(" b1=");
    upd_send_hex32((uint32_t)dmm_p10_current_dual_bank1_value());
    upd_send_text("\r\n");
}

static void upd_cmd_p10_short_diag(void) {
    dmm_p10_diag_t diag;
    dmm_p10_get_diag(&diag);

    upd_send_text("UP10S m=");
    upd_send_hex8(diag.mode);
    upd_send_text(" r=");
    upd_send_hex8(diag.range);
    upd_send_text(" st=");
    upd_send_hex8(diag.status);
    upd_send_text(" r1f=");
    upd_send_hex8(diag.reg1f);
    upd_send_text(" r20=");
    upd_send_hex8(diag.reg20);
    upd_send_text(" tw=");
    upd_send_hex16(diag.frontend_timing_word);
    upd_send_text(" sf=");
    upd_send_hex8(diag.substate_flags);
    upd_send_text(" val=");
    upd_send_hex32((uint32_t)diag.last_value);
    upd_send_text(" inst=");
    upd_send_hex32((uint32_t)diag.instant_value);
    upd_send_text(" disp=");
    upd_send_hex32((uint32_t)diag.display_value);
    if (diag.mode == DMM_MODE_TEMP) {
        bool fahrenheit = upd_temperature_fahrenheit_enabled();
        upd_send_text(" tval=");
        upd_send_hex32((uint32_t)upd_temperature_display_tenths(diag.last_value, fahrenheit));
        upd_send_text(" tu=");
        upd_send_text(fahrenheit ? "F" : "C");
        upd_send_text(" cj=");
        upd_send_hex32((uint32_t)diag.temp_cold_junction_emf);
        upd_send_text(" temf=");
        upd_send_hex32((uint32_t)diag.temp_typek_input_emf);
    }
    upd_send_text(" lcd=");
    for (uint8_t i = 0; i < 4u; ++i) {
        if (i != 0u) {
            upd_send_char(',');
        }
        upd_send_hex32(lcd_debug_shadow_word(i));
    }
    upd_send_gpio_odr_snapshot();
    upd_send_text("\r\n");
}

static void upd_cmd_p10_poll_once(void) {
    upd_reply_err("POLL_DISABLED");
    upd_cmd_p10_diag();
}

static void upd_cmd_ohms_diag_short(void) {
    dmm_p10_diag_t diag;
    dmm_p10_get_diag(&diag);

    upd_send_text("UOHMD m=");
    upd_send_hex8(diag.mode);
    upd_send_text(" r=");
    upd_send_hex8(diag.range);
    upd_send_text(" st=");
    upd_send_hex8(diag.status);
    upd_send_text(" raw=");
    upd_send_hex32((uint32_t)diag.raw_sample);
    upd_send_text(" inst=");
    upd_send_hex32((uint32_t)diag.instant_value);
    upd_send_text(" disp=");
    upd_send_hex32((uint32_t)diag.display_value);
    upd_send_text(" z=");
    upd_send_hex32(diag.zero_offset);
    upd_send_text(" g=");
    upd_send_hex32(diag.gain);
    upd_send_text(" sc=");
    upd_send_hex32(diag.gain_scale);
    upd_send_text(" ax=");
    upd_send_hex8(diag.autorange_decision);
    upd_send_text(" af=");
    upd_send_hex8(diag.autorange_from);
    upd_send_text(" at=");
    upd_send_hex8(diag.autorange_to);
    upd_send_text(" aa=");
    upd_send_hex32(diag.autorange_abs);
    upd_send_text(" lo=");
    upd_send_hex32(diag.autorange_low);
    upd_send_text(" hi=");
    upd_send_hex32(diag.autorange_high);
    upd_send_text(" dc=");
    upd_send_hex8(diag.display_count);
    upd_send_text(" ah=");
    upd_send_hex8(diag.autorange_holdoff);
    upd_send_text("\r\n");
}

static void upd_cmd_p10_autorange(const char *args) {
    const char *p = args;

    while (*p == ' ') {
        ++p;
    }

    if (*p == '\0' || *p == '?') {
        upd_send_text("UAUTO ");
        upd_send_char(dmm_p10_autorange_enabled() ? '1' : '0');
        upd_send_text("\r\n");
        return;
    }

    bool enabled = true;
    if (*p == '0' || *p == 'F' || *p == 'f' ||
        ((*p == 'O' || *p == 'o') && (p[1] == 'F' || p[1] == 'f'))) {
        enabled = false;
    } else if (*p == '1' || *p == 'N' || *p == 'n' ||
               ((*p == 'O' || *p == 'o') && (p[1] == 'N' || p[1] == 'n'))) {
        enabled = true;
    } else {
        upd_reply_err("AUTO");
        return;
    }

    dmm_p10_set_autorange(enabled);
    upd_send_text("UAUTO ");
    upd_send_char(enabled ? '1' : '0');
    upd_send_text("\r\n");
}

static void upd_cmd_frontend_raw(void) {
    uint8_t regs[FRONTEND_STATUS_SIZE];
    bool ready_before = frontend_ready();
    frontend_read_registers(0x00u, regs, (uint8_t)sizeof(regs));
    bool ready_after = frontend_ready();

    upd_send_text("UFE ready=");
    upd_send_hex8(ready_before ? 1u : 0u);
    upd_send_char('/');
    upd_send_hex8(ready_after ? 1u : 0u);
    upd_send_text(" r1e=");
    upd_send_hex8(regs[0x1Eu]);
    upd_send_text(" r1f=");
    upd_send_hex8(regs[0x1Fu]);
    upd_send_text(" r20=");
    upd_send_hex8(regs[0x20u]);
    upd_send_text(" r29=");
    upd_send_hex8(regs[0x29u]);
    upd_send_text(" s=");
    for (uint8_t i = 0; i < 6u; ++i) {
        upd_send_hex8(regs[i]);
    }
    upd_send_text(" cap=");
    for (uint8_t i = 0x15u; i <= 0x1Du; ++i) {
        upd_send_hex8(regs[i]);
    }
    upd_send_text("\r\n");
}

static void upd_cmd_frontend_config(void) {
    uint8_t regs[20];
    uint8_t ctrl[2];

    frontend_read_registers(0x20u, regs, (uint8_t)sizeof(regs));
    frontend_read_registers(0x1Eu, ctrl, (uint8_t)sizeof(ctrl));

    upd_send_text("UFECFG ctrl=");
    upd_send_hex8(ctrl[0]);
    upd_send_char('/');
    upd_send_hex8(ctrl[1]);
    upd_send_text(" cfg=");
    for (uint8_t i = 0; i < (uint8_t)sizeof(regs); ++i) {
        upd_send_hex8(regs[i]);
    }
    upd_send_text("\r\n");
}

static void upd_cmd_frontend_wait(const char *args) {
    const char *p = args;
    uint32_t duration_ms = 500u;
    uint8_t first[FRONTEND_STATUS_SIZE];
    uint8_t last[FRONTEND_STATUS_SIZE];
    uint32_t loops = 0;
    uint32_t ready_count = 0;
    uint32_t read_count = 0;
    uint32_t bit0_count = 0;
    uint32_t bit2_count = 0;
    bool have_first = false;

    if (*p != '\0') {
        if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &duration_ms)) {
            upd_reply_err("FEWAIT");
            return;
        }
    }
    if (duration_ms > 5000u) {
        duration_ms = 5000u;
    }
    for (uint8_t i = 0; i < (uint8_t)sizeof(first); ++i) {
        first[i] = 0;
        last[i] = 0;
    }

    uint32_t start = clock_millis();
    while ((uint32_t)(clock_millis() - start) < duration_ms) {
        ++loops;
        if (!frontend_ready()) {
            continue;
        }

        ++ready_count;
        frontend_read_status(last);
        ++read_count;
        uint8_t status = (uint8_t)(last[0x1Eu] & last[0x1Fu]);
        if ((status & 0x01u) != 0u) {
            ++bit0_count;
        }
        if ((status & 0x04u) != 0u) {
            ++bit2_count;
        }
        if (!have_first) {
            for (uint8_t i = 0; i < (uint8_t)sizeof(first); ++i) {
                first[i] = last[i];
            }
            have_first = true;
        }
    }

    upd_send_text("UFEWAIT loops=");
    upd_send_hex32(loops);
    upd_send_text(" ready=");
    upd_send_hex32(ready_count);
    upd_send_text(" reads=");
    upd_send_hex32(read_count);
    upd_send_text(" b0=");
    upd_send_hex32(bit0_count);
    upd_send_text(" b2=");
    upd_send_hex32(bit2_count);
    upd_send_text(" first=");
    upd_send_hex8(first[0x1Eu]);
    upd_send_char('/');
    upd_send_hex8(first[0x1Fu]);
    upd_send_char('/');
    upd_send_hex8(first[0x20u]);
    upd_send_text(" last=");
    upd_send_hex8(last[0x1Eu]);
    upd_send_char('/');
    upd_send_hex8(last[0x1Fu]);
    upd_send_char('/');
    upd_send_hex8(last[0x20u]);
    upd_send_text(" cap=");
    for (uint8_t i = 0x15u; i <= 0x1Du; ++i) {
        upd_send_hex8(last[i]);
    }
    upd_send_text("\r\n");
}

static void upd_cmd_gpio_diag(void) {
    static const uint32_t ports[] = {
        GPIOA_BASE, GPIOB_BASE, GPIOC_BASE, GPIOD_BASE, GPIOE_BASE, GPIOF_BASE, GPIOG_BASE, GPIOH_BASE,
    };

    upd_send_text("UGPIO");
    for (uint8_t i = 0; i < (uint8_t)(sizeof(ports) / sizeof(ports[0])); ++i) {
        upd_send_char(' ');
        upd_send_char((char)('A' + i));
        upd_send_text("i=");
        upd_send_hex32(REG32(GPIO_IDR(ports[i])));
        upd_send_text("o=");
        upd_send_hex32(REG32(GPIO_ODR(ports[i])));
    }
    upd_send_text("\r\n");
}

static void upd_cmd_gpio_reg_diag(void) {
    static const uint32_t ports[] = {
        GPIOA_BASE, GPIOB_BASE, GPIOC_BASE, GPIOD_BASE, GPIOE_BASE, GPIOF_BASE, GPIOG_BASE, GPIOH_BASE,
    };

    upd_send_text("UGPIOX");
    for (uint8_t i = 0; i < (uint8_t)(sizeof(ports) / sizeof(ports[0])); ++i) {
        upd_send_char(' ');
        upd_send_char((char)('A' + i));
        upd_send_text("m=");
        upd_send_hex32(REG32(GPIO_MODER(ports[i])));
        upd_send_text("s=");
        upd_send_hex32(REG32(GPIO_OSPEEDR(ports[i])));
        upd_send_text("p=");
        upd_send_hex32(REG32(GPIO_PUPDR(ports[i])));
        upd_send_text("t=");
        upd_send_hex32(REG32(GPIO_OTYPER(ports[i])));
    }
    upd_send_text("\r\n");
}

static uint32_t upd_route_port_base(char port) {
    if (port >= 'a' && port <= 'z') {
        port = (char)(port - 'a' + 'A');
    }
    switch (port) {
    case 'A':
        return GPIOA_BASE;
    case 'B':
        return GPIOB_BASE;
    case 'C':
        return GPIOC_BASE;
    case 'D':
        return GPIOD_BASE;
    case 'E':
        return GPIOE_BASE;
    case 'F':
        return GPIOF_BASE;
    case 'G':
        return GPIOG_BASE;
    default:
        return 0;
    }
}

static bool upd_route_pin_allowed(char port, uint8_t pin) {
    if (port >= 'a' && port <= 'z') {
        port = (char)(port - 'a' + 'A');
    }
    if (port == 'A') {
        return pin == 11u || pin == 12u;
    }
    if (port == 'B') {
        return pin == 6u || pin == 7u;
    }
    if (port == 'C') {
        return pin == 6u || pin == 7u || pin == 11u || pin == 13u;
    }
    if (port == 'D') {
        return pin == 0u || pin == 1u || pin == 7u;
    }
    if (port == 'E') {
        return pin == 5u || pin == 7u || pin == 8u || pin == 9u ||
               pin == 12u || pin == 13u || pin == 15u;
    }
    if (port == 'F') {
        return pin == 2u;
    }
    if (port == 'G') {
        return pin == 4u || pin == 5u || pin == 6u || pin == 15u;
    }
    return false;
}

static void upd_cmd_route_pin_set(const char *args) {
    const char *p = args;
    uint32_t pin = 0;
    uint32_t value = 0;

    if (!upd_expect_space(&p) || *p == '\0') {
        upd_reply_err("RPIN");
        return;
    }
    char port = *p++;
    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &pin) || pin > 15u ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &value) || value > 1u) {
        upd_reply_err("RPIN");
        return;
    }

    uint32_t base = upd_route_port_base(port);
    if (base == 0u || !upd_route_pin_allowed(port, (uint8_t)pin)) {
        upd_reply_err("RPINSAFE");
        return;
    }

    gpio_config_pin(base, (uint8_t)pin, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_write(base, (uint8_t)pin, value != 0u);
    upd_send_text("URPIN ");
    upd_send_char(port);
    upd_send_char(' ');
    upd_send_hex8((uint8_t)pin);
    upd_send_char('=');
    upd_send_hex8((uint8_t)value);
    upd_send_text("\r\n");
}

static void upd_cmd_frontend_dump(void) {
    uint8_t regs[FRONTEND_STATUS_SIZE];
    bool ready_before = frontend_ready();
    frontend_read_registers(0x00u, regs, (uint8_t)sizeof(regs));
    bool ready_after = frontend_ready();

    upd_send_text("UFEDUMP ready=");
    upd_send_hex8(ready_before ? 1u : 0u);
    upd_send_char('/');
    upd_send_hex8(ready_after ? 1u : 0u);
    upd_send_text(" regs=");
    for (uint8_t i = 0; i < (uint8_t)sizeof(regs); ++i) {
        upd_send_hex8(regs[i]);
    }
    upd_send_text("\r\n");
}

static void upd_cmd_p10_set(const char *args) {
    uint32_t mode = 0;
    uint32_t range = 0;
    const char *p = args;
    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &mode) || mode > 0xFFu ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &range) || range > 0xFFu) {
        upd_reply_err("P10SET");
        return;
    }
    UPD_P10_OVERRIDE_MODE = (uint8_t)mode;
    UPD_P10_OVERRIDE_RANGE = (uint8_t)range;
    ++UPD_P10_OVERRIDE_COOKIE;
    upd_reply_ok();
    upd_cmd_p10_diag();
}

static void upd_cmd_frontend_restart(const char *args) {
    uint32_t timing = 0;
    const char *p = args;

    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &timing) || timing > 0xFFFFu) {
        upd_reply_err("FERST");
        return;
    }

    frontend_restart_p10_conversion((uint16_t)timing);
    upd_cmd_frontend_raw();
}

static void upd_cmd_begin(const char *args) {
    uint32_t size = 0;
    uint32_t crc = 0;
    const char *p = args;

    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &size) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &crc)) {
        upd_reply_err("BEGIN");
        return;
    }
    if (size == 0u || size > UPD_APP_MAX_SIZE) {
        upd_reply_err("SIZE");
        return;
    }

    upd_reset_transfer();
    g_upd.receiving = true;
    g_upd.expected_size = size;
    g_upd.expected_crc = crc;
    upd_reply_ok();
}

static void upd_cmd_stream_begin(const char *args, bool flash_mode) {
    uint32_t size = 0;
    uint32_t crc = 0;
    uint32_t data_baud = 19200u;
    const char *p = args;

    if (!upd_expect_space(&p) || !upd_parse_hex32(&p, &size) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &crc)) {
        upd_reply_err("BEGIN");
        return;
    }
    if (size == 0u || size > UPD_APP_MAX_SIZE || (size & 3u) != 0u) {
        upd_reply_err("SIZE");
        return;
    }
    while (*p == ' ') {
        ++p;
    }
    if (*p != '\0') {
        if (!upd_parse_hex32(&p, &data_baud) || !upd_baud_supported(data_baud)) {
            upd_reply_err("BAUD");
            return;
        }
    }
    (void)bluetooth_wait_tx_complete(1000000u);
    __asm__ volatile("cpsid i\n dsb\n isb" ::: "memory");
    upd_stream_session(flash_mode, size, crc, data_baud);
}

static void upd_cmd_data(const char *args) {
    const char *p = args;
    bool ok = true;
    uint8_t chunk_len = 0;

    if (!g_upd.receiving) {
        upd_reply_err("IDLE");
        return;
    }
    if (!upd_expect_space(&p)) {
        upd_reply_err("DATA");
        return;
    }

    while (*p != '\0') {
        if (p[0] == ' ') {
            ++p;
            continue;
        }
        if (p[1] == '\0') {
            ok = false;
            break;
        }
        uint8_t byte = (uint8_t)((upd_hex_value(p[0], &ok) << 4) | upd_hex_value(p[1], &ok));
        if (!ok) {
            break;
        }
        if (g_upd.received_size >= g_upd.expected_size) {
            g_upd.overflow = true;
            ok = false;
            break;
        }
        if (g_upd.first_count < (uint8_t)sizeof(g_upd.first_bytes)) {
            g_upd.first_bytes[g_upd.first_count++] = byte;
        }
        g_upd.crc = upd_crc32_byte(g_upd.crc, byte);
        ++g_upd.received_size;
        ++chunk_len;
        p += 2;
    }

    if (!ok || chunk_len == 0u) {
        upd_reply_err(g_upd.overflow ? "OVERFLOW" : "HEX");
        return;
    }

    upd_send_text("UACK ");
    upd_send_hex32(g_upd.received_size);
    upd_send_text("\r\n");
}

static void upd_cmd_end(void) {
    uint32_t final_crc = g_upd.crc ^ 0xFFFFFFFFu;
    if (!g_upd.receiving) {
        upd_reply_err("IDLE");
        return;
    }
    if (g_upd.received_size != g_upd.expected_size) {
        upd_reply_err("COUNT");
        return;
    }
    if (final_crc != g_upd.expected_crc) {
        upd_reply_err("CRC");
        return;
    }
    if (!upd_image_header_plausible()) {
        upd_reply_err("VECTOR");
        return;
    }

    g_upd.receiving = false;
    g_upd.complete = true;
    upd_send_text("UOK SAFE_NOFLASH crc=");
    upd_send_hex32(final_crc);
    upd_send_text("\r\n");
}

static void upd_cmd_flash_sim(void) {
    upd_reply_err("USE_USIM");
}

static void upd_cmd_flash(void) {
    upd_reply_err("USE_UFBEGIN");
}

static void upd_cmd_sd_test(void) {
    sd_fat_result_t rc = sd_fat_append_eevlog("EEVblog121GW,sd-write-test\r\n");
    if (rc == SD_FAT_OK) {
        upd_send_text("USDOK EEVLOG.CSV\r\n");
    } else {
        upd_send_text("USDERR ");
        upd_send_hex8((uint8_t)rc);
        upd_send_text(" card=");
        upd_send_hex8(sd_fat_last_card_error());
        upd_send_text("\r\n");
    }
}

static void upd_cmd_sd_read(const char *args) {
    char name83[11];
    uint32_t offset = 0;
    uint32_t len = 0;
    const char *p = args;
    if (!upd_parse_name83(&p, name83) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &offset) ||
        !upd_expect_space(&p) || !upd_parse_hex32(&p, &len) ||
        len > 128u) {
        upd_reply_err("ARG");
        return;
    }
    uint8_t buf[128];
    uint32_t got = 0;
    uint32_t size = 0;
    sd_fat_result_t rc = sd_fat_read_named_83(name83, offset, buf, len, &got, &size);
    if (rc != SD_FAT_OK) {
        upd_send_text("USDRDERR ");
        upd_send_hex8((uint8_t)rc);
        upd_send_text(" card=");
        upd_send_hex8(sd_fat_last_card_error());
        upd_send_text("\r\n");
        return;
    }
    upd_send_text("USDRD name=");
    upd_send_name83(name83);
    upd_send_text(" size=");
    upd_send_hex32(size);
    upd_send_text(" off=");
    upd_send_hex32(offset);
    upd_send_text(" len=");
    upd_send_hex32(got);
    upd_send_text(" data=");
    for (uint32_t i = 0u; i < got; ++i) {
        upd_send_hex8(buf[i]);
    }
    upd_send_text("\r\n");
}

static void upd_cmd_log_test(void) {
    char name83[11];
    uint8_t rows = 0;
    uint8_t rc = 0;
    bool ok = app_sd_logging_test_all_modes(name83, &rows, &rc);
    if (!ok) {
        upd_send_text("ULOGTSTERR ");
        upd_send_hex8(rc);
        upd_send_text(" card=");
        upd_send_hex8(sd_fat_last_card_error());
        upd_send_text("\r\n");
        return;
    }
    upd_send_text("ULOGTSTOK name=");
    upd_send_name83(name83);
    upd_send_text(" rows=");
    upd_send_hex8(rows);
    upd_send_text("\r\n");
}

static void upd_cmd_sdio_diag(void) {
    upd_send_text("USDIO stg=");
    upd_send_hex8(sdio_sd_debug_stage());
    upd_send_text(" rca=");
    upd_send_hex16(sdio_sd_debug_rca());
    upd_send_text(" blk=");
    upd_send_hex8(sdio_sd_debug_block_addressed() ? 1u : 0u);
    upd_send_text(" wm=");
    upd_send_hex8(sdio_sd_debug_write_mode());
    upd_send_text(" flow=");
    upd_send_hex8(sdio_sd_debug_hw_flow() ? 1u : 0u);
    upd_send_text(" pwr=");
    upd_send_hex32(sdio_sd_debug_power());
    upd_send_text(" clk=");
    upd_send_hex32(sdio_sd_debug_clkcr());
    upd_send_text(" sta=");
    upd_send_hex32(sdio_sd_debug_sta());
    upd_send_text(" cr=");
    upd_send_hex32(sdio_sd_debug_rcc_cr());
    upd_send_text(" cfgr=");
    upd_send_hex32(sdio_sd_debug_rcc_cfgr());
    upd_send_text(" cnt=");
    upd_send_hex32(sdio_sd_debug_data_count());
    upd_send_text(" csta=");
    upd_send_hex32(sdio_sd_debug_cmd_sta());
    upd_send_text(" rcmd=");
    upd_send_hex32(sdio_sd_debug_resp_cmd());
    upd_send_text(" r1=");
    upd_send_hex32(sdio_sd_debug_resp1());
    upd_send_text(" dctl=");
    upd_send_hex32(sdio_sd_debug_dctrl());
    upd_send_text(" dlen=");
    upd_send_hex32(sdio_sd_debug_dlen());
    upd_send_text(" mask=");
    upd_send_hex32(sdio_sd_debug_mask());
    upd_send_text(" dmac=");
    upd_send_hex32(sdio_sd_debug_dma_ccr());
    upd_send_text(" dman=");
    upd_send_hex32(sdio_sd_debug_dma_cndtr());
    upd_send_text(" dmais=");
    upd_send_hex32(sdio_sd_debug_dma_isr());
    upd_send_text(" w13rc=");
    upd_send_hex8(sdio_sd_debug_write_status_rc());
    upd_send_text(" w13=");
    upd_send_hex32(sdio_sd_debug_write_status());
    upd_send_text(" vrc=");
    upd_send_hex8(sdio_sd_debug_verify_rc());
    upd_send_text(" voff=");
    upd_send_hex32(sdio_sd_debug_verify_offset());
    upd_send_text(" vex=");
    upd_send_hex8(sdio_sd_debug_verify_expected());
    upd_send_text(" vac=");
    upd_send_hex8(sdio_sd_debug_verify_actual());
    upd_send_text("\r\n");
}

static void upd_cmd_sdio_status(void) {
    uint32_t card_status = 0;
    sdio_sd_result_t status_rc = sdio_sd_debug_card_status(&card_status);
    upd_send_text("UC13 rc=");
    upd_send_hex8((uint8_t)status_rc);
    upd_send_text(" rca=");
    upd_send_hex16(sdio_sd_debug_rca());
    upd_send_text(" blk=");
    upd_send_hex8(sdio_sd_debug_block_addressed() ? 1u : 0u);
    upd_send_text(" st=");
    upd_send_hex32(card_status);
    upd_send_text("\r\n");
}

static void upd_cmd_sdio_clock_probe(void) {
    sdio_sd_debug_clock_probe();
    upd_cmd_sdio_diag();
}

static void upd_cmd_sdio_div(const char *args) {
    uint32_t div = 0;
    if (!upd_expect_space(&args) || !upd_parse_hex32(&args, &div) || div > 0xFFu) {
        upd_reply_err("ARG");
        return;
    }
    sdio_sd_debug_set_run_div((uint8_t)div);
    upd_send_text("USDIV ");
    upd_send_hex8((uint8_t)div);
    upd_send_text("\r\n");
}

static void upd_cmd_sdio_write_mode(const char *args) {
    uint32_t mode = 0;
    if (!upd_expect_space(&args) || !upd_parse_hex32(&args, &mode) || mode > 2u) {
        upd_reply_err("ARG");
        return;
    }
    sdio_sd_debug_set_write_mode((uint8_t)mode);
    upd_send_text("USDWM ");
    upd_send_hex8((uint8_t)mode);
    upd_send_text("\r\n");
}

static void upd_cmd_sdio_flow(const char *args) {
    uint32_t enabled = 0;
    if (!upd_expect_space(&args) || !upd_parse_hex32(&args, &enabled) || enabled > 1u) {
        upd_reply_err("ARG");
        return;
    }
    sdio_sd_debug_set_hw_flow(enabled != 0u);
    upd_send_text("USDFLOW ");
    upd_send_hex8((uint8_t)enabled);
    upd_send_text("\r\n");
}

static void upd_process_line(void) {
    g_upd.line[g_upd.line_len] = '\0';

    if (g_upd.line_len == 0u) {
        return;
    }
    if (g_upd.line[0] != 'U') {
        return;
    }
    upd_reply_rx_line();

    if ((g_upd.line[1] == '\0') ||
        (g_upd.line[1] == '?' && g_upd.line[2] == '\0') ||
        upd_starts_with(g_upd.line, "UINFO")) {
        upd_cmd_info();
    } else if (upd_starts_with(g_upd.line, "URPIN")) {
        upd_cmd_route_pin_set(&g_upd.line[5]);
    } else if (g_upd.line[1] == 'S' && g_upd.line[2] == '\0') {
        upd_reply_state();
    } else if (g_upd.line[1] == 'R') {
        upd_cmd_read(&g_upd.line[2]);
    } else if (upd_starts_with(g_upd.line, "UMEM")) {
        upd_cmd_mem_read(&g_upd.line[4]);
    } else if (upd_starts_with(g_upd.line, "UCALMW")) {
        upd_cmd_cal_mode_write(&g_upd.line[6]);
    } else if (upd_starts_with(g_upd.line, "UCALM")) {
        upd_cmd_cal_mode_dump(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "UCALS")) {
        upd_cmd_cal_status();
    } else if (upd_starts_with(g_upd.line, "UCALW")) {
        upd_cmd_cal_write(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "UCAL")) {
        upd_cmd_cal_read(&g_upd.line[4]);
    } else if ((g_upd.line[1] == 'A' && g_upd.line[2] == '\0') ||
               upd_starts_with(g_upd.line, "UABORT")) {
        upd_reset_transfer();
        upd_reply_ok();
    } else if (upd_starts_with(g_upd.line, "USIM")) {
        upd_cmd_stream_begin(&g_upd.line[4], false);
    } else if (upd_starts_with(g_upd.line, "UFBEGIN")) {
        upd_cmd_stream_begin(&g_upd.line[7], true);
    } else if (upd_starts_with(g_upd.line, "UBEGIN")) {
        upd_cmd_begin(&g_upd.line[6]);
    } else if (upd_starts_with(g_upd.line, "UBOVR")) {
        upd_cmd_battery_override(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "UBAT")) {
        upd_cmd_battery_diag();
    } else if (upd_starts_with(g_upd.line, "ULOGTST")) {
        upd_cmd_log_test();
    } else if (upd_starts_with(g_upd.line, "USDRD")) {
        upd_cmd_sd_read(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "USDTEST")) {
        upd_cmd_sd_test();
    } else if (upd_starts_with(g_upd.line, "USDCLK")) {
        upd_cmd_sdio_clock_probe();
    } else if (upd_starts_with(g_upd.line, "USDIV")) {
        upd_cmd_sdio_div(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "USDWM")) {
        upd_cmd_sdio_write_mode(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "USDFLOW")) {
        upd_cmd_sdio_flow(&g_upd.line[7]);
    } else if (upd_starts_with(g_upd.line, "USDIO")) {
        upd_cmd_sdio_diag();
    } else if (upd_starts_with(g_upd.line, "UC13")) {
        upd_cmd_sdio_status();
    } else if (upd_starts_with(g_upd.line, "UAPP")) {
        upd_cmd_app_diag();
    } else if (upd_starts_with(g_upd.line, "UBLEPWR")) {
        upd_cmd_ble_power(&g_upd.line[7]);
    } else if (upd_starts_with(g_upd.line, "UBT")) {
        upd_cmd_bt_stream(&g_upd.line[3]);
    } else if (g_upd.line[1] == 'B') {
        upd_cmd_begin(&g_upd.line[2]);
    } else if (upd_starts_with(g_upd.line, "UDEBUG")) {
        upd_cmd_debug(&g_upd.line[6]);
    } else if (upd_starts_with(g_upd.line, "UDBG")) {
        upd_cmd_debug(&g_upd.line[4]);
    } else if (upd_starts_with(g_upd.line, "UCLK")) {
        upd_cmd_clock_diag();
    } else if (upd_starts_with(g_upd.line, "UTOVR")) {
        upd_cmd_temperature_override(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "UTUNIT")) {
        upd_cmd_temperature_unit_override(&g_upd.line[6]);
    } else if (upd_starts_with(g_upd.line, "UTEMP")) {
        upd_cmd_temperature_diag();
    } else if (upd_starts_with(g_upd.line, "UCAPDUMP")) {
        upd_cmd_cap_dump();
    } else if (upd_starts_with(g_upd.line, "UCAPTO")) {
        upd_cmd_cap_timeout(&g_upd.line[6]);
    } else if (upd_starts_with(g_upd.line, "UOHMG")) {
        upd_cmd_ohms_guard(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "UOHMD")) {
        upd_cmd_ohms_diag_short();
    } else if (upd_starts_with(g_upd.line, "UAUTO")) {
        upd_cmd_p10_autorange(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "UXPHYS")) {
        upd_cmd_sim_physical_state();
    } else if (upd_starts_with(g_upd.line, "UXBTN")) {
        upd_cmd_sim_buttons(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "UXJACK")) {
        upd_cmd_sim_jack(&g_upd.line[6]);
    } else if (upd_starts_with(g_upd.line, "UXCLR")) {
        upd_cmd_sim_clear();
    } else if (upd_starts_with(g_upd.line, "UXIN")) {
        upd_cmd_sim_state();
    } else if (upd_starts_with(g_upd.line, "ULCDT")) {
        upd_cmd_lcdtest(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "ULCDR")) {
        upd_cmd_lcd_ram_dump();
    } else if (upd_starts_with(g_upd.line, "ULCD")) {
        upd_cmd_lcd_dump();
    } else if (upd_starts_with(g_upd.line, "UP10SET")) {
        upd_cmd_p10_set(&g_upd.line[7]);
    } else if (upd_starts_with(g_upd.line, "UP10S")) {
        upd_cmd_p10_short_diag();
    } else if (upd_starts_with(g_upd.line, "UP10D")) {
        upd_cmd_p10_diag();
    } else if (upd_starts_with(g_upd.line, "UP10POLL")) {
        upd_cmd_p10_poll_once();
    } else if (upd_starts_with(g_upd.line, "UFECFG")) {
        upd_cmd_frontend_config();
    } else if (upd_starts_with(g_upd.line, "UFEWAIT")) {
        upd_cmd_frontend_wait(&g_upd.line[7]);
    } else if (upd_starts_with(g_upd.line, "UFEDUMP")) {
        upd_cmd_frontend_dump();
    } else if (upd_starts_with(g_upd.line, "UFE")) {
        upd_cmd_frontend_raw();
    } else if (upd_starts_with(g_upd.line, "UFRST")) {
        upd_cmd_frontend_restart(&g_upd.line[5]);
    } else if (upd_starts_with(g_upd.line, "UGPIOX")) {
        upd_cmd_gpio_reg_diag();
    } else if (upd_starts_with(g_upd.line, "UGPIO")) {
        upd_cmd_gpio_diag();
    } else if (upd_starts_with(g_upd.line, "USTOPDBG")) {
        g_upd_debug_enabled = false;
        upd_reply_debug_state();
    } else if (upd_starts_with(g_upd.line, "USTARTDBG")) {
        g_upd_debug_enabled = true;
        upd_reply_debug_state();
    } else if (upd_starts_with(g_upd.line, "UDATA")) {
        upd_cmd_data(&g_upd.line[5]);
    } else if (g_upd.line[1] == 'D') {
        upd_cmd_data(&g_upd.line[2]);
    } else if ((g_upd.line[1] == 'E' && g_upd.line[2] == '\0') ||
               upd_starts_with(g_upd.line, "UEND")) {
        upd_cmd_end();
    } else if (upd_starts_with(g_upd.line, "UFSIM")) {
        upd_cmd_flash_sim();
    } else if ((g_upd.line[1] == 'F' && g_upd.line[2] == '\0') ||
               upd_starts_with(g_upd.line, "UFLASH")) {
        upd_cmd_flash();
    } else if (!g_upd.receiving && g_upd.line_len <= 8u) {
        upd_cmd_info();
    } else {
        upd_reply_err("CMD");
    }
}

void uart_update_init(void) {
    upd_reset_transfer();
    UPD_LCDTEST_ENABLED = 0;
    uart_update_lcdtest_set_index(0);
    uart_update_lcdtest_set_period(0);
    UPD_P10_OVERRIDE_COOKIE = 0u;
    UPD_P10_OVERRIDE_MODE = 0xFFu;
    UPD_P10_OVERRIDE_RANGE = 0u;
    g_upd.line_len = 0;
    g_upd_quiet_until = 0;
    g_upd_debug_enabled = true;
    g_upd_bt_stream_enabled = false;
}

bool uart_update_rx_byte(uint8_t byte) {
    g_upd_quiet_until = clock_millis() + 1000u;

    if (byte == '\r' || byte == '\n') {
        upd_process_line();
        g_upd.line_len = 0;
        return true;
    }
    if (byte < 0x20u || byte > 0x7Eu) {
        if (g_upd.line_len != 0u) {
            g_upd.line_len = 0;
        }
        return false;
    }
    if (g_upd.line_len == 0u && byte != 'U') {
        return false;
    }
    if (g_upd.line_len >= (UPD_LINE_MAX - 1u)) {
        g_upd.line_len = 0;
        upd_reply_err("LINE");
        return true;
    }
    g_upd.line[g_upd.line_len++] = (char)byte;
    return true;
}

bool uart_update_active(void) {
    return g_upd.receiving || (int32_t)(clock_millis() - g_upd_quiet_until) < 0;
}

bool uart_update_debug_allowed(void) {
    return g_upd_debug_enabled && !uart_update_active();
}

bool uart_update_bt_stream_allowed(void) {
    return g_upd_bt_stream_enabled && !uart_update_active();
}

bool uart_update_bt_stream_enabled(void) {
    return g_upd_bt_stream_enabled;
}

void uart_update_set_bt_stream_enabled(bool enabled) {
    g_upd_bt_stream_enabled = enabled;
    if (enabled) {
        g_upd_debug_enabled = false;
        g_upd.line_len = 0;
        g_upd_quiet_until = 0;
    } else {
        g_upd_debug_enabled = true;
        g_upd.line_len = 0;
        g_upd_quiet_until = 0;
    }
}
