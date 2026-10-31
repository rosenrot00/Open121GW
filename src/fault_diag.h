#ifndef EEVBLOG_FAULT_DIAG_H
#define EEVBLOG_FAULT_DIAG_H

#include <stdbool.h>
#include <stdint.h>

#define FAULT_DIAG_MAGIC 0x4641554Cu
#define FAULT_DIAG_COMMIT 0xA5C39E71u
#define FAULT_DIAG_VERSION 1u

typedef enum {
    FAULT_DIAG_KIND_HARDFAULT = 1u,
    FAULT_DIAG_KIND_EXCEPTION = 2u,
} fault_diag_kind_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t sequence;
    uint32_t kind;
    uint32_t exception_number;
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t dfsr;
    uint32_t afsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint32_t checksum;
    uint32_t commit;
} fault_diag_record_t;

uint32_t fault_diag_record_checksum(const volatile fault_diag_record_t *record);
bool fault_diag_record_valid(const volatile fault_diag_record_t *record);
const volatile fault_diag_record_t *fault_diag_get(void);
void fault_diag_clear(void);

__attribute__((noreturn))
void fault_diag_capture_hardfault(const uint32_t *stacked_regs);

__attribute__((noreturn))
void fault_diag_capture_exception(uint32_t exception_number);

#endif
