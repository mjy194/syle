#ifndef SYLE_LINUX_PROFILE_ACTIVATION_H
#define SYLE_LINUX_PROFILE_ACTIVATION_H
#include "linux_profile_gate.h"
#include "linux_guest_session.h"
struct lpa_state {
 enum lcp_kind kind; bool active,failed;
 struct lgs_session *session; struct lad_config config;
 struct sg_memory_ops owner;
 struct guest_apic *apic;struct ioapic *io;
 void *host_ctx;bool (*vmwrite)(void*,uint32_t,uint64_t);
 void *read_ctx;bool (*read_field)(void*,uint32_t,uint64_t*);
 struct lvp_cpu_features cpu;unsigned physical_bits,timer_shift;
 uint64_t domain,eptp,last_time;
};
/* Fresh zero state per destroyed domain. False is sticky; never fall back.
 * Caller owns serialization through entry. No allocation/install/VMX here. */
bool lpa_before(struct lpa_state*,const struct lad_config*,struct lgs_session*,const struct lvp_cpu_features*);
#endif
