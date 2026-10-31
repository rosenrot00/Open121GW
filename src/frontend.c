#include "frontend.h"

#include "calibration.h"
#include "clock.h"
#include "gpio.h"
#include "hw.h"

#define FE_PORT GPIOG_BASE
#define FE_CLK  0u
#define FE_CS   1u
#define FE_DOUT 2u
#define FE_DIN  3u
#define FE_CLK_MASK  BIT(FE_CLK)
#define FE_DOUT_MASK BIT(FE_DOUT)
#define FE_DIN_MASK  BIT(FE_DIN)
#define DIODE_VOLTAGE_SEL_PORT GPIOE_BASE
#define DIODE_VOLTAGE_SEL_PIN  15u
#define DIODE_VOLTAGE_BOOST_PIN 6u
#define ROUTE_PA11_PIN 11u
#define ROUTE_PA12_PIN 12u
#define ANALOG_PA4_PIN  4u
#define ANALOG_PA5_PIN  5u
#define ROUTE_PC11_PIN 11u
#define ROUTE_PD0_PIN  0u
#define ROUTE_PD1_PIN  1u
#define ROUTE_PD7_PIN  7u
#define ROUTE_PE5_PIN  5u
#define ROUTE_PE7_PIN  7u
#define ROUTE_PE8_PIN  8u
#define ROUTE_PE9_PIN  9u
#define ROUTE_PE12_PIN 12u
#define ROUTE_PE13_PIN 13u
#define ROUTE_PF2_PIN  2u
#define ROUTE_PG4_PIN  4u
#define ROUTE_PG5_PIN  5u
#define ANALOG_PG11_PIN 11u
#define ANALOG_PG13_PIN 13u
#define ANALOG_PG14_PIN 14u
#define ROUTE_PB6_PIN  6u
#define ROUTE_PB7_PIN  7u
#define ROUTE_PC13_PIN 13u
#define CAP_OPTION_DETECT_PIN 8u
#define FE_STOCK_DELAY_SCALE 1u

static uint8_t g_frontend_reg20_shadow;
static uint8_t g_frontend_config_reg20;
static uint8_t g_frontend_active_mode;
static bool g_cap_option_table_enabled;

static const uint8_t k_diode_config[20] = {
    0x00u, 0x00u, 0x12u, 0x83u, 0x8Du,
    0x00u, 0x00u, 0x00u, 0x11u, 0x00u,
    0x00u, 0x00u, 0x08u, 0x00u, 0x40u,
    0x92u, 0x80u, 0xE1u, 0x03u, 0xACu,
};

static const uint8_t k_mode9_configs[7][20] = {
    {0x74u, 0xCFu, 0x13u, 0x8Fu, 0x85u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x40u, 0x06u, 0x00u, 0x04u, 0x94u, 0x80u, 0xD2u, 0x0Cu, 0x2Cu},
    {0x74u, 0xCFu, 0x13u, 0x8Fu, 0x85u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x40u, 0x06u, 0x00u, 0x04u, 0x94u, 0x80u, 0xD2u, 0x0Cu, 0x2Cu},
    {0x00u, 0x00u, 0x11u, 0x83u, 0x85u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x40u, 0x60u, 0x00u, 0x00u, 0x94u, 0x80u, 0xD3u, 0x0Cu, 0x2Cu},
    {0x00u, 0x00u, 0x12u, 0x83u, 0x85u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x40u, 0x00u, 0x06u, 0x00u, 0x94u, 0x80u, 0xD3u, 0x0Cu, 0x2Cu},
    {0x00u, 0x00u, 0x12u, 0x83u, 0x85u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x40u, 0x00u, 0x60u, 0x00u, 0x94u, 0x80u, 0xD2u, 0x0Cu, 0x20u},
    {0x00u, 0x00u, 0x17u, 0x93u, 0x85u, 0x00u, 0x00u, 0x00u, 0x55u, 0x00u,
     0x00u, 0x08u, 0x00u, 0x80u, 0x00u, 0x86u, 0x80u, 0xD1u, 0x00u, 0xACu},
    {0x00u, 0x00u, 0x14u, 0x93u, 0x85u, 0x00u, 0x00u, 0x00u, 0x55u, 0x00u,
     0x08u, 0x08u, 0x00u, 0x00u, 0x00u, 0x86u, 0x80u, 0xD1u, 0x00u, 0xACu},
};

static const uint8_t k_mode10_config[20] = {
    0x74u, 0xCFu, 0x10u, 0x8Fu, 0x85u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x40u, 0x06u, 0x00u, 0x04u,
    0x94u, 0x80u, 0xD2u, 0x0Cu, 0x2Cu,
};

static const uint8_t k_modes6_8_config[20] = {
    0xFEu, 0xDDu, 0x00u, 0x00u, 0x00u,
    0x01u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x22u, 0x00u, 0x00u, 0x90u, 0x28u,
    0xA0u, 0x00u, 0xC0u, 0x08u, 0x2Cu,
};

static const uint8_t k_mode12_configs[6][20] = {
    {0x76u, 0x21u, 0x07u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x08u, 0x00u, 0x80u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x76u, 0x43u, 0x07u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x08u, 0x00u, 0x80u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x76u, 0x21u, 0x07u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x08u, 0x80u, 0x00u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x76u, 0x21u, 0x04u, 0x8Au, 0xA5u, 0x00u, 0x00u, 0x00u, 0x55u, 0x00u,
     0x00u, 0x08u, 0x08u, 0x00u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x76u, 0x33u, 0x04u, 0x8Au, 0xA5u, 0x00u, 0x00u, 0x00u, 0x55u, 0x00u,
     0x00u, 0x08u, 0x08u, 0x00u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x52u, 0x21u, 0x00u, 0x00u, 0x00u, 0x11u, 0x00u, 0x50u, 0x11u, 0x00u,
     0x00u, 0x00u, 0x08u, 0x00u, 0x00u, 0x9Au, 0x00u, 0xC2u, 0x3Cu, 0x20u},
};

static const uint8_t k_mode12_default_configs[6][20] = {
    {0x76u, 0x21u, 0x07u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x08u, 0x00u, 0x80u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x76u, 0x21u, 0x07u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x08u, 0x00u, 0x80u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x76u, 0x21u, 0x07u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
     0x00u, 0x08u, 0x80u, 0x00u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x76u, 0x21u, 0x04u, 0x8Au, 0xA5u, 0x00u, 0x00u, 0x00u, 0x55u, 0x00u,
     0x00u, 0x08u, 0x08u, 0x00u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x76u, 0x21u, 0x04u, 0x8Au, 0xA5u, 0x00u, 0x00u, 0x00u, 0x55u, 0x00u,
     0x00u, 0x08u, 0x08u, 0x00u, 0x00u, 0x8Eu, 0x00u, 0xC2u, 0x0Eu, 0x20u},
    {0x52u, 0x21u, 0x00u, 0x00u, 0x00u, 0x11u, 0x00u, 0x50u, 0x11u, 0x00u,
     0x00u, 0x00u, 0x08u, 0x00u, 0x00u, 0x9Au, 0x00u, 0xC2u, 0x3Cu, 0x20u},
};

static const uint8_t k_current_temp_shared_configs[9][20] = {
    {0xF6u, 0xDDu, 0x12u, 0x8Bu, 0xF3u, 0x11u, 0x00u, 0x55u, 0x31u, 0x00u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x80u, 0xC7u, 0x3Du, 0x2Cu},
    {0xF6u, 0xDDu, 0x12u, 0x8Bu, 0xF3u, 0x11u, 0x00u, 0x55u, 0x31u, 0x00u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x80u, 0xC7u, 0x3Du, 0x2Cu},
    {0xF6u, 0xDDu, 0x12u, 0x8Bu, 0xF3u, 0x10u, 0x00u, 0x55u, 0x31u, 0x00u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x80u, 0xC7u, 0x3Du, 0x2Cu},
    {0x00u, 0x00u, 0x12u, 0x9Bu, 0x93u, 0x31u, 0x00u, 0x55u, 0x31u, 0x00u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x80u, 0xC7u, 0x33u, 0x2Cu},
    {0x00u, 0x00u, 0x12u, 0x8Bu, 0x93u, 0x11u, 0x00u, 0x55u, 0x31u, 0x00u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x80u, 0xC7u, 0x33u, 0x2Cu},
    {0x00u, 0x00u, 0x12u, 0x8Bu, 0x93u, 0x11u, 0x00u, 0x55u, 0x31u, 0x00u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x80u, 0xC7u, 0x33u, 0x2Cu},
    {0x00u, 0x00u, 0x12u, 0x9Bu, 0x93u, 0x31u, 0x00u, 0x55u, 0x31u, 0x00u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x80u, 0xC7u, 0x33u, 0x20u},
    {0x00u, 0x00u, 0x12u, 0x8Bu, 0x93u, 0x11u, 0x00u, 0x55u, 0x31u, 0x00u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x80u, 0xC7u, 0x33u, 0x20u},
    {0x00u, 0x00u, 0x12u, 0x8Bu, 0x93u, 0x11u, 0x00u, 0x55u, 0x31u, 0x00u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x80u, 0xC7u, 0x33u, 0x20u},
};

static const uint8_t k_stock_primary_voltage_configs[5][20] = {
    {0x00u, 0x00u, 0x13u, 0x8Au, 0x05u, 0x40u, 0x00u, 0x4Du, 0x31u, 0x01u,
     0x22u, 0x00u, 0x00u, 0x90u, 0x28u, 0xA0u, 0x80u, 0xC7u, 0x00u, 0x20u},
    {0x00u, 0x00u, 0x13u, 0x8Au, 0x05u, 0x40u, 0x00u, 0x4Du, 0x31u, 0x01u,
     0x22u, 0x00u, 0x00u, 0x09u, 0x28u, 0xA0u, 0x80u, 0xC7u, 0x08u, 0x2Cu},
    {0x00u, 0x00u, 0x13u, 0x8Au, 0x05u, 0x40u, 0x00u, 0x4Du, 0x31u, 0x01u,
     0x22u, 0x00u, 0x90u, 0x00u, 0x28u, 0xA0u, 0x80u, 0xC7u, 0x08u, 0x2Cu},
    {0x00u, 0x00u, 0x13u, 0x8Au, 0x05u, 0x40u, 0x00u, 0x4Du, 0x31u, 0x01u,
     0x22u, 0x00u, 0x09u, 0x00u, 0x28u, 0xA0u, 0x80u, 0xC7u, 0x08u, 0x2Cu},
    {0x00u, 0x00u, 0x16u, 0x8Au, 0x31u, 0x01u, 0x00u, 0x00u, 0x31u, 0x01u,
     0x22u, 0x00u, 0x00u, 0x90u, 0x28u, 0xA0u, 0x80u, 0xC7u, 0x08u, 0x20u},
};

static const uint8_t k_stock_secondary_voltage_configs[4][20] = {
    {0xF6u, 0xDDu, 0x02u, 0x83u, 0xF5u, 0x41u, 0x02u, 0x4Du, 0x31u, 0x01u,
     0x00u, 0x00u, 0x00u, 0x09u, 0x88u, 0xB0u, 0x80u, 0xC0u, 0x38u, 0x2Cu},
    {0xF6u, 0xDDu, 0x02u, 0x83u, 0xF5u, 0x41u, 0x02u, 0x4Du, 0x31u, 0x01u,
     0x00u, 0x00u, 0x90u, 0x00u, 0x88u, 0xB0u, 0x80u, 0xC0u, 0x38u, 0x2Cu},
    {0xF6u, 0xDDu, 0x02u, 0x83u, 0xF5u, 0x41u, 0x02u, 0x4Du, 0x31u, 0x01u,
     0x00u, 0x00u, 0x09u, 0x00u, 0x88u, 0xB0u, 0x80u, 0xC0u, 0x38u, 0x2Cu},
    {0xF6u, 0xDDu, 0x00u, 0x83u, 0xF5u, 0x41u, 0x02u, 0x4Du, 0x31u, 0x01u,
     0x00u, 0x00u, 0x09u, 0x00u, 0x88u, 0xB0u, 0x80u, 0xC0u, 0x38u, 0x2Cu},
};

static const uint8_t k_stock_alt_voltage_configs[5][20] = {
    {0xF6u, 0xDDu, 0x04u, 0x03u, 0x52u, 0x10u, 0x87u, 0x25u, 0x31u, 0xC4u,
     0x00u, 0x00u, 0x00u, 0x90u, 0x88u, 0xA0u, 0x99u, 0xC7u, 0x38u, 0x20u},
    {0xF6u, 0xDDu, 0x04u, 0x03u, 0x52u, 0x10u, 0x87u, 0x25u, 0x31u, 0xECu,
     0x00u, 0x00u, 0x00u, 0x09u, 0x88u, 0xA0u, 0xB0u, 0xC7u, 0x38u, 0x20u},
    {0xF6u, 0xDDu, 0x04u, 0x03u, 0x52u, 0x10u, 0x87u, 0x25u, 0x31u, 0xECu,
     0x00u, 0x00u, 0x90u, 0x00u, 0x88u, 0xA0u, 0x80u, 0xC7u, 0x38u, 0x20u},
    {0xF6u, 0xDDu, 0x04u, 0x03u, 0x52u, 0x10u, 0x87u, 0x25u, 0x31u, 0xECu,
     0x00u, 0x00u, 0x09u, 0x00u, 0x88u, 0xA0u, 0x80u, 0xC7u, 0x38u, 0x20u},
    {0x60u, 0x00u, 0x00u, 0x00u, 0x00u, 0x40u, 0x81u, 0x2Du, 0x00u, 0xF8u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x08u, 0x81u, 0x80u, 0xC0u, 0x0Au, 0x2Cu},
};

static const uint8_t k_mode3_or5_configs[2][20] = {
    {0x60u, 0x00u, 0x1Cu, 0x8Bu, 0x3Bu, 0x51u, 0x00u, 0x6Du, 0x31u, 0x01u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x08u, 0x81u, 0x80u, 0xC0u, 0x0Cu, 0x60u},
    {0x60u, 0x00u, 0x10u, 0x8Au, 0x35u, 0x40u, 0x00u, 0x6Du, 0x31u, 0x01u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x08u, 0x81u, 0x80u, 0xC0u, 0x0Cu, 0x2Cu},
};

static const uint8_t k_mode4_default_configs[2][20] = {
    {0xF6u, 0xDDu, 0x13u, 0x8Au, 0xF5u, 0x43u, 0x06u, 0x6Du, 0x31u, 0x01u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x08u, 0x91u, 0x80u, 0xC0u, 0x3Au, 0x2Cu},
    {0xF6u, 0xDDu, 0x13u, 0x8Au, 0xF5u, 0x40u, 0x00u, 0x6Du, 0x31u, 0x01u,
     0x00u, 0x00u, 0x00u, 0x00u, 0x08u, 0x91u, 0x80u, 0xC0u, 0x3Au, 0x2Cu},
};

static const uint8_t k_mode4_alt_configs[2][20] = {
    {0xF6u, 0xDDu, 0x07u, 0x03u, 0x00u, 0x13u, 0x81u, 0x25u, 0x11u, 0xECu,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x90u, 0x80u, 0xC7u, 0x3Au, 0x28u},
    {0xF6u, 0xDDu, 0x07u, 0x03u, 0x52u, 0x10u, 0x81u, 0x25u, 0x11u, 0xECu,
     0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x90u, 0x80u, 0xC7u, 0x3Au, 0x28u},
};

static bool frontend_detect_cap_option_table(void) {
    uint8_t high_count = 0;

    gpio_config_pin(GPIOG_BASE, CAP_OPTION_DETECT_PIN, GPIO_MODE_INPUT, GPIO_PULL_UP, 0);
    for (uint8_t i = 0; i < 20u; ++i) {
        if (gpio_read(GPIOG_BASE, CAP_OPTION_DETECT_PIN)) {
            ++high_count;
        }
    }

    bool enabled = high_count < 10u;
    gpio_config_pin(GPIOG_BASE, CAP_OPTION_DETECT_PIN, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
    return enabled;
}

static void frontend_delay(uint32_t stock_loop_count) {
    delay_cycles(stock_loop_count * FE_STOCK_DELAY_SCALE);
}

/*
 * The HY3131 serial bus is bit-banged on GPIOG. Keep the inner transfer loop
 * local so each edge is one ordered peripheral write instead of a C call.
 * At the 4 MHz stock-compatible core clock, adjacent accesses are already
 * slower than the HY3131's 60 ns setup/data-ready requirements.
 */
static inline void frontend_fast_set(uint32_t mask) {
    REG32(GPIO_BSRR(FE_PORT)) = mask;
}

static inline void frontend_fast_clear(uint32_t mask) {
    REG32(GPIO_BSRR(FE_PORT)) = mask << 16u;
}

static inline void frontend_fast_write_dout(bool high) {
    REG32(GPIO_BSRR(FE_PORT)) = high ? FE_DOUT_MASK : (FE_DOUT_MASK << 16u);
}

static inline bool frontend_fast_read_din(void) {
    return (REG32(GPIO_IDR(FE_PORT)) & FE_DIN_MASK) != 0u;
}

static void frontend_clock_pulse(void) {
    frontend_delay(1);
    frontend_fast_set(FE_CLK_MASK);
    frontend_delay(1);
    frontend_fast_clear(FE_CLK_MASK);
}

static void frontend_shift_write_byte(uint8_t value, bool final_turnaround) {
    for (uint8_t bit = 0; bit < 8u; ++bit) {
        frontend_fast_write_dout((value & 0x80u) != 0u);
        frontend_clock_pulse();
        value <<= 1;
    }

    if (final_turnaround) {
        frontend_fast_clear(FE_DOUT_MASK);
        frontend_clock_pulse();
    }
    frontend_fast_set(FE_DOUT_MASK);
}

static uint8_t frontend_shift_read_byte(void) {
    uint8_t value = 0;
    frontend_fast_clear(FE_CLK_MASK);
    for (uint8_t bit = 0; bit < 8u; ++bit) {
        frontend_fast_set(FE_CLK_MASK);
        value = (uint8_t)((value << 1u) | (frontend_fast_read_din() ? 1u : 0u));
        frontend_fast_clear(FE_CLK_MASK);
    }
    return value;
}

void frontend_init_pins(void) {
    gpio_config_pin(FE_PORT, FE_CLK, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(FE_PORT, FE_CS, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(FE_PORT, FE_DOUT, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(FE_PORT, FE_DIN, GPIO_MODE_INPUT, GPIO_PULL_UP, 0);
    gpio_config_pin(DIODE_VOLTAGE_SEL_PORT, DIODE_VOLTAGE_SEL_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(FE_PORT, DIODE_VOLTAGE_BOOST_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOA_BASE, ROUTE_PA11_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOA_BASE, ROUTE_PA12_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOC_BASE, ROUTE_PC11_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOD_BASE, ROUTE_PD0_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOD_BASE, ROUTE_PD1_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOD_BASE, ROUTE_PD7_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOE_BASE, ROUTE_PE5_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOE_BASE, ROUTE_PE7_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOE_BASE, ROUTE_PE8_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOE_BASE, ROUTE_PE9_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOE_BASE, ROUTE_PE12_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOE_BASE, ROUTE_PE13_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOF_BASE, ROUTE_PF2_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOG_BASE, ROUTE_PG4_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOG_BASE, ROUTE_PG5_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOB_BASE, ROUTE_PB6_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOB_BASE, ROUTE_PB7_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOC_BASE, ROUTE_PC13_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOA_BASE, ANALOG_PA4_PIN, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOA_BASE, ANALOG_PA5_PIN, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOG_BASE, ANALOG_PG11_PIN, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOG_BASE, ANALOG_PG13_PIN, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOG_BASE, ANALOG_PG14_PIN, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
    for (uint8_t pin = 0; pin < 16u; ++pin) {
        gpio_config_pin(GPIOH_BASE, pin, GPIO_MODE_INPUT, GPIO_PULL_UP, 0);
    }
    g_cap_option_table_enabled = frontend_detect_cap_option_table();

    gpio_clear(FE_PORT, FE_CLK);
    gpio_set(FE_PORT, FE_CS);
    gpio_set(FE_PORT, FE_DOUT);
    gpio_clear(GPIOB_BASE, ROUTE_PB6_PIN);
    gpio_clear(GPIOB_BASE, ROUTE_PB7_PIN);
    gpio_clear(GPIOC_BASE, ROUTE_PC13_PIN);
    gpio_clear(GPIOE_BASE, ROUTE_PE7_PIN);
    gpio_clear(GPIOE_BASE, ROUTE_PE8_PIN);
    gpio_set(GPIOE_BASE, ROUTE_PE9_PIN);
    gpio_clear(GPIOE_BASE, ROUTE_PE13_PIN);
    /*
     * Stock primes the mux through the common voltage route during startup,
     * which leaves PG4 high. Modes such as mV/DC do not touch PG4 later, so
     * keep the same retained state when booting directly into them.
     */
    gpio_set(GPIOG_BASE, ROUTE_PG4_PIN);
    frontend_set_diode_voltage_15v(false);
}

void frontend_power_down(void) {
    gpio_clear(FE_PORT, FE_CLK);
    gpio_set(FE_PORT, FE_CS);
    gpio_clear(FE_PORT, FE_DOUT);
    gpio_clear(DIODE_VOLTAGE_SEL_PORT, DIODE_VOLTAGE_SEL_PIN);
    gpio_clear(FE_PORT, DIODE_VOLTAGE_BOOST_PIN);

    gpio_clear(GPIOA_BASE, ROUTE_PA11_PIN);
    gpio_clear(GPIOA_BASE, ROUTE_PA12_PIN);
    gpio_clear(GPIOB_BASE, ROUTE_PB6_PIN);
    gpio_clear(GPIOB_BASE, ROUTE_PB7_PIN);
    gpio_clear(GPIOC_BASE, ROUTE_PC11_PIN);
    gpio_clear(GPIOC_BASE, ROUTE_PC13_PIN);
    gpio_clear(GPIOD_BASE, ROUTE_PD0_PIN);
    gpio_clear(GPIOD_BASE, ROUTE_PD1_PIN);
    gpio_clear(GPIOD_BASE, ROUTE_PD7_PIN);
    gpio_clear(GPIOE_BASE, ROUTE_PE5_PIN);
    gpio_clear(GPIOE_BASE, ROUTE_PE7_PIN);
    gpio_clear(GPIOE_BASE, ROUTE_PE8_PIN);
    gpio_clear(GPIOE_BASE, ROUTE_PE9_PIN);
    gpio_clear(GPIOE_BASE, ROUTE_PE12_PIN);
    gpio_clear(GPIOE_BASE, ROUTE_PE13_PIN);
    gpio_clear(GPIOF_BASE, ROUTE_PF2_PIN);
    gpio_clear(GPIOG_BASE, ROUTE_PG4_PIN);
    gpio_clear(GPIOG_BASE, ROUTE_PG5_PIN);
}

bool frontend_ready(void) {
    return gpio_read(FE_PORT, FE_DIN);
}

void frontend_write_registers(uint8_t reg, const uint8_t *src, uint8_t len) {
    gpio_set(FE_PORT, FE_DOUT);
    gpio_clear(FE_PORT, FE_CLK);
    gpio_clear(FE_PORT, FE_CS);
    frontend_delay(10);

    frontend_shift_write_byte((uint8_t)(reg * 2u), false);
    frontend_delay(10);
    for (uint8_t i = 0; i < len; ++i) {
        frontend_shift_write_byte(src[i], false);
        frontend_delay(10);
    }
    gpio_set(FE_PORT, FE_CS);
}

void frontend_read_registers(uint8_t reg, uint8_t *dst, uint8_t len) {
    gpio_set(FE_PORT, FE_DOUT);
    gpio_clear(FE_PORT, FE_CLK);
    gpio_clear(FE_PORT, FE_CS);
    frontend_delay(5);

    frontend_shift_write_byte((uint8_t)((reg * 2u) | 1u), true);
    frontend_delay(5);
    for (uint8_t i = 0; i < len; ++i) {
        dst[i] = frontend_shift_read_byte();
        frontend_delay(2);
    }
    gpio_set(FE_PORT, FE_CS);
}

static void frontend_restart_conversion(uint16_t timing_word) {
    uint8_t reg20_mask = (g_frontend_active_mode >= 6u && g_frontend_active_mode <= 8u) ?
                          0xFDu : 0xF9u;
    uint8_t reg20 = (uint8_t)(g_frontend_reg20_shadow & reg20_mask);
    frontend_write_registers(0x20u, &reg20, 1);
    frontend_delay(10000);

    uint8_t timing[3] = {
        0x00u,
        (uint8_t)(timing_word & 0xFFu),
        (uint8_t)(timing_word >> 8),
    };
    frontend_write_registers(0x1Bu, timing, 3);

    uint8_t verify[3];
    frontend_read_registers(0x1Bu, verify, 3);

    reg20 = (uint8_t)(g_frontend_reg20_shadow | 0x02u);
    frontend_write_registers(0x20u, &reg20, 1);
}

static bool frontend_mode_uses_voltage_common_route(uint8_t mode) {
    return mode == 0u || mode == 2u || mode == 4u ||
           mode == 13u || mode == 14u || mode == 15u ||
           mode == 16u || mode == 18u || mode == 20u;
}

static bool frontend_mode_uses_current_mux(uint8_t mode) {
    return mode >= 13u && mode <= 24u;
}

static bool frontend_mode_uses_primary_current_range_mux(uint8_t mode) {
    return mode == 16u || mode == 17u || mode == 18u ||
           mode == 19u || mode == 20u || mode == 21u;
}

static void frontend_apply_p10_routing(uint8_t mode, uint8_t range, bool voltage_alt_path) {
    /*
     * Stock sub_800FBE8 drives these analog mux pins after every mode/range
     * change. Keep the GPIO end state aligned with analog_mux_route_* in stock,
     * because these lines select the analog front-end path before conversion.
     */
    gpio_set(GPIOD_BASE, ROUTE_PD1_PIN);

    if (frontend_mode_uses_voltage_common_route(mode)) {
        gpio_clear(GPIOD_BASE, ROUTE_PD0_PIN);
        if ((mode == 2u || mode == 4u) && voltage_alt_path) {
            gpio_clear(GPIOG_BASE, ROUTE_PG4_PIN);
            gpio_set(GPIOG_BASE, ROUTE_PG5_PIN);
        } else {
            gpio_set(GPIOG_BASE, ROUTE_PG4_PIN);
            gpio_clear(GPIOG_BASE, ROUTE_PG5_PIN);
        }
    } else {
        gpio_set(GPIOD_BASE, ROUTE_PD0_PIN);
        /*
         * Stock leaves PG4 unchanged for this route. In particular, CAP keeps
         * the PG4-high state primed during frontend initialization while also
         * enabling PG5.
         */
        gpio_set(GPIOG_BASE, ROUTE_PG5_PIN);
    }

    gpio_write(GPIOC_BASE, ROUTE_PC11_PIN, mode != 9u);
    gpio_write(GPIOD_BASE, ROUTE_PD7_PIN, mode != 2u);

    gpio_write(GPIOA_BASE, ROUTE_PA12_PIN, !(mode >= 6u && mode <= 8u));
    gpio_write(GPIOA_BASE, ROUTE_PA11_PIN, !(mode == 14u || mode == 15u));
    gpio_write(GPIOF_BASE, ROUTE_PF2_PIN, mode == 5u);
    gpio_write(GPIOE_BASE, ROUTE_PE12_PIN, frontend_mode_uses_current_mux(mode));

    if (mode != 11u) {
        /*
         * Stock analog_mux_route_diode_boost clears PE15 only for mV AC
         * (mode 4). Other non-diode modes leave PE15 high and PG6 low.
         */
        gpio_write(DIODE_VOLTAGE_SEL_PORT, DIODE_VOLTAGE_SEL_PIN, mode != 4u);
        gpio_clear(FE_PORT, DIODE_VOLTAGE_BOOST_PIN);
    }

    if (frontend_mode_uses_primary_current_range_mux(mode)) {
        gpio_write(GPIOE_BASE, ROUTE_PE5_PIN, range == 0u);
    } else {
        gpio_set(GPIOE_BASE, ROUTE_PE5_PIN);
    }
}

void frontend_configure_diode(uint16_t timing_word) {
    g_frontend_active_mode = 11u;
    frontend_write_registers(0x20u, k_diode_config, (uint8_t)sizeof(k_diode_config));
    g_frontend_config_reg20 = k_diode_config[0];
    g_frontend_reg20_shadow = k_diode_config[0];

    const uint8_t reg1e[2] = {0x00u, 0x04u};
    frontend_write_registers(0x1Eu, reg1e, (uint8_t)sizeof(reg1e));
    frontend_restart_conversion(timing_word);
}

void frontend_configure_p10_lowz(bool alt_path, uint16_t timing_word) {
    g_frontend_active_mode = 0u;
    uint8_t config[20];
    const uint8_t *stock = alt_path ? k_stock_secondary_voltage_configs[3] :
                                      k_stock_primary_voltage_configs[3];
    for (uint8_t i = 0; i < 20u; ++i) {
        config[i] = stock[i];
    }

    if (alt_path) {
        config[10] |= 0x20u;
        config[14] = (uint8_t)((config[14] & (uint8_t)~0x80u) | 0x20u);
    }

    frontend_write_registers(0x20u, config, 20u);
    g_frontend_config_reg20 = config[0];
    g_frontend_reg20_shadow = config[0];

    const uint8_t reg1e[2] = {0x00u, alt_path ? 0x05u : 0x04u};
    frontend_write_registers(0x1Eu, reg1e, (uint8_t)sizeof(reg1e));
    frontend_restart_conversion(timing_word);
    frontend_apply_p10_routing(0u, 0u, alt_path);
}

void frontend_configure_p10_voltage(uint8_t mode, uint8_t range, bool alt_bank,
                                    bool alt_primary_path, uint16_t timing_word) {
    g_frontend_active_mode = mode;
    const uint8_t *config = k_stock_primary_voltage_configs[3];
    uint8_t control_profile = 0x04u;

    if (mode == 1u) {
        if (range >= 4u) {
            range = 3u;
        }
        config = k_stock_primary_voltage_configs[range];
    } else if (mode == 2u) {
        if (alt_bank && alt_primary_path) {
            if (range >= 5u) {
                range = 4u;
            }
            config = k_stock_primary_voltage_configs[range];
            control_profile = 0x04u;
        } else {
            if (range >= 4u) {
                range = 3u;
            }
            config = k_stock_secondary_voltage_configs[range];
            control_profile = 0x05u;
        }
    }

    (void)k_stock_alt_voltage_configs;
    frontend_write_registers(0x20u, config, 20u);
    g_frontend_config_reg20 = config[0];
    g_frontend_reg20_shadow = config[0];

    const uint8_t reg1e[2] = {0x00u, control_profile};
    frontend_write_registers(0x1Eu, reg1e, (uint8_t)sizeof(reg1e));
    frontend_restart_conversion(timing_word);
    frontend_apply_p10_routing(mode, range, alt_bank && !alt_primary_path);
}

void frontend_configure_p10_mode(uint8_t mode, uint8_t range, uint16_t timing_word,
                                 bool fast_ohms_search) {
    g_frontend_active_mode = mode;
    const uint8_t *config = k_diode_config;
    uint8_t config_override[20];
    uint8_t control_profile = (mode >= 13u && mode <= 15u) ? 0x05u : 0x04u;

    if (mode <= 2u) {
        frontend_configure_p10_voltage(mode, range, false, false, timing_word);
        return;
    }
    if (mode == 3u) {
        if (range >= 2u) {
            range = 1u;
        }
        config = k_mode3_or5_configs[range];
    } else if (mode == 4u) {
        if (range >= 2u) {
            range = 1u;
        }
        (void)k_mode4_alt_configs;
        config = k_mode4_default_configs[range];
        control_profile = 0x05u;
    } else if (mode == 5u) {
        (void)range;
        config = k_mode3_or5_configs[0];
    } else if (mode == 9u) {
        if (range >= 7u) {
            range = 6u;
        }
        config = k_mode9_configs[range];
        bool range6_option_disabled =
            range == 6u && calibration_read_u8(38u) != 0xA5u;
        if (range6_option_disabled || fast_ohms_search) {
            for (uint8_t i = 0; i < 20u; ++i) {
                config_override[i] = config[i];
            }
            if (range6_option_disabled) {
                config_override[10] &= (uint8_t)~0x08u;
            }
            /*
             * R22[2:0] is AD1OSR. Stock uses OSR 32768 in Ohm range 5
             * and OSR 4096 in range 6. During range search, cap those
             * slow profiles at OSR 2048; the precision profile is restored
             * before any displayed sample is accumulated.
             */
            if ((config_override[2] & 0x07u) > 0x03u) {
                config_override[2] =
                    (uint8_t)((config_override[2] & 0xF8u) | 0x03u);
            }
            config = config_override;
        }
    } else if (mode >= 6u && mode <= 8u) {
        config = k_modes6_8_config;
        control_profile = 0x01u;
    } else if (mode == 10u) {
        config = k_mode10_config;
    } else if (mode == 12u) {
        if (range >= 6u) {
            range = 5u;
        }
        config = g_cap_option_table_enabled ? k_mode12_configs[range] : k_mode12_default_configs[range];
        control_profile = 0x01u;
    } else if (mode == 14u || mode == 16u || mode == 18u || mode == 20u) {
        if (range >= 3u) {
            range = 2u;
        }
        config = k_current_temp_shared_configs[range];
        control_profile = 0x05u;
    } else if (mode == 15u || mode == 17u || mode == 19u || mode == 21u) {
        if (range >= 3u) {
            range = 2u;
        }
        config = k_current_temp_shared_configs[6u + range];
    }

    frontend_write_registers(0x20u, config, 20u);
    g_frontend_config_reg20 = config[0];
    g_frontend_reg20_shadow = config[0];

    const uint8_t reg1e[2] = {0x00u, control_profile};
    frontend_write_registers(0x1Eu, reg1e, (uint8_t)sizeof(reg1e));
    frontend_restart_conversion(timing_word);
    frontend_apply_p10_routing(mode, range, false);
}

void frontend_configure_p10_current_dual_bank(uint8_t mode, uint8_t range, bool bank1,
                                              uint8_t alt_range0, uint8_t alt_range1,
                                              uint16_t timing_word) {
    g_frontend_active_mode = mode;
    const uint8_t *config;
    uint8_t control_profile = (mode >= 13u && mode <= 15u) ? 0x05u : 0x04u;

    if (range >= 3u) {
        range = 2u;
    }
    if (alt_range0 >= 4u) {
        alt_range0 = 3u;
    }
    if (alt_range1 != 0u) {
        alt_range1 = 1u;
    }

    if (bank1) {
        uint8_t row = alt_range1;
        if (mode == 15u) {
            row = alt_range1 ? 2u : 0u;
        } else if (mode == 24u) {
            row = alt_range1 ? 5u : 3u;
        } else if (mode >= 22u && mode <= 24u) {
            row = 3u + alt_range1;
        }
        config = k_current_temp_shared_configs[row];
    } else if (mode >= 22u && mode <= 24u) {
        config = k_stock_primary_voltage_configs[alt_range0];
    } else {
        config = k_stock_secondary_voltage_configs[alt_range0];
    }

    frontend_write_registers(0x20u, config, 20u);
    g_frontend_config_reg20 = config[0];
    g_frontend_reg20_shadow = config[0];

    const uint8_t reg1e[2] = {0x00u, control_profile};
    frontend_write_registers(0x1Eu, reg1e, (uint8_t)sizeof(reg1e));
    frontend_restart_conversion(timing_word);
    frontend_apply_p10_routing(mode, range, false);
}

void frontend_restart_p10_conversion(uint16_t timing_word) {
    frontend_restart_conversion(timing_word);
}

void frontend_set_diode_voltage_15v(bool enabled) {
    /*
     * Stock sub_800FD30 selects the diode source:
     * 3 V mode:  PE15 high, PG6 low
     * 15 V mode: PE15 low,  PG6 high
     */
    gpio_config_pin(DIODE_VOLTAGE_SEL_PORT, DIODE_VOLTAGE_SEL_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(FE_PORT, DIODE_VOLTAGE_BOOST_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_write(DIODE_VOLTAGE_SEL_PORT, DIODE_VOLTAGE_SEL_PIN, !enabled);
    gpio_write(FE_PORT, DIODE_VOLTAGE_BOOST_PIN, enabled);
}

bool frontend_cap_option_table_enabled(void) {
    return g_cap_option_table_enabled;
}

uint8_t frontend_configured_reg20(void) {
    return g_frontend_config_reg20;
}

void frontend_restore_configured_reg20_shadow(void) {
    g_frontend_reg20_shadow = g_frontend_config_reg20;
}

void frontend_read_status(uint8_t *dst52) {
    frontend_read_registers(0x00u, dst52, FRONTEND_STATUS_SIZE);
    g_frontend_reg20_shadow = dst52[0x20u];
    uint8_t clear_events = 0u;
    frontend_write_registers(0x1Eu, &clear_events, 1u);
}

bool frontend_status_has_sample(const uint8_t *status52) {
    return ((status52[0x1Eu] & status52[0x1Fu]) & 0x04u) != 0u;
}

int32_t frontend_decode_signed24_sample(const uint8_t *status52) {
    int32_t value = (int32_t)status52[0] |
                    ((int32_t)status52[1] << 8) |
                    ((int32_t)status52[2] << 16);
    if (value > 0x800000) {
        value -= 0xFFFFFF;
    }
    return value;
}

uint32_t frontend_decode_u24_le_at(const uint8_t *status52, uint8_t offset) {
    return (uint32_t)status52[offset] |
           ((uint32_t)status52[offset + 1u] << 8) |
           ((uint32_t)status52[offset + 2u] << 16);
}
