#include "lcd.h"

#include "clock.h"
#include "gpio.h"
#include "hw.h"

#define LCD_APB1EN BIT(9)
#define PWR_APB1EN BIT(28)
#define LCD_AF 11u

#define RCC_CSR_LSEON  BIT(0)
#define RCC_CSR_LSERDY BIT(1)
#define RCC_CSR_RTCSEL_MASK (3u << 16)
#define RCC_CSR_RTCSEL_LSE  0x00020000u
#define RCC_CSR_RTCEN  BIT(22)
#define RCC_CSR_RTCRST BIT(23)
#define PWR_CR_DBP     BIT(8)

static uint32_t g_lcd_shadow[16];

typedef struct {
    uint8_t ram;
    uint32_t bit;
} lcd_bar_segment_t;

static const lcd_bar_segment_t k_bar_segments[25] = {
    {2u, 0x00000002u}, {4u, 0x00000002u}, {6u, 0x00000002u},
    {6u, 0x00000004u}, {4u, 0x00000004u}, {2u, 0x00000004u},
    {0u, 0x00000004u}, {0u, 0x00000008u}, {2u, 0x00000008u},
    {4u, 0x00000008u}, {6u, 0x00000008u}, {6u, 0x00000010u},
    {4u, 0x00000010u}, {2u, 0x00000010u}, {0u, 0x00000010u},
    {0u, 0x00000020u}, {2u, 0x00000020u}, {4u, 0x00000020u},
    {6u, 0x00000020u}, {6u, 0x00000040u}, {4u, 0x00000040u},
    {2u, 0x00000040u}, {0u, 0x00000040u}, {0u, 0x00000080u},
    {2u, 0x00000080u},
};

static const uint8_t k_digit_masks[10] = {
    0xBBu, 0x11u, 0xA7u, 0x97u, 0x1Du,
    0x9Eu, 0xBEu, 0x1Bu, 0xBFu, 0x9Fu,
};

static const uint8_t k_upper_masks[26] = {
    0x3Fu, 0xBCu, 0xAAu, 0xB5u, 0xAEu, 0x2Eu, 0x9Fu,
    0x3Du, 0x11u, 0xB1u, 0x2Du, 0xA8u, 0xBFu, 0x3Bu,
    0xBBu, 0x2Fu, 0x1Fu, 0x2Au, 0x9Eu, 0xACu, 0xB9u,
    0xB9u, 0xB9u, 0x1Cu, 0x9Du, 0x86u,
};

static const uint8_t k_lower_masks[26] = {
    0x3Fu, 0xBCu, 0xA4u, 0xB5u, 0xAEu, 0x2Eu, 0x9Fu,
    0x3Cu, 0x10u, 0xB1u, 0x2Du, 0xA8u, 0xBFu, 0x34u,
    0xB4u, 0x2Fu, 0x1Fu, 0x24u, 0x9Eu, 0xACu, 0xB0u,
    0xB0u, 0xB0u, 0x1Cu, 0x9Du, 0x86u,
};

static void lcd_wait_fcr_sync(void) {
    uint32_t timeout = 0x8000u;
    while (((REG32(LCD_SR) & BIT(5)) == 0u) && timeout-- != 0u) {
    }
}

static void lcd_wait_update_done(void) {
    uint32_t timeout = 0x8000u;
    while (((REG32(LCD_SR) & BIT(2)) != 0u) && timeout-- != 0u) {
    }
}

static void lcd_wait_status(uint32_t mask) {
    uint32_t timeout = 0x80000u;
    while (((REG32(LCD_SR) & mask) != mask) && timeout-- != 0u) {
    }
}

static void lcd_init_clock_domain(void) {
    REG32(RCC_APB1ENR) |= PWR_APB1EN;
    (void)REG32(RCC_APB1ENR);

    REG32(PWR_CR) |= PWR_CR_DBP;

    /*
     * The stock firmware resets the backup domain, enables LSE, selects it as
     * the RTC/LCD clock source, then enables the RTC clock gate. The LCD glass
     * needs this low-speed domain clock to actually transfer RAM updates.
     */
    REG32(RCC_CSR) |= RCC_CSR_RTCRST;
    REG32(RCC_CSR) &= ~RCC_CSR_RTCRST;

    REG32(RCC_CSR) |= RCC_CSR_LSEON;
    uint32_t timeout = 2000000u;
    while (((REG32(RCC_CSR) & RCC_CSR_LSERDY) == 0u) && timeout-- != 0u) {
    }

    REG32(RCC_CSR) = (REG32(RCC_CSR) & ~RCC_CSR_RTCSEL_MASK) | RCC_CSR_RTCSEL_LSE;
    REG32(RCC_CSR) |= RCC_CSR_RTCEN;
}

static void lcd_config_mask_af11(uint32_t port, uint16_t mask) {
    for (uint8_t pin = 0; pin < 16u; ++pin) {
        if ((mask & BIT(pin)) != 0u) {
            gpio_config_pin(port, pin, GPIO_MODE_AF, GPIO_PULL_NONE, LCD_AF);
            uint32_t shift2 = (uint32_t)pin * 2u;
            REG32(GPIO_OSPEEDR(port)) &= ~(3u << shift2);
        }
    }
}

static void lcd_discharge_mask_low(uint32_t port, uint16_t mask) {
    for (uint8_t pin = 0; pin < 16u; ++pin) {
        if ((mask & BIT(pin)) != 0u) {
            gpio_clear(port, pin);
            gpio_config_pin(port, pin, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0u);
            uint32_t shift2 = (uint32_t)pin * 2u;
            REG32(GPIO_OSPEEDR(port)) &= ~(3u << shift2);
        }
    }
}

static void lcd_discharge_glass_pins(void) {
    lcd_discharge_mask_low(GPIOA_BASE, 0x87CEu);
    lcd_discharge_mask_low(GPIOB_BASE, 0xFF3Bu);
    lcd_discharge_mask_low(GPIOC_BASE, 0x06FFu);
    lcd_discharge_mask_low(GPIOD_BASE, 0xFF00u);
    lcd_discharge_mask_low(GPIOE_BASE, 0x000Fu);
    delay_ms(4u);
}

static uint32_t lcd_contrast_bits(uint8_t level) {
    if (level < 10u) {
        level = 10u;
    }
    if (level > 17u) {
        level = 17u;
    }
    return (uint32_t)(level - 10u) * 0x0400u;
}

void lcd_set_contrast(uint8_t contrast_level) {
    REG32(LCD_FCR) &= 0xFFFFE3FFu;
    REG32(LCD_FCR) |= lcd_contrast_bits(contrast_level);
    lcd_wait_fcr_sync();
}

static void lcd_request_update(void) {
    REG32(LCD_SR_UDR_BB) = 1;
}

void lcd_clear_shadow(void) {
    for (uint8_t i = 0; i < 16u; ++i) {
        g_lcd_shadow[i] = 0;
    }
}

static void lcd_clear_controller_ram_raw(void) {
    for (uint8_t i = 0; i < 16u; ++i) {
        REG32(LCD_RAM_BASE + (uint32_t)i * 4u) = 0;
    }
}

uint32_t lcd_debug_shadow_word(uint8_t index) {
    return index < 16u ? g_lcd_shadow[index] : 0u;
}

void lcd_commit(void) {
    lcd_wait_update_done();
    for (uint8_t i = 0; i < 16u; ++i) {
        REG32(LCD_RAM_BASE + (uint32_t)i * 4u) = g_lcd_shadow[i];
    }
    lcd_request_update();
}

void lcd_blank(void) {
    lcd_clear_shadow();
    lcd_commit();
}

void lcd_power_down(void) {
    lcd_blank();
    lcd_wait_update_done();
    REG32(LCD_CR_LCDEN_BB) = 0;
    REG32(RCC_APB1ENR) &= ~LCD_APB1EN;
}

void lcd_show_stock_unused_segments(void) {
    /*
     * Stock writes these glass bits in secondary/alt-display paths that our
     * normal UI does not currently expose. Keep this as a boot-only diagnostic
     * so the unknown annunciators can be identified on the real glass.
     */
    static const uint32_t k_stock_unused_segment_masks[16] = {
        0x00000001u, 0x00000000u, 0x80000000u, 0x00000000u,
        0x00020000u, 0x00000000u, 0x00070000u, 0x00000000u,
        0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
        0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    };

    for (uint8_t i = 0; i < 16u; ++i) {
        g_lcd_shadow[i] = k_stock_unused_segment_masks[i];
    }
    lcd_commit();
}

static uint8_t lcd_char_to_segment_mask(uint8_t ch) {
    if (ch >= '0' && ch <= '9') {
        return k_digit_masks[ch - '0'];
    }
    if (ch >= 'A' && ch <= 'Z') {
        return k_upper_masks[ch - 'A'];
    }
    if (ch >= 'a' && ch <= 'z') {
        return k_lower_masks[ch - 'a'];
    }

    switch (ch) {
    case ' ':
        return 0x00u;
    case '-':
        return 0x04u;
    case '=':
        return 0x82u;
    case 0xFFu:
        return 0xBFu;
    default:
        return 0x00u;
    }
}

void lcd_draw_char_at_position(uint8_t ch, uint8_t pos) {
    uint32_t m = lcd_char_to_segment_mask(ch);

    switch (pos) {
    case 0:
        g_lcd_shadow[0] &= ~0x10000000u;
        g_lcd_shadow[2] &= 0xE7FFFFFFu;
        g_lcd_shadow[4] &= 0xE7FFFFFFu;
        g_lcd_shadow[6] &= 0xE7FFFFFFu;
        g_lcd_shadow[0] |= (m << 21) & 0x18000000u;
        g_lcd_shadow[2] |= (m << 23) & 0x18000000u;
        g_lcd_shadow[4] |= (m << 25) & 0x18000000u;
        g_lcd_shadow[6] |= (m << 27) & 0x18000000u;
        break;
    case 1:
        g_lcd_shadow[2] &= ~0x02000000u;
        g_lcd_shadow[4] &= ~0x02000000u;
        g_lcd_shadow[6] &= ~0x02000000u;
        g_lcd_shadow[0] |= (m << 19) & 0x02000000u;
        g_lcd_shadow[2] |= (m << 21) & 0x02000000u;
        g_lcd_shadow[4] |= (m << 23) & 0x02000000u;
        g_lcd_shadow[6] |= (m << 25) & 0x02000000u;
        g_lcd_shadow[1] &= 0xEFFu;
        g_lcd_shadow[3] &= 0xEFFu;
        g_lcd_shadow[5] &= 0xEFFu;
        g_lcd_shadow[7] &= 0xEFFu;
        g_lcd_shadow[1] |= (m << 1) & 0x100u;
        g_lcd_shadow[3] |= (m << 3) & 0x100u;
        g_lcd_shadow[5] |= (m << 5) & 0x100u;
        g_lcd_shadow[7] |= (m << 7) & 0x100u;
        break;
    case 2:
        g_lcd_shadow[0] &= ~0x01000000u;
        g_lcd_shadow[2] &= 0xFE7FFFFFu;
        g_lcd_shadow[4] &= 0xFE7FFFFFu;
        g_lcd_shadow[6] &= 0xFE7FFFFFu;
        g_lcd_shadow[0] |= (m << 17) & 0x01800000u;
        g_lcd_shadow[2] |= (m << 19) & 0x01800000u;
        g_lcd_shadow[4] |= (m << 21) & 0x01800000u;
        g_lcd_shadow[6] |= (m << 23) & 0x01800000u;
        break;
    case 3:
        g_lcd_shadow[0] &= ~0x00400000u;
        g_lcd_shadow[2] &= 0xFF9FFFFFu;
        g_lcd_shadow[4] &= 0xFF9FFFFFu;
        g_lcd_shadow[6] &= 0xFF9FFFFFu;
        g_lcd_shadow[0] |= (m << 15) & 0x00600000u;
        g_lcd_shadow[2] |= (m << 17) & 0x00600000u;
        g_lcd_shadow[4] |= (m << 19) & 0x00600000u;
        g_lcd_shadow[6] |= (m << 21) & 0x00600000u;
        break;
    case 4:
        g_lcd_shadow[0] &= ~0x00100000u;
        g_lcd_shadow[2] &= 0xFFE7FFFFu;
        g_lcd_shadow[4] &= 0xFFE7FFFFu;
        g_lcd_shadow[6] &= 0xFFE7FFFFu;
        g_lcd_shadow[0] |= (m << 13) & 0x00180000u;
        g_lcd_shadow[2] |= (m << 15) & 0x00180000u;
        g_lcd_shadow[4] |= (m << 17) & 0x00180000u;
        g_lcd_shadow[6] |= (m << 19) & 0x00180000u;
        break;
    case 5:
        g_lcd_shadow[1] &= 0xFBFu;
        g_lcd_shadow[3] &= 0xF9Fu;
        g_lcd_shadow[5] &= 0xF9Fu;
        g_lcd_shadow[7] &= 0xF9Fu;
        g_lcd_shadow[1] |= (m >> 1) & 0x60u;
        g_lcd_shadow[3] |= (m << 1) & 0x60u;
        g_lcd_shadow[5] |= (m << 3) & 0x60u;
        g_lcd_shadow[7] |= (m << 5) & 0x60u;
        break;
    case 6:
        g_lcd_shadow[1] &= 0xFEFu;
        g_lcd_shadow[3] &= 0xFE7u;
        g_lcd_shadow[5] &= 0xFE7u;
        g_lcd_shadow[7] &= 0xFE7u;
        g_lcd_shadow[1] |= (m >> 3) & 0x18u;
        g_lcd_shadow[3] |= (m >> 1) & 0x18u;
        g_lcd_shadow[5] |= (m << 1) & 0x18u;
        g_lcd_shadow[7] |= (m << 3) & 0x18u;
        break;
    case 7:
        g_lcd_shadow[1] &= 0xFFBu;
        g_lcd_shadow[3] &= 0xFF9u;
        g_lcd_shadow[5] &= 0xFF9u;
        g_lcd_shadow[7] &= 0xFF9u;
        g_lcd_shadow[1] |= (m >> 5) & 0x06u;
        g_lcd_shadow[3] |= (m >> 3) & 0x06u;
        g_lcd_shadow[5] |= (m >> 1) & 0x06u;
        g_lcd_shadow[7] |= (m << 1) & 0x06u;
        break;
    case 8:
        g_lcd_shadow[0] &= ~0x00008000u;
        g_lcd_shadow[2] &= 0xFFFF3FFFu;
        g_lcd_shadow[4] &= 0xFFFF3FFFu;
        g_lcd_shadow[6] &= 0xFFFF3FFFu;
        g_lcd_shadow[0] |= (m << 8) & 0x0000C000u;
        g_lcd_shadow[2] |= (m << 10) & 0x0000C000u;
        g_lcd_shadow[4] |= (m << 12) & 0x0000C000u;
        g_lcd_shadow[6] |= (m << 14) & 0x0000C000u;
        break;
    case 9:
        g_lcd_shadow[0] &= ~0x00002000u;
        g_lcd_shadow[2] &= 0xFFFFCFFFu;
        g_lcd_shadow[4] &= 0xFFFFCFFFu;
        g_lcd_shadow[6] &= 0xFFFFCFFFu;
        g_lcd_shadow[0] |= (m << 6) & 0x00003000u;
        g_lcd_shadow[2] |= (m << 8) & 0x00003000u;
        g_lcd_shadow[4] |= (m << 10) & 0x00003000u;
        g_lcd_shadow[6] |= (m << 12) & 0x00003000u;
        break;
    default:
        break;
    }
}

void lcd_write_5chars(bool upper, const char text[5]) {
    uint8_t pos = upper ? 0u : 5u;
    for (uint8_t i = 0; i < 5u; ++i) {
        lcd_draw_char_at_position((uint8_t)text[i], (uint8_t)(pos + i));
    }
}

void lcd_clear_main_numeric_marks(void) {
    g_lcd_shadow[0] &= ~0x00004000u;
    g_lcd_shadow[1] &= 0x00000FD5u;
    g_lcd_shadow[3] &= ~0x00000080u;
}

static void lcd_set_main_decimal_position(uint8_t pos) {
    g_lcd_shadow[0] &= ~0x00004000u;
    g_lcd_shadow[1] &= 0x00000FD5u;

    if (pos == 1u) {
        g_lcd_shadow[0] |= 0x00004000u;
    } else if (pos == 2u) {
        g_lcd_shadow[1] |= 0x00000002u;
    } else if (pos == 3u) {
        g_lcd_shadow[1] |= 0x00000008u;
    } else if (pos == 4u) {
        g_lcd_shadow[1] |= 0x00000020u;
    }
}

void lcd_show_startup_version(void) {
    lcd_clear_shadow();
    lcd_write_5chars(false, "V-206");
    lcd_set_main_decimal_position(2u);
    lcd_commit();
}

static void lcd_set_raw_segment_index(uint16_t index) {
    if (index >= 512u) {
        return;
    }
    uint8_t word = (uint8_t)(index >> 5);
    uint8_t bit = (uint8_t)(index & 31u);
    g_lcd_shadow[word] |= BIT(bit);
}

void lcd_show_raw_segment_test(uint16_t index) {
    if (index > 511u) {
        index = 511u;
    }
    lcd_clear_shadow();
    lcd_write_5chars(true, "     ");
    lcd_write_lower_numeric((int32_t)index, 0u);
    lcd_set_raw_segment_index(index);
    lcd_commit();
}

void lcd_write_lower_blank_with_decimal(uint8_t decimal_pos) {
    lcd_set_main_decimal_position(decimal_pos);
    g_lcd_shadow[3] &= ~0x00000080u;
    lcd_write_5chars(false, "     ");
}

static void lcd_set_secondary_decimal_position(uint8_t pos) {
    g_lcd_shadow[0] &= 0xF55FFFFFu;

    if (pos == 1u) {
        g_lcd_shadow[0] |= 0x00200000u;
    } else if (pos == 2u) {
        g_lcd_shadow[0] |= 0x00800000u;
    } else if (pos == 3u) {
        g_lcd_shadow[0] |= 0x02000000u;
    } else if (pos == 4u) {
        g_lcd_shadow[0] |= 0x08000000u;
    }
}

void lcd_set_secondary_decimal(uint8_t pos) {
    lcd_set_secondary_decimal_position(pos);
}

static void lcd_normalize_numeric_chars(uint8_t decimal_pos, char out[5]) {
    if (decimal_pos == 0u) {
        if (out[0] == '0') {
            out[0] = ' ';
            if (out[1] == '0') {
                out[1] = ' ';
                if (out[2] == '0') {
                    out[2] = ' ';
                    if (out[3] == '0') {
                        out[3] = ' ';
                    }
                }
            }
        }
    } else if (decimal_pos == 1u) {
        if (out[0] == '0') {
            out[0] = ' ';
            if (out[1] == '0') {
                out[1] = ' ';
                if (out[2] == '0') {
                    out[2] = ' ';
                }
            }
        }
    } else if (decimal_pos == 2u) {
        if (out[0] == '0') {
            out[0] = ' ';
            if (out[1] == '0') {
                out[1] = ' ';
            }
        }
    } else if (decimal_pos == 3u) {
        if (out[0] == '0') {
            out[0] = ' ';
        }
    }
}

void lcd_write_lower_numeric(int32_t value, uint8_t decimal_pos) {
    bool negative = value < 0;
    uint32_t abs_value = negative ? (uint32_t)(-value) : (uint32_t)value;
    if (abs_value > 99999u) {
        lcd_clear_main_numeric_marks();
        lcd_write_5chars(false, "OFL  ");
        return;
    }

    lcd_set_main_decimal_position(decimal_pos);
    if (negative) {
        g_lcd_shadow[3] |= 0x00000080u;
    } else {
        g_lcd_shadow[3] &= ~0x00000080u;
    }

    char out[5];
    out[0] = (char)('0' + (abs_value / 10000u));
    abs_value %= 10000u;
    out[1] = (char)('0' + (abs_value / 1000u));
    abs_value %= 1000u;
    out[2] = (char)('0' + (abs_value / 100u));
    abs_value %= 100u;
    out[3] = (char)('0' + (abs_value / 10u));
    out[4] = (char)('0' + (abs_value % 10u));

    lcd_normalize_numeric_chars(decimal_pos, out);
    lcd_write_5chars(false, out);
}

void lcd_write_lower_temperature_c_tenths(int32_t value, uint8_t decimal_pos) {
    if (value > 99999 || value < -99999) {
        lcd_clear_main_numeric_marks();
        lcd_write_5chars(false, "OPEN ");
        return;
    }

    lcd_write_lower_numeric(value, decimal_pos);
}

void lcd_write_lower_diode_open(uint8_t decimal_pos) {
    lcd_set_main_decimal_position(decimal_pos);
    g_lcd_shadow[3] &= ~0x00000080u;
    lcd_write_5chars(false, " 0FL ");
}

void lcd_write_lower_ohms_open(uint8_t decimal_pos) {
    lcd_set_main_decimal_position(decimal_pos);
    g_lcd_shadow[3] &= ~0x00000080u;
    lcd_write_5chars(false, "0FL  ");
}

void lcd_write_lower_continuity_open(uint8_t decimal_pos) {
    lcd_set_main_decimal_position(decimal_pos);
    g_lcd_shadow[3] &= ~0x00000080u;
    lcd_write_5chars(false, "0FL  ");
}

void lcd_write_lower_overflow(uint8_t decimal_pos) {
    lcd_set_main_decimal_position(decimal_pos);
    g_lcd_shadow[3] &= ~0x00000080u;
    lcd_write_5chars(false, " OFL ");
}

void lcd_write_upper_signed_numeric(int32_t value, uint8_t decimal_pos) {
    bool negative = value < 0;
    uint32_t abs_value = negative ? (uint32_t)(-(value + 1)) + 1u : (uint32_t)value;
    if (abs_value > 99999u) {
        lcd_set_secondary_decimal_position(0u);
        g_lcd_shadow[4] &= ~0x20000000u;
        lcd_write_5chars(true, " OFL ");
        return;
    }

    lcd_set_secondary_decimal_position(decimal_pos);
    if (negative) {
        g_lcd_shadow[4] |= 0x20000000u;
    } else {
        g_lcd_shadow[4] &= ~0x20000000u;
    }

    char out[5];
    out[0] = (char)('0' + (abs_value / 10000u));
    abs_value %= 10000u;
    out[1] = (char)('0' + (abs_value / 1000u));
    abs_value %= 1000u;
    out[2] = (char)('0' + (abs_value / 100u));
    abs_value %= 100u;
    out[3] = (char)('0' + (abs_value / 10u));
    out[4] = (char)('0' + (abs_value % 10u));

    lcd_normalize_numeric_chars(decimal_pos, out);
    lcd_write_5chars(true, out);
}

void lcd_set_secondary_sign(bool negative) {
    if (negative) {
        g_lcd_shadow[4] |= 0x20000000u;
    } else {
        g_lcd_shadow[4] &= ~0x20000000u;
    }
}

void lcd_write_upper_b_scaled(int32_t value, uint8_t scale_mode, bool overrange) {
    bool negative = value < 0;
    uint32_t abs_value = negative ? (uint32_t)(-(value + 1)) + 1u : (uint32_t)value;
    uint32_t scaled = abs_value;

    if (overrange) {
        lcd_set_secondary_sign(false);
        lcd_set_secondary_decimal_position(0u);
        lcd_write_5chars(true, "b OFL");
        return;
    }

    if (scale_mode == 0u) {
        scaled = abs_value / 1000u;
        if ((abs_value % 1000u) >= 500u) {
            ++scaled;
        }
    } else if (scale_mode != 100u) {
        scaled = abs_value / 100u;
        if ((abs_value % 100u) >= 50u) {
            ++scaled;
        }
    }

    if (scaled > 9999u) {
        lcd_set_secondary_sign(false);
        lcd_set_secondary_decimal_position(0u);
        lcd_write_5chars(true, "b OFL");
        return;
    }

    lcd_set_secondary_sign(negative);
    lcd_set_secondary_decimal_position(1u);
    char out[5];
    out[0] = 'b';
    out[1] = (char)('0' + (scaled / 1000u) % 10u);
    out[2] = (char)('0' + (scaled / 100u) % 10u);
    out[3] = (char)('0' + (scaled / 10u) % 10u);
    out[4] = (char)('0' + scaled % 10u);
    lcd_write_5chars(true, out);
}

void lcd_write_upper_temperature_c_tenths(int16_t value) {
    lcd_write_upper_temperature_tenths(value, false);
}

void lcd_write_upper_temperature_tenths(int16_t value, bool fahrenheit) {
    bool negative = value < 0;
    uint16_t abs_value = negative ? (uint16_t)(-value) : (uint16_t)value;
    uint32_t digits = (uint32_t)abs_value * 10u;

    if (digits > 9999u) {
        lcd_set_secondary_decimal_position(0u);
        lcd_write_5chars(true, "OPEN ");
        return;
    }

    lcd_set_secondary_decimal_position(2u);

    char out[5];
    out[0] = (char)('0' + (digits / 10000u));
    digits %= 10000u;
    out[1] = (char)('0' + (digits / 1000u));
    digits %= 1000u;
    out[2] = (char)('0' + (digits / 100u));
    digits %= 100u;
    out[3] = (char)('0' + (digits / 10u));
    out[4] = fahrenheit ? 'F' : 'c';

    lcd_normalize_numeric_chars(2u, out);
    if (negative) {
        if (out[0] == ' ') {
            out[0] = '-';
        } else {
            out[1] = '-';
        }
    }
    lcd_write_5chars(true, out);
}

void lcd_write_upper_battery_decivolts(uint16_t decivolts) {
    if (decivolts > 99u) {
        decivolts = 99u;
    }

    lcd_set_secondary_decimal_position(1u);
    char out[5];
    out[0] = 'B';
    out[1] = 'A';
    out[2] = 'T';
    out[3] = (char)('0' + ((decivolts / 10u) % 10u));
    out[4] = (char)('0' + (decivolts % 10u));
    lcd_write_5chars(true, out);
}

void lcd_write_upper_battery_percent(uint8_t percent) {
    if (percent > 99u) {
        percent = 99u;
    }

    lcd_set_secondary_decimal_position(1u);
    char out[5];
    out[0] = 'B';
    out[1] = 'A';
    out[2] = 'T';
    out[3] = (char)('0' + (percent / 10u));
    out[4] = (char)('0' + (percent % 10u));
    lcd_write_5chars(true, out);
}

static void lcd_clear_mode_unit_annunciators(void) {
    g_lcd_shadow[0] &= ~0x60020900u;
    g_lcd_shadow[1] &= ~0x00000001u;
    g_lcd_shadow[2] &= ~0x00000F00u;
    g_lcd_shadow[3] &= ~0x00000001u;
    g_lcd_shadow[4] &= ~0x80000780u;
    g_lcd_shadow[5] &= ~0x00000001u;
    g_lcd_shadow[6] &= ~0x80000581u;
}

static void lcd_set_unit_scale_index(uint8_t index) {
    if (index == 0u) {
        g_lcd_shadow[6] &= ~0x80u;
        g_lcd_shadow[2] &= ~0x800u;
    } else if (index == 1u) {
        g_lcd_shadow[6] |= 0x80u;
        g_lcd_shadow[2] &= ~0x800u;
    } else if (index == 2u) {
        g_lcd_shadow[6] |= 0x80u;
        g_lcd_shadow[2] |= 0x800u;
    }
}

static void lcd_set_p10_unit_scale(uint8_t mode, uint8_t range) {
    g_lcd_shadow[6] &= ~0x100u;
    g_lcd_shadow[4] &= ~0x80u;
    g_lcd_shadow[6] &= ~0x80u;
    g_lcd_shadow[2] &= ~0x800u;

    if (mode == 1u || mode == 2u || mode == 25u) {
        g_lcd_shadow[4] |= 0x80u;
        lcd_set_unit_scale_index(range);
    } else if (mode == 3u || mode == 4u) {
        g_lcd_shadow[4] |= 0x80u;
        lcd_set_unit_scale_index((uint8_t)(range + 1u));
    } else if (mode == 9u) {
        g_lcd_shadow[4] |= 0x80u;
        if (range >= 5u) {
            lcd_set_unit_scale_index((uint8_t)(range - 5u));
        } else if (range >= 2u) {
            lcd_set_unit_scale_index((uint8_t)(range - 2u));
        } else {
            lcd_set_unit_scale_index((uint8_t)(range + 1u));
        }
    } else if (mode == 10u) {
        g_lcd_shadow[4] |= 0x80u;
        lcd_set_unit_scale_index(2u);
    } else if (mode == 11u) {
        g_lcd_shadow[4] |= 0x80u;
        lcd_set_unit_scale_index(range);
    } else if (mode == 16u || mode == 17u) {
        g_lcd_shadow[4] |= 0x80u;
        lcd_set_unit_scale_index((uint8_t)(range + 1u));
    } else if (mode == 18u || mode == 19u) {
        g_lcd_shadow[4] |= 0x80u;
        lcd_set_unit_scale_index(range);
    } else if (mode == 20u || mode == 21u) {
        g_lcd_shadow[4] |= 0x80u;
        lcd_set_unit_scale_index(range == 0u ? 2u : 0u);
    }
}

void lcd_set_p10_mode_annunciators(uint8_t mode, uint8_t range) {
    lcd_clear_mode_unit_annunciators();
    lcd_set_p10_unit_scale(mode, range);

    if (mode == 0u) {
        g_lcd_shadow[0] |= 0x40000000u;
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[5] |= 0x1u;
    } else if (mode == 1u) {
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[5] |= 0x1u;
    } else if (mode == 2u) {
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[1] |= 0x1u;
    } else if (mode == 3u) {
        g_lcd_shadow[2] |= 0x400u;
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[5] |= 0x1u;
    } else if (mode == 4u) {
        g_lcd_shadow[2] |= 0x400u;
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[1] |= 0x1u;
    } else if (mode == 25u) {
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[1] |= 0x1u;
        g_lcd_shadow[3] |= 0x1u;
        g_lcd_shadow[5] |= 0x1u;
    } else if (mode == 6u) {
        if (range >= 5u) {
            g_lcd_shadow[0] &= ~0x100u;
            g_lcd_shadow[0] |= 0x800u;
        } else {
            if (range >= 2u) {
                g_lcd_shadow[0] |= 0x100u;
            } else {
                g_lcd_shadow[0] &= ~0x100u;
            }
            g_lcd_shadow[0] &= ~0x800u;
        }
        g_lcd_shadow[4] |= 0x100u;
    } else if (mode == 7u) {
        g_lcd_shadow[6] |= 0x1u;
    } else if (mode == 8u) {
        g_lcd_shadow[0] |= 0x20000u;
    } else if (mode == 9u) {
        if (range >= 5u) {
            g_lcd_shadow[0] &= ~0x100u;
            g_lcd_shadow[0] |= 0x800u;
        } else {
            if (range >= 2u) {
                g_lcd_shadow[0] |= 0x100u;
            } else {
                g_lcd_shadow[0] &= ~0x100u;
            }
            g_lcd_shadow[0] &= ~0x800u;
        }
        g_lcd_shadow[2] |= 0x100u;
    } else if (mode == 10u) {
        g_lcd_shadow[4] |= 0x80000000u;
        g_lcd_shadow[2] |= 0x100u;
    } else if (mode == 11u) {
        g_lcd_shadow[6] |= 0x80000000u;
        g_lcd_shadow[4] |= 0x400u;
    } else if (mode == 12u) {
        if (range >= 2u) {
            g_lcd_shadow[0] |= 0x200u;
            g_lcd_shadow[2] &= ~0x200u;
        } else {
            g_lcd_shadow[0] &= ~0x200u;
            g_lcd_shadow[2] |= 0x200u;
        }
        g_lcd_shadow[4] |= 0x200u;
    } else if (mode == 13u) {
        g_lcd_shadow[0] |= 0x200u;
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[1] |= 0x1u;
    } else if (mode == 14u) {
        g_lcd_shadow[2] |= 0x400u;
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[1] |= 0x1u;
    } else if (mode == 15u) {
        if (range < 2u) {
            g_lcd_shadow[2] |= 0x400u;
        }
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[1] |= 0x1u;
    } else if (mode == 16u) {
        g_lcd_shadow[0] |= 0x200u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[1] |= 0x1u;
    } else if (mode == 17u) {
        g_lcd_shadow[0] |= 0x200u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[5] |= 0x1u;
    } else if (mode == 18u) {
        g_lcd_shadow[2] |= 0x400u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[1] |= 0x1u;
    } else if (mode == 19u) {
        g_lcd_shadow[2] |= 0x400u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[5] |= 0x1u;
    } else if (mode == 20u) {
        if (range == 0u) {
            g_lcd_shadow[2] |= 0x400u;
        }
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[1] |= 0x1u;
    } else if (mode == 21u) {
        if (range == 0u) {
            g_lcd_shadow[2] |= 0x400u;
        }
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[5] |= 0x1u;
    } else if (mode == 22u) {
        g_lcd_shadow[0] |= 0x200u;
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[5] |= 0x1u;
    } else if (mode == 23u) {
        g_lcd_shadow[2] |= 0x400u;
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[5] |= 0x1u;
    } else if (mode == 24u) {
        if (range < 2u) {
            g_lcd_shadow[2] |= 0x400u;
        }
        g_lcd_shadow[4] |= 0x400u;
        g_lcd_shadow[6] |= 0x400u;
        g_lcd_shadow[5] |= 0x1u;
    }
}

void lcd_set_temperature_unit_annunciator(bool temp_mode, bool fahrenheit) {
    g_lcd_shadow[4] &= ~0x800u;
    g_lcd_shadow[6] &= ~0x800u;
    if (!temp_mode) {
        return;
    }
    if (fahrenheit) {
        g_lcd_shadow[6] |= 0x800u;
    } else {
        g_lcd_shadow[4] |= 0x800u;
    }
}

void lcd_set_auto_annunciator(bool enabled) {
    if (enabled) {
        g_lcd_shadow[7] |= 0x00000001u;
    } else {
        g_lcd_shadow[7] &= ~0x00000001u;
    }
}

void lcd_set_apo_annunciator(bool enabled) {
    if (enabled) {
        g_lcd_shadow[5] |= 0x00000080u;
    } else {
        g_lcd_shadow[5] &= ~0x00000080u;
    }
}

void lcd_set_low_battery_annunciator(bool enabled) {
    if (enabled) {
        g_lcd_shadow[7] |= 0x00000080u;
    } else {
        g_lcd_shadow[7] &= ~0x00000080u;
    }
}

void lcd_set_threshold_annunciator(bool enabled) {
    if (enabled) {
        g_lcd_shadow[1] |= 0x00000080u;
    } else {
        g_lcd_shadow[1] &= ~0x00000080u;
    }
}

void lcd_set_rel_hold_annunciator(uint8_t state) {
    if (state == 1u) {
        g_lcd_shadow[2] |= 0x00010000u;
        g_lcd_shadow[4] |= 0x00010000u;
    } else if (state == 2u) {
        g_lcd_shadow[2] &= ~0x00010000u;
        g_lcd_shadow[4] |= 0x00010000u;
    } else {
        g_lcd_shadow[2] &= ~0x00010000u;
        g_lcd_shadow[4] &= ~0x00010000u;
    }
}

void lcd_set_rel_annunciator(bool enabled) {
    lcd_set_rel_hold_annunciator(enabled ? 1u : 0u);
}

void lcd_set_relative_delta_annunciator(bool enabled) {
    if (enabled) {
        g_lcd_shadow[0] |= 0x00000001u;
    } else {
        g_lcd_shadow[0] &= ~0x00000001u;
    }
}

void lcd_set_minmax_annunciator(uint8_t state) {
    g_lcd_shadow[2] &= ~0x40000000u;
    g_lcd_shadow[4] &= ~0x40000000u;
    g_lcd_shadow[6] &= ~0x40000000u;

    if (state == 1u) {
        g_lcd_shadow[4] |= 0x40000000u;
    } else if (state == 2u) {
        g_lcd_shadow[6] |= 0x40000000u;
    } else if (state == 3u) {
        g_lcd_shadow[2] |= 0x40000000u;
    } else if (state == 4u) {
        g_lcd_shadow[2] |= 0x40000000u;
        g_lcd_shadow[4] |= 0x40000000u;
        g_lcd_shadow[6] |= 0x40000000u;
    }
}

void lcd_set_mem_annunciator(bool enabled) {
    if (enabled) {
        g_lcd_shadow[0] |= 0x00010000u;
    } else {
        g_lcd_shadow[0] &= ~0x00010000u;
    }
}

void lcd_set_bt_annunciator(bool enabled) {
    if (enabled) {
        g_lcd_shadow[0] |= 0x80000000u;
    } else {
        g_lcd_shadow[0] &= ~0x80000000u;
    }
}

void lcd_set_secondary_voltage_annunciator(bool enabled) {
    if (enabled) {
        g_lcd_shadow[2] |= 0x00020000u;
    } else {
        g_lcd_shadow[2] &= ~0x00020000u;
    }
}

void lcd_set_secondary_frequency_annunciator(uint8_t range) {
    if (range >= 2u) {
        g_lcd_shadow[4] |= 0x00020000u;
    }
    g_lcd_shadow[6] |= 0x00020000u;
}

void lcd_set_dbm_annunciator(bool enabled) {
    if (enabled) {
        g_lcd_shadow[0] |= 0x000C0000u;
    } else {
        g_lcd_shadow[0] &= ~0x000C0000u;
    }
}

void lcd_clear_bargraph(void) {
    for (uint8_t i = 0; i < (uint8_t)(sizeof(k_bar_segments) / sizeof(k_bar_segments[0])); ++i) {
        g_lcd_shadow[k_bar_segments[i].ram] &= ~k_bar_segments[i].bit;
    }

    g_lcd_shadow[0] &= ~(0x00001000u | 0x00000400u | 0x00000002u);
    g_lcd_shadow[2] &= ~0x00000001u;
    g_lcd_shadow[4] &= ~0x00000001u;
}

static bool lcd_stock_bargraph_high_scale(uint8_t mode, uint8_t range) {
    if (mode == 0u) {
        return true;
    }
    if (mode >= 1u && mode <= 4u) {
        return range == 3u;
    }
    if (mode == 20u || mode == 21u) {
        return range == 2u;
    }
    return false;
}

void lcd_set_bargraph_limited(int32_t value, bool overrange, uint8_t mode,
                              uint8_t range, uint8_t max_count) {
    lcd_clear_bargraph();

    if (value < 0) {
        g_lcd_shadow[4] |= 0x00000001u;
        value = -value;
    } else {
        g_lcd_shadow[2] |= 0x00000001u;
    }

    bool high_scale = lcd_stock_bargraph_high_scale(mode, range);
    if (high_scale) {
        g_lcd_shadow[0] &= ~0x00001000u;
        g_lcd_shadow[0] |= 0x00000400u;
        g_lcd_shadow[6] |= 0x00000100u;
        g_lcd_shadow[4] &= ~0x00000080u;
        g_lcd_shadow[6] &= ~0x00000080u;
        g_lcd_shadow[2] &= ~0x00000800u;
    } else {
        g_lcd_shadow[0] |= 0x00001000u;
        g_lcd_shadow[0] &= ~0x00000400u;
    }
    g_lcd_shadow[0] |= 0x00000002u;

    uint32_t divisor = high_scale ? 400u : 2000u;
    uint8_t count = overrange ? 25u : (uint8_t)((uint32_t)value / divisor);
    if (count > 25u) {
        count = 25u;
    }
    if (max_count > 25u) {
        max_count = 25u;
    }
    if (count > max_count) {
        count = max_count;
    }

    for (uint8_t i = 0; i < count; ++i) {
        g_lcd_shadow[k_bar_segments[i].ram] |= k_bar_segments[i].bit;
    }
}

void lcd_set_bargraph(int32_t value, bool overrange, uint8_t mode) {
    lcd_set_bargraph_limited(value, overrange, mode, 0u, 25u);
}

void lcd_init_blank(uint8_t contrast_level) {
    lcd_discharge_glass_pins();
    lcd_init_clock_domain();

    lcd_config_mask_af11(GPIOA_BASE, 0x87CEu);
    lcd_config_mask_af11(GPIOB_BASE, 0xFF3Bu);
    lcd_config_mask_af11(GPIOC_BASE, 0x06FFu);
    lcd_config_mask_af11(GPIOD_BASE, 0xFF00u);
    lcd_config_mask_af11(GPIOE_BASE, 0x000Fu);

    REG32(RCC_APB1ENR) |= LCD_APB1EN;
    (void)REG32(RCC_APB1ENR);

    REG32(LCD_CR_LCDEN_BB) = 0;
    lcd_clear_shadow();
    lcd_clear_controller_ram_raw();
    REG32(LCD_FCR) &= 0xFC03FFFFu;
    REG32(LCD_FCR) |= 0x00C00000u;
    lcd_wait_fcr_sync();

    REG32(LCD_CR) &= 0xFFFFFF81u;
    REG32(LCD_CR) |= 0x0000004Cu;
    REG32(LCD_CR_MUXSEG_BB) = 0;

    REG32(LCD_FCR) &= 0xFFFFE3FFu;
    REG32(LCD_FCR) |= lcd_contrast_bits(contrast_level);
    REG32(LCD_FCR) &= 0xFFFFFC7Fu;
    REG32(LCD_FCR) &= 0xFFFFFF8Fu;
    REG32(LCD_FCR) |= 0x00000040u;
    lcd_wait_fcr_sync();

    REG32(LCD_CR_LCDEN_BB) = 1;
    lcd_wait_status(BIT(0));
    lcd_wait_status(BIT(4));

    REG32(LCD_FCR) &= 0xFFFC1FFFu;
    REG32(LCD_FCR) |= 0x00004000u;
    lcd_blank();
}
