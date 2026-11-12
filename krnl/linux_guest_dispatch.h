#ifndef SYLE_LINUX_GUEST_DISPATCH_H
#define SYLE_LINUX_GUEST_DISPATCH_H
#include "linux_vcpu_policy.h"
#include "linux_high_pf.h"
#include "linux_uart_runtime.h"
#include "linux_microcode_model.h"
#include "linux_irq_glue.h"
#include "linux_apic_dispatch.h"
#include "linux_profile_activation.h"
#include "linux_nonpresent_pf.h"
#include "linux_pit2.h"
#include "legacy_irq_runtime.h"
#include "pic0_ports.h"
struct lgs_session;struct lgm_service_ops;struct lgs_entry_result;
struct lgd_config {
 bool legacy_pic_enabled; /* Opt-in, exclusive with APIC/lia. */
 struct lvp_cpu_features cpu;
 struct lad_config apic; /* Optional entire guest_apic domain; never mix lia. */
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
#include "linux_debug_disabled.h"
#include "linux_pic_mask.h"
#include "linux_rtc.h"
#include "linux_msr_gp.h"
#include "linux_port80.h"
struct lgd_context {
 struct lgp_state msr_gp;
 struct lp80_state post;
 struct lir legacy;struct pcp ports;uint64_t generation;bool legacy_initialized;
 struct ldd_state debug_disabled;
 struct lpic_state pic;
 struct lrtc_state rtc;
 struct lgd_config config;
 struct lpit2 pit;uint64_t ticks,staged_tick;bool clock_staged;
 struct lpa_state activation;
 struct lpfg_state pf;
 struct lnp_state demand_pf;
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
/* APIC synthetic clock is the same virtual TSC=1000*PITtick. */
bool lgd_apic_now(void*,uint64_t*);
bool lgd_init(struct lgd_context*,const struct lgd_config*,struct lgm_service_ops*);
bool lgd_reset(void*);
bool lgd_before_entry(void*,struct lgs_session*);
bool lgd_after_entry(void*,struct lgs_session*,const struct lgs_entry_result*);
bool lgd_dispatch(void*,struct lgs_session*,uint32_t);
int lgd_send(void*,uint8_t);
int lgd_recv(void*);
/* Default profile remains APIC-hidden. An explicitly configured entire APIC
 * domain starts inactive: only the first before_entry evidence gate activates
 * the experimental profile, and every entry revalidates controls/EPT/identity.
 * SEND/RECV return busy until activation for this experimental domain only;
 * default polling SEND semantics are unchanged. No claim of Linux acceptance.
 * reset requires complete guest resource destruction and reconciled devices;
 * owner must initialize a fresh domain (new nonzero generation ID) per CREATE.
 * STOP/START cannot discard inflight IRQ ownership or sticky failure.
 * Session owns per-entry VMX timer. This layer adds a per-CREATE total-exit cap.
 * Unknown or failed transactions latch failure; never resume until DESTROY.
 */
#endif
