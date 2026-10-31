#include "fault_diag.h"

#include "hw.h"

#ifdef EEV_FAULT_DIAG_HOST_TEST
static volatile fault_diag_record_t g_fault_diag;
#else
static volatile fault_diag_record_t g_fault_diag
    __attribute__((section(".noinit.fault_diag"), used));
#endif

static uint32_t fault_diag_mix(uint32_t hash, uint32_t value) {
    hash ^= value;
    hash *= 16777619u;
    return hash;
}

uint32_t fault_diag_record_checksum(const volatile fault_diag_record_t *record) {
    if (record == 0) {
        return 0u;
    }

    uint32_t hash = 2166136261u;
    hash = fault_diag_mix(hash, record->magic);
    hash = fault_diag_mix(hash, record->version);
    hash = fault_diag_mix(hash, record->size);
    hash = fault_diag_mix(hash, record->sequence);
    hash = fault_diag_mix(hash, record->kind);
    hash = fault_diag_mix(hash, record->exception_number);
    hash = fault_diag_mix(hash, record->r0);
    hash = fault_diag_mix(hash, record->r1);
    hash = fault_diag_mix(hash, record->r2);
    hash = fault_diag_mix(hash, record->r3);
    hash = fault_diag_mix(hash, record->r12);
    hash = fault_diag_mix(hash, record->lr);
    hash = fault_diag_mix(hash, record->pc);
    hash = fault_diag_mix(hash, record->xpsr);
    hash = fault_diag_mix(hash, record->cfsr);
    hash = fault_diag_mix(hash, record->hfsr);
    hash = fault_diag_mix(hash, record->dfsr);
    hash = fault_diag_mix(hash, record->afsr);
    hash = fault_diag_mix(hash, record->mmfar);
    hash = fault_diag_mix(hash, record->bfar);
    return hash;
}

bool fault_diag_record_valid(const volatile fault_diag_record_t *record) {
    return record != 0 &&
           record->magic == FAULT_DIAG_MAGIC &&
           record->commit == FAULT_DIAG_COMMIT &&
           record->version == FAULT_DIAG_VERSION &&
           record->size == sizeof(*record) &&
           record->checksum == fault_diag_record_checksum(record);
}

const volatile fault_diag_record_t *fault_diag_get(void) {
    if (!fault_diag_record_valid(&g_fault_diag)) {
        return 0;
    }
    return &g_fault_diag;
}

void fault_diag_clear(void) {
    g_fault_diag.commit = 0u;
    g_fault_diag.magic = 0u;
#ifndef EEV_FAULT_DIAG_HOST_TEST
    cpu_dsb_isb();
#endif
}

static void fault_diag_store(uint32_t kind,
                             uint32_t exception_number,
                             const uint32_t *stacked_regs) {
    uint32_t sequence = 1u;
    if (fault_diag_record_valid(&g_fault_diag)) {
        sequence = g_fault_diag.sequence + 1u;
    }

    g_fault_diag.commit = 0u;
    g_fault_diag.magic = FAULT_DIAG_MAGIC;
    g_fault_diag.version = FAULT_DIAG_VERSION;
    g_fault_diag.size = sizeof(g_fault_diag);
    g_fault_diag.sequence = sequence;
    g_fault_diag.kind = kind;
    g_fault_diag.exception_number = exception_number;
    g_fault_diag.r0 = stacked_regs != 0 ? stacked_regs[0] : 0u;
    g_fault_diag.r1 = stacked_regs != 0 ? stacked_regs[1] : 0u;
    g_fault_diag.r2 = stacked_regs != 0 ? stacked_regs[2] : 0u;
    g_fault_diag.r3 = stacked_regs != 0 ? stacked_regs[3] : 0u;
    g_fault_diag.r12 = stacked_regs != 0 ? stacked_regs[4] : 0u;
    g_fault_diag.lr = stacked_regs != 0 ? stacked_regs[5] : 0u;
    g_fault_diag.pc = stacked_regs != 0 ? stacked_regs[6] : 0u;
    g_fault_diag.xpsr = stacked_regs != 0 ? stacked_regs[7] : 0u;
#ifndef EEV_FAULT_DIAG_HOST_TEST
    g_fault_diag.cfsr = REG32(SCB_CFSR);
    g_fault_diag.hfsr = REG32(SCB_HFSR);
    g_fault_diag.dfsr = REG32(SCB_DFSR);
    g_fault_diag.afsr = REG32(SCB_AFSR);
    g_fault_diag.mmfar = REG32(SCB_MMFAR);
    g_fault_diag.bfar = REG32(SCB_BFAR);
#else
    g_fault_diag.cfsr = 0u;
    g_fault_diag.hfsr = 0u;
    g_fault_diag.dfsr = 0u;
    g_fault_diag.afsr = 0u;
    g_fault_diag.mmfar = 0u;
    g_fault_diag.bfar = 0u;
#endif
    g_fault_diag.checksum = fault_diag_record_checksum(&g_fault_diag);
#ifndef EEV_FAULT_DIAG_HOST_TEST
    __asm__ volatile("dmb" ::: "memory");
#endif
    g_fault_diag.commit = FAULT_DIAG_COMMIT;
#ifndef EEV_FAULT_DIAG_HOST_TEST
    cpu_dsb_isb();
#endif
}

__attribute__((noreturn))
static void fault_diag_reset(void) {
#ifndef EEV_FAULT_DIAG_HOST_TEST
    cpu_disable_irq();
    REG32(SCB_AIRCR) = 0x05FA0004u;
    cpu_dsb_isb();
#endif
    while (1) {
    }
}

void fault_diag_capture_hardfault(const uint32_t *stacked_regs) {
    fault_diag_store(FAULT_DIAG_KIND_HARDFAULT, 3u, stacked_regs);
    fault_diag_reset();
}

void fault_diag_capture_exception(uint32_t exception_number) {
    fault_diag_store(FAULT_DIAG_KIND_EXCEPTION, exception_number, 0);
    fault_diag_reset();
}
