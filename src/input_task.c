#include "input_task.h"

#include "input.h"

#define INPUT_TASK_ROTARY_DEBOUNCE_SAMPLES 5u
#define INPUT_TASK_ROTARY_LOCKOUT_SAMPLES 15u
#define INPUT_TASK_JACK_DEBOUNCE_SAMPLES 150u
#define INPUT_TASK_BUTTON_SAMPLE_MS 15u
#define INPUT_TASK_BUTTON_DEBOUNCE_SAMPLES 2u

static input_task_snapshot_t g_input_task_snapshot;
static uint8_t g_rotary_candidate;
static uint8_t g_rotary_debounce_count;
static uint8_t g_rotary_lockout_count;
static uint8_t g_jack_candidate;
static uint8_t g_jack_debounce_count;
static uint8_t g_button_last_raw[8];
static uint8_t g_button_stable[8];
static uint8_t g_button_debounce_count[8];
static uint32_t g_button_next_sample;
static uint8_t g_button_pulse_mask;
static uint32_t g_button_pulse_until;

static bool input_task_rotary_is_onehot(uint8_t rotary) {
    return rotary == 0x01u || rotary == 0x02u || rotary == 0x04u || rotary == 0x08u ||
           rotary == 0x10u || rotary == 0x20u || rotary == 0x40u || rotary == 0x80u;
}

void input_task_init(uint32_t now_ms) {
    g_input_task_snapshot.raw_rotary = 0;
    g_input_task_snapshot.raw_jack = 0;
    g_input_task_snapshot.raw_buttons = 0;
    g_input_task_snapshot.rotary = 0;
    g_input_task_snapshot.jack = 0;
    g_input_task_snapshot.buttons = 0;
    g_input_task_snapshot.button_pressed = 0;
    g_input_task_snapshot.button_released = 0;
    g_input_task_snapshot.rotary_changed = false;
    g_input_task_snapshot.jack_changed = false;
    g_input_task_snapshot.initialized = false;
    g_input_task_snapshot.sample_count = 0;
    g_rotary_candidate = 0;
    g_rotary_debounce_count = 0;
    g_rotary_lockout_count = 0;
    g_jack_candidate = 0;
    g_jack_debounce_count = 0;
    for (uint8_t i = 0; i < 8u; ++i) {
        g_button_last_raw[i] = 0;
        g_button_stable[i] = 0;
        g_button_debounce_count[i] = 0;
    }
    g_button_next_sample = now_ms;
    g_button_pulse_mask = 0;
    g_button_pulse_until = 0;
}

void input_task_inject_button_pulse(uint8_t mask, uint32_t now_ms, uint32_t duration_ms) {
    if (mask == 0u) {
        return;
    }
    g_button_pulse_mask |= mask;
    g_button_pulse_until = now_ms + duration_ms;
}

static void input_task_service_buttons(uint32_t now_ms, uint8_t raw_buttons) {
    if ((int32_t)(now_ms - g_button_next_sample) < 0) {
        return;
    }
    g_button_next_sample = now_ms + INPUT_TASK_BUTTON_SAMPLE_MS;

    uint8_t previous = g_input_task_snapshot.buttons;
    uint8_t stable = previous;

    for (uint8_t i = 0; i < 8u; ++i) {
        uint8_t mask = (uint8_t)(1u << i);
        uint8_t raw = raw_buttons & mask;

        if (raw == g_button_last_raw[i]) {
            if (g_button_debounce_count[i] < 3u) {
                ++g_button_debounce_count[i];
            }
        } else {
            g_button_last_raw[i] = raw;
            g_button_debounce_count[i] = 0;
        }

        if (g_button_debounce_count[i] >= INPUT_TASK_BUTTON_DEBOUNCE_SAMPLES &&
            raw != g_button_stable[i]) {
            g_button_stable[i] = raw;
        }

        if (g_button_stable[i] != 0u) {
            stable |= mask;
        } else {
            stable &= (uint8_t)~mask;
        }
    }

    g_input_task_snapshot.buttons = stable;
    g_input_task_snapshot.button_pressed = (uint8_t)(stable & (uint8_t)~previous);
    g_input_task_snapshot.button_released = (uint8_t)(previous & (uint8_t)~stable);
}

void input_task_poll(uint32_t now_ms) {
    uint8_t raw_rotary = input_rotary_sample_active_low();
    uint8_t raw_jack = (uint8_t)(input_jack_sample_active_low() & 0x03u);
    uint8_t raw_buttons = input_button_sample_active_low();
    if (g_button_pulse_mask != 0u) {
        if ((int32_t)(now_ms - g_button_pulse_until) < 0) {
            raw_buttons |= g_button_pulse_mask;
        } else {
            g_button_pulse_mask = 0;
        }
    }

    g_input_task_snapshot.raw_rotary = raw_rotary;
    g_input_task_snapshot.raw_jack = raw_jack;
    g_input_task_snapshot.raw_buttons = raw_buttons;
    g_input_task_snapshot.rotary_changed = false;
    g_input_task_snapshot.jack_changed = false;
    g_input_task_snapshot.button_pressed = 0;
    g_input_task_snapshot.button_released = 0;

    if (!g_input_task_snapshot.initialized) {
        g_input_task_snapshot.rotary = input_task_rotary_is_onehot(raw_rotary) ? raw_rotary : 0u;
        g_input_task_snapshot.jack = raw_jack;
        g_input_task_snapshot.buttons = raw_buttons;
        g_rotary_candidate = raw_rotary;
        g_jack_candidate = raw_jack;
        g_rotary_debounce_count = 0;
        g_rotary_lockout_count = 0;
        g_jack_debounce_count = 0;
        for (uint8_t i = 0; i < 8u; ++i) {
            uint8_t mask = (uint8_t)(1u << i);
            uint8_t raw = raw_buttons & mask;
            g_button_last_raw[i] = raw;
            g_button_stable[i] = raw;
            g_button_debounce_count[i] = INPUT_TASK_BUTTON_DEBOUNCE_SAMPLES;
        }
        g_button_next_sample = now_ms + INPUT_TASK_BUTTON_SAMPLE_MS;
        g_input_task_snapshot.initialized = true;
    } else {
        input_task_service_buttons(now_ms, raw_buttons);

        if (g_rotary_lockout_count != 0u) {
            --g_rotary_lockout_count;
        }
        if (raw_rotary != g_rotary_candidate) {
            g_rotary_candidate = raw_rotary;
            g_rotary_debounce_count = 0;
        } else if (g_rotary_debounce_count < INPUT_TASK_ROTARY_DEBOUNCE_SAMPLES) {
            ++g_rotary_debounce_count;
        }
        if (g_rotary_debounce_count >= INPUT_TASK_ROTARY_DEBOUNCE_SAMPLES &&
            input_task_rotary_is_onehot(g_rotary_candidate) &&
            g_rotary_candidate != g_input_task_snapshot.rotary &&
            g_rotary_lockout_count == 0u) {
            g_input_task_snapshot.rotary = g_rotary_candidate;
            g_input_task_snapshot.rotary_changed = true;
            g_rotary_lockout_count = INPUT_TASK_ROTARY_LOCKOUT_SAMPLES;
        }

        if (raw_jack != g_jack_candidate) {
            g_jack_candidate = raw_jack;
            g_jack_debounce_count = INPUT_TASK_JACK_DEBOUNCE_SAMPLES;
        } else if (g_jack_debounce_count != 0u) {
            --g_jack_debounce_count;
            if (g_jack_debounce_count == 0u && g_input_task_snapshot.jack != raw_jack) {
                g_input_task_snapshot.jack = raw_jack;
                g_input_task_snapshot.jack_changed = true;
            }
        }
    }

    ++g_input_task_snapshot.sample_count;
}

void input_task_get_snapshot(input_task_snapshot_t *snapshot) {
    if (snapshot == 0) {
        return;
    }
    *snapshot = g_input_task_snapshot;
}

uint8_t input_task_raw_rotary(void) {
    return g_input_task_snapshot.raw_rotary;
}

uint8_t input_task_raw_jack(void) {
    return g_input_task_snapshot.raw_jack;
}

uint8_t input_task_raw_buttons(void) {
    return g_input_task_snapshot.raw_buttons;
}

uint8_t input_task_rotary(void) {
    return g_input_task_snapshot.rotary;
}

uint8_t input_task_jack(void) {
    return g_input_task_snapshot.jack;
}

uint8_t input_task_buttons(void) {
    return g_input_task_snapshot.buttons;
}

uint8_t input_task_button_pressed(void) {
    return g_input_task_snapshot.button_pressed;
}

uint8_t input_task_button_released(void) {
    return g_input_task_snapshot.button_released;
}
