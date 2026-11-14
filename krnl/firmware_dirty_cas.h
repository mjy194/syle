#ifndef SYLE_FIRMWARE_DIRTY_CAS_H
#define SYLE_FIRMWARE_DIRTY_CAS_H
#include "firmware_dirty_transaction.h"
/* No ownership inference: caller must first prove a stable WB writable mapped
 * owned RAM entry, 8-byte aligned, entire snapshot exclusive and unchanged.
 * Host-testable one-attempt primitive; nondefault cache fixture only.
 * Only sets D in an exact matched value; no retries, no blind OR or rollback. */
enum fwd_cas_result fwd_atomic_cas(uint64_t *verified_entry,uint64_t expected,uint64_t desired);
#endif
