#ifndef SYLE_LINUX_VCPU_EVENT_H
#define SYLE_LINUX_VCPU_EVENT_H
#include <stdbool.h>
#include <stdint.h>
/* Reconstructed pure policy, not wired to a runnable VM. Caller must limit
 * to the pinned compressed Linux phase and reject repeated fault tuples. */
struct lve_pf_exit { uint64_t reason,qualification,vectoring_info,rip;
 uint64_t interruption_info,interruption_error,entry_info,interruptibility;
 uint64_t idt_base,idt_limit; };
struct lve_pf_entry { uint64_t info,error,instruction_length,cr2,rip; };
bool lve_demand_pf(const struct lve_pf_exit *,struct lve_pf_entry *);
#endif
