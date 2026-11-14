#ifndef SYLE_LINUX_NONPRESENT_PF_H
#define SYLE_LINUX_NONPRESENT_PF_H
#include "linux_mmio_memory.h"
#define LNP_MAX_FAULTS 32u
struct lnp_tuple {uint64_t rip,address,cr3,error;};
struct lnp_state {struct lnp_tuple history[LNP_MAX_FAULTS];unsigned count;bool pending,failed;};
struct lnp_exit {
 uint64_t reason,info,error,address,rip,rsp,idtr,idt_limit,gdtr,gdt_limit;
 uint64_t event,entry_error,entry_length,vectoring,activity,blocking;
};
/* Supervisor data #PF only: exact error0/2, nonpresent owned guest page walk,
 * flat long mode, same-CPL interrupt gate IST0. Never repairs page tables. */
bool lnp_validate(const struct lnp_state*,const struct lmm_memory*,const struct lnp_exit*);
bool lnp_observe(const struct lnp_state*,uint64_t,uint64_t,uint64_t,struct lnp_state*);
/* Narrow receipt for a distinct intercepted #PF following an injected #PF.
 * Valid-bit clearing alone is NOT completion evidence. Exact new #PF metadata,
 * no IDT vectoring, and matching old entry error/length are also required. */
struct lnp_pf_receipt {uint64_t reason,info,error,vector,event,entry_error,entry_length;};
bool lnp_observe_pf(const struct lnp_state*,const struct lnp_pf_receipt*,struct lnp_state*);
struct lgs_session;
bool lnp_inject(struct lnp_state*,struct lgs_session*);
#endif
