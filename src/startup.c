#include <stdint.h>

#include "bluetooth.h"
#include "clock.h"
#include "hw.h"
#include "uart_update.h"

extern uint32_t __data_load;
extern uint32_t __data_start;
extern uint32_t __data_end;
extern uint32_t __ramfunc_load;
extern uint32_t __ramfunc_start;
extern uint32_t __ramfunc_end;
extern uint32_t __bss_start;
extern uint32_t __bss_end;
extern uint32_t __stack_top;

void Reset_Handler(void);
void Reset_Handler_C(void);
void Default_Handler(void);
void HardFault_Handler(void);
void HardFault_Handler_C(uint32_t *stacked_regs);
void SysTick_Handler(void);
void ADC1_IRQHandler(void);
void TIM2_IRQHandler(void);
void TIM3_IRQHandler(void);
void USART2_IRQHandler(void);
int main(void);

__attribute__((section(".isr_vector"), used))
void (*const vector_table[96])(void) = {
    [0] = (void (*)(void))&__stack_top,
    [1] = Reset_Handler,
    [2] = Default_Handler,
    [3] = HardFault_Handler,
    [4 ... 14] = Default_Handler,
    [15] = SysTick_Handler,
    [16 ... 33] = Default_Handler,
    [34] = ADC1_IRQHandler,
    [35 ... 43] = Default_Handler,
    [44] = TIM2_IRQHandler,
    [45] = TIM3_IRQHandler,
    [46 ... 53] = Default_Handler,
    [54] = USART2_IRQHandler,
    [55 ... 95] = Default_Handler,
};

__attribute__((naked))
void Reset_Handler(void) {
    __asm__ volatile(
        "ldr r0, =__stack_top\n"
        "msr msp, r0\n"
        "cpsid i\n"
        "dsb\n"
        "b Reset_Handler_C\n"
    );
}

static void disable_inherited_interrupts(void) {
    REG32(SYST_CSR) = 0;
    for (uint32_t i = 0; i < 8; ++i) {
        REG32(NVIC_ICER0 + i * 4u) = 0xFFFFFFFFu;
        REG32(NVIC_ICPR0 + i * 4u) = 0xFFFFFFFFu;
    }
    REG32(SCB_VTOR) = APP_FLASH_BASE;
    cpu_dsb_isb();
}

void Reset_Handler_C(void) {
    disable_inherited_interrupts();

    uint32_t *src = &__data_load;
    for (uint32_t *dst = &__data_start; dst < &__data_end;) {
        *dst++ = *src++;
    }

    src = &__ramfunc_load;
    for (uint32_t *dst = &__ramfunc_start; dst < &__ramfunc_end;) {
        *dst++ = *src++;
    }

    for (uint32_t *dst = &__bss_start; dst < &__bss_end;) {
        *dst++ = 0;
    }

    (void)main();
    while (1) {
    }
}

void Default_Handler(void) {
    while (1) {
    }
}

__attribute__((naked))
void HardFault_Handler(void) {
    __asm__ volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b HardFault_Handler_C\n"
    );
}

void HardFault_Handler_C(uint32_t *stacked_regs) {
    if (uart_update_hardfault_recover(stacked_regs)) {
        return;
    }
    Default_Handler();
}

void SysTick_Handler(void) {
    clock_systick_irq();
}

void TIM2_IRQHandler(void) {
    Default_Handler();
}

void TIM3_IRQHandler(void) {
    Default_Handler();
}

void USART2_IRQHandler(void) {
    bluetooth_irq_handler();
}
