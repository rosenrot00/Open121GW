#include "buzzer.h"

#include <stdbool.h>

#include "clock.h"
#include "gpio.h"
#include "hw.h"

#define BUZZER_PORT GPIOE_BASE
#define BUZZER_PIN  7u

static bool g_buzzer_active;
static bool g_buzzer_enabled = true;
static uint32_t g_buzzer_off_at;

void buzzer_init(void) {
    gpio_config_pin(BUZZER_PORT, BUZZER_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    buzzer_off();
}

void buzzer_set_enabled(bool enabled) {
    g_buzzer_enabled = enabled;
    if (!enabled) {
        buzzer_off();
    }
}

bool buzzer_enabled(void) {
    return g_buzzer_enabled;
}

bool buzzer_active(void) {
    return g_buzzer_active;
}

void buzzer_on_ms(uint32_t ms) {
    if (!g_buzzer_enabled || ms == 0u) {
        buzzer_off();
        return;
    }
    gpio_set(BUZZER_PORT, BUZZER_PIN);
    g_buzzer_active = true;
    g_buzzer_off_at = clock_millis() + ms;
}

void buzzer_force_on_ms(uint32_t ms) {
    if (ms == 0u) {
        buzzer_off();
        return;
    }
    gpio_set(BUZZER_PORT, BUZZER_PIN);
    g_buzzer_active = true;
    g_buzzer_off_at = clock_millis() + ms;
}

void buzzer_beep_blocking_ms(uint32_t ms) {
    if (!g_buzzer_enabled || ms == 0u) {
        buzzer_off();
        return;
    }

    gpio_set(BUZZER_PORT, BUZZER_PIN);
    for (uint32_t i = 0; i < ms; ++i) {
        delay_cycles(clock_core_hz() / 6000u);
    }
    buzzer_off();
}

void buzzer_off(void) {
    gpio_clear(BUZZER_PORT, BUZZER_PIN);
    g_buzzer_active = false;
}

void buzzer_poll(void) {
    if (g_buzzer_active && (int32_t)(clock_millis() - g_buzzer_off_at) >= 0) {
        buzzer_off();
    }
}
