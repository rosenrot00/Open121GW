#ifndef EEVBLOG_GPIO_H
#define EEVBLOG_GPIO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT = 1,
    GPIO_MODE_AF = 2,
    GPIO_MODE_ANALOG = 3,
} gpio_mode_t;

typedef enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP = 1,
    GPIO_PULL_DOWN = 2,
} gpio_pull_t;

void gpio_enable_all_clocks(void);
void gpio_config_pin(uint32_t base, uint8_t pin, gpio_mode_t mode, gpio_pull_t pull, uint8_t af);
void gpio_set(uint32_t base, uint8_t pin);
void gpio_clear(uint32_t base, uint8_t pin);
void gpio_write(uint32_t base, uint8_t pin, bool high);
bool gpio_read(uint32_t base, uint8_t pin);

#endif
