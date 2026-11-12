#ifndef SYLE_LINUX_GUEST_DISPATCH_H
#define SYLE_LINUX_GUEST_DISPATCH_H
#include "linux_vcpu_policy.h"
#include "linux_high_pf.h"
#include "linux_uart_runtime.h"
#include "linux_microcode_model.h"
#include "linux_irq_glue.h"
struct lgs_session;struct lgm_service_ops;struct lgs_entry_result;
struct lgd_config {
 struct lvp_cpu_features cpu;
 struct lia *irq_adapter;
 bool irq_delivery_enabled,irq_platform_ready; /* Default false. Platform-ready
  * requires coherent guest APIC MSRs/MMIO/firmware routing, not just a sink. */
 uint32_t max_basic,vendor_b,vendor_c,vendor_d,max_extended;
 uint64_t max_exits; /* 1..2^20 observed-exit hard cap, including timer yields.
  * Exit N is counted then fails closed before reads/writes/emulation;
  * N=1 observes first exit only. False forbids session entry N+1. */
 bool fixed_input_verified; /* Trusted build SHA check of immutable fixed inputs;
  * NOT inferred from guest RIP. False disables all pinned PF reinjection. */
 void *irq_ctx;
 bool (*sync_irq)(void*,unsigned,bool); /* explicit logical sink, no fake APIC */
};
struct lgd_context {
 struct lgd_config config;
 struct lpfg_state pf;
 struct lmc_state microcode;
 struct lig_state irq;
 bool entry_inflight;
 enum lhp_state high;
 struct linux_uart_runtime uart;
 struct lgs_session *session;
 uint64_t exits;
 bool configured,ready,failed,busy;
};
/* Fresh zero object; publishes callbacks only after validation. Manager owns
 * access control and calls reset before every CREATE, never on live resources.
 * Reset retains immutable config; clears all PF/highPF/UART/CPU runtime state.
 * No VM, host MSR/port access, printing, or allocator. */
bool lgd_init(struct lgd_context*,const struct lgd_config*,struct lgm_service_ops*);
bool lgd_reset(void*);
bool lgd_before_entry(void*,struct lgs_session*);
bool lgd_after_entry(void*,struct lgs_session*,const struct lgs_entry_result*);
bool lgd_dispatch(void*,struct lgs_session*,uint32_t);
int lgd_send(void*,uint8_t);
int lgd_recv(void*);
/* External IRQ transaction hooks are opt-in; current CPUID hides APIC and
 * rejects enabling them. Thus Linux IRQ delivery is NOT available. sync_irq retains/publishes device
 * logical level only. A true sink return is level ownership, NOT guest delivery.
 * CPUID continues to hide APIC. UART polling is supported;
 * no claim that IRQ-driven Linux works. No implicit device clock advances. Reset cannot discard an IRQ token: after
 * full guest+LAPIC/IOAPIC domain destruction, caller must reinitialize the
 * adapter before CREATE; merely STOP is not sufficient.
 * Session owns per-entry VMX timer. This layer adds a per-CREATE total-exit cap.
 * Unknown or failed transactions latch failure; never resume until DESTROY.
 */
#endif
