#ifndef SYLE_FIRMWARE_ACCESSED_CAS_H
#define SYLE_FIRMWARE_ACCESSED_CAS_H
#include "firmware_dirty_transaction.h"
/* Caller proves aligned WB owned writable RAM and exclusive stable snapshot.
 * One exact A-only compare-exchange; D is unchanged, no ownership inference,
 * no retry/rollback. Independent from the D-only primitive. */
enum fwd_cas_result faa_atomic_cas(uint64_t*,uint64_t expected,uint64_t desired);
#endif
