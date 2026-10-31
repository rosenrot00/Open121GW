#include "input.h"

#include "gpio.h"
#include "hw.h"

static bool g_input_debug_rotary_override;
static bool g_input_debug_jack_override;
static bool g_input_debug_buttons_override;
static uint8_t g_input_debug_rotary;
static uint8_t g_input_debug_jack;
static uint8_t g_input_debug_buttons;

static bool input_rotary_is_onehot(uint8_t value) {
    return value == 0x01u || value == 0x02u || value == 0x04u || value == 0x08u ||
           value == 0x10u || value == 0x20u || value == 0x40u || value == 0x80u;
}

static uint8_t input_rotary_gpio_active_low(void) {
    uint32_t idr = REG32(GPIO_IDR(GPIOH_BASE));
    uint8_t mask = 0;

    if ((idr & BIT(7)) == 0u) {
        mask |= 0x01u;
    }
    if ((idr & BIT(6)) == 0u) {
        mask |= 0x02u;
    }
    if ((idr & BIT(5)) == 0u) {
        mask |= 0x04u;
    }
    if ((idr & BIT(4)) == 0u) {
        mask |= 0x08u;
    }
    if ((idr & BIT(3)) == 0u) {
        mask |= 0x10u;
    }
    if ((idr & BIT(2)) == 0u) {
        mask |= 0x20u;
    }
    if ((idr & BIT(1)) == 0u) {
        mask |= 0x40u;
    }
    if ((idr & BIT(0)) == 0u) {
        mask |= 0x80u;
    }

    return mask;
}

void input_init(void) {
    for (uint8_t pin = 0; pin < 16u; ++pin) {
        gpio_config_pin(GPIOH_BASE, pin, GPIO_MODE_INPUT, GPIO_PULL_UP, 0);
    }

    gpio_config_pin(GPIOE_BASE, 10, GPIO_MODE_INPUT, GPIO_PULL_NONE, 0);
    gpio_config_pin(GPIOE_BASE, 11, GPIO_MODE_INPUT, GPIO_PULL_NONE, 0);
}

uint8_t input_rotary_sample_active_low(void) {
    uint8_t physical = input_rotary_gpio_active_low();
    if (g_input_debug_rotary_override &&
        input_rotary_is_onehot(physical) &&
        input_rotary_is_onehot(g_input_debug_rotary) &&
        physical != g_input_debug_rotary) {
        g_input_debug_rotary_override = false;
        g_input_debug_rotary = 0u;
        return physical;
    }

    if (g_input_debug_rotary_override) {
        return g_input_debug_rotary;
    }

    return physical;
}

uint8_t input_jack_sample_active_low(void) {
    if (g_input_debug_jack_override) {
        return g_input_debug_jack;
    }

    uint32_t idr = REG32(GPIO_IDR(GPIOE_BASE));
    uint8_t state = 0;

    if ((idr & BIT(10)) == 0u) {
        state |= 0x01u;
    }
    if ((idr & BIT(11)) == 0u) {
        state |= 0x02u;
    }

    return state;
}

uint8_t input_button_sample_active_low(void) {
    if (g_input_debug_buttons_override) {
        return g_input_debug_buttons;
    }

    uint32_t idr = REG32(GPIO_IDR(GPIOH_BASE));
    uint8_t mask = 0;

    for (uint8_t pin = 8; pin < 16u; ++pin) {
        if ((idr & BIT(pin)) == 0u) {
            mask |= (uint8_t)BIT(pin - 8u);
        }
    }

    return mask;
}

void input_debug_override_set(bool enabled, uint8_t rotary, uint8_t jack, uint8_t buttons) {
    g_input_debug_rotary_override = enabled;
    g_input_debug_jack_override = enabled;
    g_input_debug_buttons_override = enabled;
    g_input_debug_rotary = rotary;
    g_input_debug_jack = jack;
    g_input_debug_buttons = buttons;
}

void input_debug_override_clear(void) {
    g_input_debug_rotary_override = false;
    g_input_debug_jack_override = false;
    g_input_debug_buttons_override = false;
    g_input_debug_rotary = 0;
    g_input_debug_jack = 0;
    g_input_debug_buttons = 0;
}

void input_debug_override_set_jack(bool enabled, uint8_t jack) {
    g_input_debug_jack_override = enabled;
    g_input_debug_jack = jack;
}

void input_debug_override_set_buttons(bool enabled, uint8_t buttons) {
    g_input_debug_buttons_override = enabled;
    g_input_debug_buttons = buttons;
}

bool input_debug_override_enabled(void) {
    return g_input_debug_rotary_override || g_input_debug_jack_override ||
           g_input_debug_buttons_override;
}

bool input_debug_override_rotary_enabled(void) {
    return g_input_debug_rotary_override;
}

bool input_debug_override_jack_enabled(void) {
    return g_input_debug_jack_override;
}

bool input_debug_override_buttons_enabled(void) {
    return g_input_debug_buttons_override;
}

uint8_t input_debug_override_rotary(void) {
    return g_input_debug_rotary;
}

uint8_t input_debug_override_jack(void) {
    return g_input_debug_jack;
}

uint8_t input_debug_override_buttons(void) {
    return g_input_debug_buttons;
}

uint8_t input_debug_physical_rotary(void) {
    return input_rotary_gpio_active_low();
}

uint8_t input_debug_physical_jack(void) {
    uint32_t idr = REG32(GPIO_IDR(GPIOE_BASE));
    uint8_t state = 0;

    if ((idr & BIT(10)) == 0u) {
        state |= 0x01u;
    }
    if ((idr & BIT(11)) == 0u) {
        state |= 0x02u;
    }

    return state;
}
