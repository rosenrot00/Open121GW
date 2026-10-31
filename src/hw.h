#ifndef EEVBLOG_HW_H
#define EEVBLOG_HW_H

#include <stdbool.h>
#include <stdint.h>

#define BIT(n) (1u << (n))

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define REG16(addr) (*(volatile uint16_t *)(uintptr_t)(addr))
#define REG8(addr)  (*(volatile uint8_t *)(uintptr_t)(addr))

#define APP_FLASH_BASE 0x08006000u
#define CAL_FLASH_BASE 0x08080000u

#define RCC_BASE       0x40023800u
#define RCC_CR         (RCC_BASE + 0x00u)
#define RCC_ICSCR      (RCC_BASE + 0x04u)
#define RCC_CFGR       (RCC_BASE + 0x08u)
#define RCC_AHBENR     (RCC_BASE + 0x1Cu)
#define RCC_APB1ENR    (RCC_BASE + 0x24u)
#define RCC_APB2ENR    (RCC_BASE + 0x20u)
#define RCC_APB2RSTR   (RCC_BASE + 0x14u)
#define RCC_CSR        (RCC_BASE + 0x34u)

#define FLASH_BASE     0x40023C00u
#define FLASH_ACR      (FLASH_BASE + 0x00u)
#define FLASH_PECR     (FLASH_BASE + 0x04u)
#define FLASH_PEKEYR   (FLASH_BASE + 0x0Cu)
#define FLASH_PRGKEYR  (FLASH_BASE + 0x10u)
#define FLASH_SR       (FLASH_BASE + 0x18u)
#define SCB_AIRCR      0xE000ED0Cu
#define SCB_ACTLR      0xE000E008u

#define PWR_BASE       0x40007000u
#define PWR_CR         (PWR_BASE + 0x00u)

#define LCD_BASE       0x40002400u
#define LCD_CR         (LCD_BASE + 0x00u)
#define LCD_FCR        (LCD_BASE + 0x04u)
#define LCD_SR         (LCD_BASE + 0x08u)
#define LCD_RAM_BASE   (LCD_BASE + 0x14u)
#define LCD_CR_LCDEN_BB  0x42048000u
#define LCD_CR_MUXSEG_BB 0x4204801Cu
#define LCD_SR_UDR_BB    0x42048108u

#define GPIOA_BASE     0x40020000u
#define GPIOB_BASE     0x40020400u
#define GPIOC_BASE     0x40020800u
#define GPIOD_BASE     0x40020C00u
#define GPIOE_BASE     0x40021000u
#define GPIOF_BASE     0x40021400u
#define GPIOG_BASE     0x40021800u
#define GPIOH_BASE     0x40021C00u

#define GPIO_MODER(base)   ((base) + 0x00u)
#define GPIO_OTYPER(base)  ((base) + 0x04u)
#define GPIO_OSPEEDR(base) ((base) + 0x08u)
#define GPIO_PUPDR(base)   ((base) + 0x0Cu)
#define GPIO_IDR(base)     ((base) + 0x10u)
#define GPIO_ODR(base)     ((base) + 0x14u)
#define GPIO_BSRR(base)    ((base) + 0x18u)
#define GPIO_AFRL(base)    ((base) + 0x20u)
#define GPIO_AFRH(base)    ((base) + 0x24u)

#define USART2_BASE    0x40004400u
#define USART_SR(base)  ((base) + 0x00u)
#define USART_DR(base)  ((base) + 0x04u)
#define USART_BRR(base) ((base) + 0x08u)
#define USART_CR1(base) ((base) + 0x0Cu)
#define USART_CR2(base) ((base) + 0x10u)
#define USART_CR3(base) ((base) + 0x14u)

#define SCB_VTOR       0xE000ED08u
#define SCB_SCR        0xE000ED10u
#define SYST_CSR       0xE000E010u
#define SYST_RVR       0xE000E014u
#define SYST_CVR       0xE000E018u
#define NVIC_ISER0     0xE000E100u
#define NVIC_ICER0     0xE000E180u
#define NVIC_ICPR0     0xE000E280u

static inline void cpu_dsb_isb(void) {
    __asm__ volatile("dsb\nisb" ::: "memory");
}

static inline void cpu_enable_irq(void) {
    __asm__ volatile("cpsie i" ::: "memory");
}

static inline void cpu_disable_irq(void) {
    __asm__ volatile("cpsid i" ::: "memory");
}

#endif
