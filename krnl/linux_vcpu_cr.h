#ifndef SYLE_LINUX_VCPU_CR_H
#define SYLE_LINUX_VCPU_CR_H
#include <stdbool.h>
#include <stdint.h>
struct lvc_state {
 /* Native order RAX RBX RCX RDX RSI RDI RBP R8..R15, NOT Intel encoding. */
 uint64_t gpr[15];
 uint64_t rsp,rip,cr0,cr3,cr4,cr0_shadow,cr4_shadow;
};
struct lvc_limits { uint64_t cr0_fixed0,cr0_fixed1,cr4_fixed0,cr4_fixed1,ram_size; };
enum {LVC_GPR=1,LVC_RSP=2,LVC_CR0=4,LVC_CR3=8,LVC_CR4=16};
struct lvc_change {struct lvc_state next;unsigned changed;};
/* Pure staged MOV CR0/3/4. Caller verifies genuine exit28/no event collision.
 * Unsupported CLTS/LMSW reject. Failure leaves output unchanged. Apply CR and
 * shadow together, RIP last; any partial VMWRITE failure must STOP. No VPID
 * profile: caller ensures required translation invalidation. CR3 supports only
 * 128MiB private RAM+PWT/PCD, no PCID/no-flush. Does not inspect page contents.
 * CR4 actual differs from visible shadow only by hidden VMXE. */
bool lvc_emulate(const struct lvc_state *,const struct lvc_limits *,uint64_t,unsigned,struct lvc_change *);
#endif
