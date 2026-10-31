#include "analog.h"

#include "gpio.h"
#include "hw.h"

#define ADC1_BASE 0x40012400u
#define ADC_SR    (ADC1_BASE + 0x00u)
#define ADC_CR1   (ADC1_BASE + 0x04u)
#define ADC_CR2   (ADC1_BASE + 0x08u)
#define ADC_SMPR1 (ADC1_BASE + 0x0Cu)
#define ADC_SMPR2 (ADC1_BASE + 0x10u)
#define ADC_SMPR3 (ADC1_BASE + 0x14u)
#define ADC_SQR1  (ADC1_BASE + 0x30u)
#define ADC_SQR5  (ADC1_BASE + 0x40u)
#define ADC_DR    (ADC1_BASE + 0x58u)

#define ADC_SR_EOC   BIT(1)
#define ADC_SR_ADONS BIT(6)
#define ADC_CR1_EOCIE BIT(5)
#define ADC_CR2_ADON BIT(0)
#define ADC_CR2_SWSTART BIT(30)
#define RCC_CR_HSION BIT(0)
#define RCC_CR_HSIRDY BIT(1)
#define ADC1_IRQ 18u

static volatile bool g_adc_irq_busy;
static volatile bool g_adc_irq_ready;
static volatile uint8_t g_adc_irq_channel;
static volatile uint16_t g_adc_irq_raw;

void analog_power_down(void) {
    REG32(ADC_CR1) &= ~ADC_CR1_EOCIE;
    REG32(ADC_CR2) &= ~ADC_CR2_ADON;
    REG32(RCC_APB2ENR) &= ~BIT(9);
    g_adc_irq_busy = false;
    g_adc_irq_ready = false;
}

static bool adc_enable_clock_and_hsi(void) {
    REG32(RCC_CR) |= RCC_CR_HSION;
    uint32_t timeout = 100000u;
    while (((REG32(RCC_CR) & RCC_CR_HSIRDY) == 0u) && timeout-- != 0u) {
    }
    if (timeout == 0u) {
        return false;
    }

    REG32(RCC_APB2ENR) |= BIT(9);
    (void)REG32(RCC_APB2ENR);
    return true;
}

static void adc_config_channel(uint8_t channel) {
    uint8_t shift;
    if (channel < 10u) {
        shift = (uint8_t)(channel * 3u);
        REG32(ADC_SMPR3) = (REG32(ADC_SMPR3) & ~(7u << shift)) | (7u << shift);
    } else {
        shift = (uint8_t)((channel - 10u) * 3u);
        REG32(ADC_SMPR2) = (REG32(ADC_SMPR2) & ~(7u << shift)) | (7u << shift);
    }
    REG32(ADC_SQR1) &= ~(0xFu << 20);
    REG32(ADC_SQR5) = (REG32(ADC_SQR5) & ~0x1Fu) | channel;
}

void analog_aux_adc_init(void) {
    if (!adc_enable_clock_and_hsi()) {
        return;
    }

    REG32(ADC_CR1) &= 0xFCFFFEFFu;
    REG32(ADC_CR2) &= 0xC0FFF7FDu;
    REG32(ADC_CR2) |= ADC_CR2_ADON;
    uint32_t timeout = 100000u;
    while (((REG32(ADC_SR) & ADC_SR_ADONS) == 0u) && timeout-- != 0u) {
    }
    REG32(NVIC_ICPR0 + (ADC1_IRQ / 32u) * 4u) = BIT(ADC1_IRQ % 32u);
    REG32(NVIC_ISER0 + (ADC1_IRQ / 32u) * 4u) = BIT(ADC1_IRQ % 32u);
}

bool analog_start_adc_channel_irq(uint8_t channel) {
    if (channel > 18u || g_adc_irq_busy || g_adc_irq_ready) {
        return false;
    }
    if (channel <= 7u) {
        gpio_config_pin(GPIOA_BASE, channel, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
    }
    if ((REG32(RCC_APB2ENR) & BIT(9)) == 0u || (REG32(ADC_CR2) & ADC_CR2_ADON) == 0u) {
        analog_aux_adc_init();
    }
    if ((REG32(ADC_SR) & ADC_SR_ADONS) == 0u) {
        return false;
    }

    g_adc_irq_channel = channel;
    g_adc_irq_busy = true;
    adc_config_channel(channel);
    REG32(ADC_SR) &= ~ADC_SR_EOC;
    REG32(ADC_CR1) |= ADC_CR1_EOCIE;
    REG32(ADC_CR2) |= ADC_CR2_SWSTART;
    return true;
}

bool analog_take_irq_sample(uint8_t *channel, uint16_t *raw) {
    if (!g_adc_irq_ready || channel == 0 || raw == 0) {
        return false;
    }
    *channel = g_adc_irq_channel;
    *raw = g_adc_irq_raw;
    g_adc_irq_ready = false;
    return true;
}

void ADC1_IRQHandler(void) {
    if ((REG32(ADC_SR) & ADC_SR_EOC) != 0u) {
        g_adc_irq_raw = (uint16_t)(REG32(ADC_DR) & 0xFFFFu);
        REG32(ADC_CR1) &= ~ADC_CR1_EOCIE;
        g_adc_irq_busy = false;
        g_adc_irq_ready = true;
    }
}

bool analog_read_adc_channel(uint8_t channel, uint16_t *raw) {
    if (raw == 0 || channel > 18u) {
        return false;
    }
    if (g_adc_irq_busy) {
        return false;
    }
    g_adc_irq_ready = false;
    REG32(ADC_CR1) &= ~ADC_CR1_EOCIE;

    if (channel <= 7u) {
        gpio_config_pin(GPIOA_BASE, channel, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
    }

    if (!adc_enable_clock_and_hsi()) {
        return false;
    }

    REG32(ADC_CR1) &= 0xFCFFFEFFu;
    REG32(ADC_CR2) &= 0xC0FFF7FDu;
    REG32(ADC_CR2) |= ADC_CR2_ADON;
    uint32_t timeout = 100000u;
    while (((REG32(ADC_SR) & ADC_SR_ADONS) == 0u) && timeout-- != 0u) {
    }
    if (timeout == 0u) {
        return false;
    }

    adc_config_channel(channel);
    REG32(ADC_SR) &= ~ADC_SR_EOC;
    REG32(ADC_CR2) |= ADC_CR2_SWSTART;

    timeout = 100000u;
    while (((REG32(ADC_SR) & ADC_SR_EOC) == 0u) && timeout-- != 0u) {
    }
    if (timeout == 0u) {
        return false;
    }

    *raw = (uint16_t)(REG32(ADC_DR) & 0xFFFFu);
    return true;
}
