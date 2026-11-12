#ifndef SYLE_LINUX_APIC_MSR_H
#define SYLE_LINUX_APIC_MSR_H
#include <guest_apic.h>
enum lams_result { LAMS_REJECTED=-1,LAMS_FATAL=-2,LAMS_COMMITTED=0,LAMS_EFFECT_PENDING=1 };
typedef bool (*lams_notify)(void*,uint64_t domain,uint64_t sequence,uint8_t vector);
struct lams_state {
 struct guest_apic *model;
 void *sink;
 lams_notify notify;
 uint64_t domain,sequence;
 struct guest_apic_effect effect;
 bool ready,busy,pending,failed;
};
/* Fresh zero object. domain is a nonzero unique guest-domain generation, never
 * reused while any sink receipt can survive. No model/device initialization.
 * notify MUST deduplicate (domain,sequence) durably within that domain: false
 * may mean already applied, so retry must not EOI a newly delivered interrupt.
 * Plain ioapic_eoi(vector) is NOT a suitable callback. */
bool lams_init(struct lams_state*,struct guest_apic*,uint64_t,void*,lams_notify);
/* Caller proves exclusive APIC/device ownership, no inflight IRQ reservation,
 * real RDMSR/WRMSR exit, no vectoring/pending injection, host restored, and
 * coherent guest CPU/platform exposure. basic bits are guest-visible CPUID.01.
 * now uses the device's explicit synthetic timer domain, NOT calibrated time.
 * Native GPR order AX BX CX DX ...; ECX/EDX/EAX architectural low32 semantics.
 * COMMITTED/PENDING both advanced RIP: NEVER retry this instruction.
 * FATAL (possibly partial VMWRITE) permanently forbids guest resume. */
enum lams_result lams_apply(struct lams_state*,uint64_t[15],uint64_t rip,
 unsigned cpl,uint32_t reason,unsigned length,uint64_t now,
 uint32_t basic_ecx,uint32_t basic_edx,bool exclusive,
 void*,bool(*)(void*,uint32_t,uint64_t));
/* Only repeats notification; no model/GPR/RIP/timer changes. */
enum lams_result lams_retry(struct lams_state*);
#endif
