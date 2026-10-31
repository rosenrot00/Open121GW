#include <assert.h>

#include "fault_diag.h"

static fault_diag_record_t valid_record(void) {
    fault_diag_record_t record = {0};
    record.magic = FAULT_DIAG_MAGIC;
    record.version = FAULT_DIAG_VERSION;
    record.size = sizeof(record);
    record.sequence = 4u;
    record.kind = FAULT_DIAG_KIND_HARDFAULT;
    record.exception_number = 3u;
    record.pc = 0x08012345u;
    record.lr = 0x08006789u;
    record.cfsr = 0x00008200u;
    record.commit = FAULT_DIAG_COMMIT;
    record.checksum = fault_diag_record_checksum(&record);
    return record;
}

int main(void) {
    fault_diag_record_t record = valid_record();
    assert(fault_diag_record_valid(&record));

    record.pc ^= 2u;
    assert(!fault_diag_record_valid(&record));

    record = valid_record();
    record.commit = 0u;
    assert(!fault_diag_record_valid(&record));

    fault_diag_clear();
    assert(fault_diag_get() == 0);
    return 0;
}
