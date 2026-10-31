#ifndef EEVBLOG_ANALOG_H
#define EEVBLOG_ANALOG_H

#include <stdbool.h>
#include <stdint.h>

bool analog_read_adc_channel(uint8_t channel, uint16_t *raw);
void analog_power_down(void);
void analog_aux_adc_init(void);
bool analog_start_adc_channel_irq(uint8_t channel);
bool analog_take_irq_sample(uint8_t *channel, uint16_t *raw);
void ADC1_IRQHandler(void);

#endif
