#include "gpio.h"

#include "hw.h"

void gpio_enable_all_clocks(void) {
    REG32(RCC_AHBENR) |= 0x000000FFu;
    (void)REG32(RCC_AHBENR);
}

void gpio_config_pin(uint32_t base, uint8_t pin, gpio_mode_t mode, gpio_pull_t pull, uint8_t af) {
    uint32_t shift2 = (uint32_t)pin * 2u;
    uint32_t mask2 = 3u << shift2;

    REG32(GPIO_MODER(base)) = (REG32(GPIO_MODER(base)) & ~mask2) | ((uint32_t)mode << shift2);
    REG32(GPIO_PUPDR(base)) = (REG32(GPIO_PUPDR(base)) & ~mask2) | ((uint32_t)pull << shift2);

    if (mode == GPIO_MODE_OUTPUT || mode == GPIO_MODE_AF) {
        REG32(GPIO_OTYPER(base)) &= ~BIT(pin);
        REG32(GPIO_OSPEEDR(base)) = (REG32(GPIO_OSPEEDR(base)) & ~mask2) | (2u << shift2);
    }

    if (mode == GPIO_MODE_AF) {
        uint32_t afr_addr = pin < 8u ? GPIO_AFRL(base) : GPIO_AFRH(base);
        uint32_t afr_shift = (uint32_t)(pin & 7u) * 4u;
        REG32(afr_addr) = (REG32(afr_addr) & ~(0xFu << afr_shift)) | ((uint32_t)(af & 0xFu) << afr_shift);
    }
}

void gpio_set(uint32_t base, uint8_t pin) {
    REG32(GPIO_BSRR(base)) = BIT(pin);
}

void gpio_clear(uint32_t base, uint8_t pin) {
    REG32(GPIO_BSRR(base)) = BIT(pin + 16u);
}

void gpio_write(uint32_t base, uint8_t pin, bool high) {
    if (high) {
        gpio_set(base, pin);
    } else {
        gpio_clear(base, pin);
    }
}

bool gpio_read(uint32_t base, uint8_t pin) {
    return (REG32(GPIO_IDR(base)) & BIT(pin)) != 0u;
}
