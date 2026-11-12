#ifndef SYLE_LINUX_VCPU_STATE_H
#define SYLE_LINUX_VCPU_STATE_H
#include <stdbool.h>
#include <stdint.h>
#define LVS_MSR_COUNT 6u
extern const uint32_t lvs_msr_ids[LVS_MSR_COUNT];
struct lvs_ops {
 void *ctx;
 bool (*save_fx)(void *,uint8_t *);
 bool (*load_fx)(void *,const uint8_t *);
 bool (*read_msr)(void *,uint32_t,uint64_t *);
 bool (*write_msr)(void *,uint32_t,uint64_t);
 bool (*enter)(void *);
};
struct lvs_image { _Alignas(16) uint8_t fx[512]; uint64_t msr[LVS_MSR_COUNT]; };
struct lvs_state {
 struct lvs_image host,guest;
 struct lvs_ops owner;
 bool ready,host_dirty,guest_valid;
};
enum lvs_result { LVS_BAD_STATE,LVS_CAPTURE_FAILED,LVS_ENTRY_FAILED,
 LVS_GUEST_CAPTURE_FAILED,LVS_HOST_DIRTY,LVS_EXIT };
/* Fresh zeroed, 16-byte aligned object. No privilege or hardware operations.
 * Caller pins one CPU, disables IRQs, prevents concurrent/reentrant access,
 * and compiles all reachable code general-registers-only/no-red-zone.
 * Callbacks may partially mutate before reporting failure. save/read must
 * not mutate the architectural state they capture. enter returns true only
 * after a VM exit; false guarantees no guest instruction executed. All buffers
 * stay resident. This layer cannot validate privileged backend behavior.
 * VMCS owns EFER/PAT, FS/GS bases, SYSENTER and DEBUGCTL separately.
 * XSAVE/AVX/PKU/CET/AMX and other unpreserved state must not be exposed.
 */
bool lvs_init(struct lvs_state *);
/* HOST_DIRTY requires retry with identical backend or fatal stop; no ordinary
 * host continuation or guest reentry. A failed guest capture invalidates guest.
 * ENTRY_FAILED never implies successful guest execution. */
enum lvs_result lvs_run(struct lvs_state *,const struct lvs_ops *);
bool lvs_restore_host(struct lvs_state *,const struct lvs_ops *);
#endif
