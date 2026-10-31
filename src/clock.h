#ifndef EEVBLOG_CLOCK_H
#define EEVBLOG_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

bool clock_init_stock_msi_4mhz(void);
void clock_init_systick_1ms(void);
void clock_systick_irq(void);
uint32_t clock_millis(void);
uint32_t clock_core_hz(void);
uint32_t clock_apb1_hz(void);
void delay_ms(uint32_t ms);
void delay_cycles(uint32_t cycles);

#endif
