#include "backlight.h"

#include <stdint.h>

#include "calibration.h"
#include "gpio.h"
#include "hw.h"

#define BACKLIGHT_PORT GPIOE_BASE
#define BACKLIGHT_PIN  8u

#define BACKLIGHT_SETTING_OFFSET 0x04F0u
#define BACKLIGHT_SETTING_ON     0xA5u
#define BACKLIGHT_SETTING_OFF    0x5Au

static bool g_backlight_enabled;

void backlight_set(bool enabled) {
    g_backlight_enabled = enabled;
    gpio_write(BACKLIGHT_PORT, BACKLIGHT_PIN, enabled);
}

bool backlight_enabled(void) {
    return g_backlight_enabled;
}

void backlight_init(void) {
    gpio_config_pin(BACKLIGHT_PORT, BACKLIGHT_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);

    uint8_t saved = REG8(CAL_FLASH_BASE + BACKLIGHT_SETTING_OFFSET);
    backlight_set(saved == BACKLIGHT_SETTING_ON);
}

bool backlight_toggle_persist(void) {
    bool enabled = !g_backlight_enabled;
    backlight_set(enabled);
    uint8_t saved = enabled ? BACKLIGHT_SETTING_ON : BACKLIGHT_SETTING_OFF;
    return calibration_write_u8(BACKLIGHT_SETTING_OFFSET, saved);
}
