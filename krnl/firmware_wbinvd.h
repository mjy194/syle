#ifndef SYLE_FIRMWARE_WBINVD_H
#define SYLE_FIRMWARE_WBINVD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct fwb_state { bool failed; };
/* Host-only transaction skeleton. Contains NO native WBINVD or default cache
 * maintenance implementation. CPU support is NOT exclusive cache-domain
 * authority. admitted_cache_contract must come from an external scoped
 * maintenance/ownership contract; it cannot be inferred from CPUID/VMX alone.
 * Caller supplies immutable safely fetched bytes. Exact WBINVD: exit54/q0,
 * len2/0f09/CPL0/canonical RIP+2. GPRs are not accessed.
 * !exclusive leaves all state untouched; otherwise any validation/callback
 * failure permanently latches failed. maintain(false) may have partial effects;
 * RIP write failure after maintenance cannot be rolled back or retried.
 * Order: validate, pre-sample, maintenance, post-sample, RIP commit.
 * Sample callbacks synchronize the admitted clock/event contract; neither
 * sample callback may be omitted or treated as optional success.
 * The caller must not resume on any failure. No current native admission is
 * established by this module or its tests. Fresh zeroed state required. */
bool fwb_apply(struct fwb_state*,uint32_t reason,uint64_t qualification,
 uint64_t rip,unsigned length,unsigned cpl,const uint8_t *instruction,size_t available,
 bool exclusive,bool admitted_cache_contract,void *ctx,
 bool (*sample)(void*),bool (*maintain)(void*),bool (*write)(void*,uint32_t,uint64_t));
#endif
