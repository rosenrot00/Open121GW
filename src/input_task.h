#ifndef EEVBLOG_INPUT_TASK_H
#define EEVBLOG_INPUT_TASK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t raw_rotary;
    uint8_t raw_jack;
    uint8_t raw_buttons;
    uint8_t rotary;
    uint8_t jack;
    uint8_t buttons;
    uint8_t button_pressed;
    uint8_t button_released;
    bool rotary_changed;
    bool jack_changed;
    bool initialized;
    uint32_t sample_count;
} input_task_snapshot_t;

void input_task_init(uint32_t now_ms);
void input_task_poll(uint32_t now_ms);
void input_task_inject_button_pulse(uint8_t mask, uint32_t now_ms, uint32_t duration_ms);
void input_task_get_snapshot(input_task_snapshot_t *snapshot);
uint8_t input_task_raw_rotary(void);
uint8_t input_task_raw_jack(void);
uint8_t input_task_raw_buttons(void);
uint8_t input_task_rotary(void);
uint8_t input_task_jack(void);
uint8_t input_task_buttons(void);
uint8_t input_task_button_pressed(void);
uint8_t input_task_button_released(void);

#endif
