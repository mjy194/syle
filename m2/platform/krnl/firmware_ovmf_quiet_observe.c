/* NONDEFAULT quiet-observer A/B; same timer/exit budgets.
 * Deferred diagnostic bytes are dumped only after guest execution stops. */
/* NONDEFAULT timer IRQ experiment; never a replacement for the frozen cache observer. */
#ifndef SYLE_FIRMWARE_IRQ_FIXTURE
#error "Explicit IRQ fixture required"
#endif
#include "firmware_timer_irq_session_import.h"
#include "firmware_timer_irq_delivery.h"
#include "firmware_irq_window.h"
#include "firmware_irq_apic.h"
#include "firmware_irq_cr8.h"
#include "firmware_irq_cr8_write.h"
#include "firmware_irq_lvt_same.h"
#include "firmware_cpu_mode_de.h"
#include "firmware_active_cpu_de.h"
#include "firmware_active_cpu_tsc.h"
#include "firmware_boot_cpu.h"
#define fcats_query fboot_query
#define fcp_efer_allowed fboot_efer_allowed
#include "firmware_tsc_read.h"
#include "firmware_observer_stack.h"
#include "firmware_debug_read.h"
static struct fdr_bank debug_bank;
#ifdef SYLE_M2_NESTED
#include "nested_vmx.h"
#include "linux_vcpu_prepare.h"
#include "linux_vcpu_state.h"
#include "nested_ept.h"
#include "nested_ept_sparse.h"
static bool nested_enabled;
struct nested_page_budget {uint64_t vmcs;unsigned pages;};
#ifdef SYLE_M2_DUAL_VCPU
#define NESTED_L1_CPUS 2
#else
#define NESTED_L1_CPUS 1
#endif
#ifdef SYLE_TRACE_L2_CLOCK
struct nested_timer_counters {
 uint64_t l1_expired,l1_late_ticks,l1_max_late,l1_batched,l1_accept,l1_reflect;
 uint64_t l2_submit,l2_deliver,l1_vmx_expired;
};
static struct nested_timer_counters nested_timer_counters[2];
#endif
struct nested_l1_context {
 struct nvmx vmcs, parked_vmcs;
 uint64_t last_field;
 struct fcm_state *l1_mode;
 struct lvn_context *l1_native;
 struct lvs_state *l1_state;
 struct lvs_ops *l1_ops;
 struct fbd_bank *debug;
 uint64_t *l1_page;
 struct {
  uint64_t guest_cr0,guest_cr3,guest_cr4,eptp12,entry_count,exit_count,store_count;
  uint64_t reason,qualification,gpa,vm_instruction_error;
  uint64_t walk_status,fill_status,attempt,vectoring,exit_rip;
  uint64_t stage,field,en,sec,event,rip,link;
  uint8_t interrupt_vector;
  bool entered,long_mode,unrestricted,suspended;
#ifdef SYLE_NESTED_VMX_TIMER
  uint64_t timer_deadline;
  bool timer_enabled;
#endif
#ifdef SYLE_TRACE_L2_CLOCK
  bool trace_timer_pending;
#endif
#ifdef SYLE_M2_HOST_PARALLEL
  bool prefetched;
  enum lvs_result outcome;
#endif
 } entry;

 struct nested_ept_sparse composed;
 struct nested_page_budget composed_budget;
 uint64_t composed_source;
 /* Both physical VMCS pages stay pinned to the same CPU. A virtual
  * VMCLEAR, VMXOFF or guest teardown ends the VMCS02 lifetime. */
 struct {
  uint64_t page,fields[LVP_MAX_FIELDS];
  bool launched;
 } cpu;
 struct {
  uint64_t samples,begin,elapsed,over_period,vmx_ops,ops_begin,ops_total;
  bool pending,eoi;
 } irq0_cost;
};
static struct nested_l1_context nested_contexts[NESTED_L1_CPUS];
/* The pinned execution CPU switches this owner only across L1 run slots. */
static struct nested_l1_context *nested_current=nested_contexts;
#endif
#ifdef SYLE_FIRMWARE_DISK
static bool disk_pic_active,disk_pic_window;
static bool disk_tick(void);
static bool disk_cs_identity(uint64_t,uint64_t);
#else
#define disk_pic_active false
#define disk_pic_window false
#define disk_tick() true
#endif
static struct ftis_session irq_session;
static struct ftirt_route irq_route;
static struct fiw_state irq_window;
static bool irq_identity(void);
/* NONDEFAULT fixed QEMU/KVM cache experiment. Derived from the frozen
 * read-only observer; no production/default target references this file. */
#ifndef SYLE_CACHE_KVM_FIXTURE
#error "Scoped KVM fixture target required"
#endif
#include "firmware_wbinvd.h"
#include "firmware_tpm_transaction.h"
#include "firmware_vars_rom.h"
#ifdef SYLE_FIRMWARE_CODE_ROM
#include "firmware_code_rom.h"
#endif
#ifdef SYLE_FIRMWARE_PAUSE
#include "firmware_pause.h"
static struct fpa_state pause_state;
#ifdef SYLE_FIRMWARE_HALT
#include "firmware_halt.h"
static struct fhl_state halt_state;
static bool halt_supported;
static uint64_t halt_eois,halt_last_eoi_wake;
#endif
#ifndef FPM_DELAY_EXIT_CAP
#error "Use the isolated firmware-pm-delay target"
#endif
_Static_assert(FPM_DELAY_EXIT_CAP==6144 || FPM_DELAY_EXIT_CAP==32768 || FPM_DELAY_EXIT_CAP==131072 || FPM_DELAY_EXIT_CAP==1048576,"bounded diagnostic profiles only");
static bool pm_delay_seen,pm_delay_quiet;
static uint64_t pm_delay_target,pm_delay_reads,pm_delay_pauses,pm_delay_quiet_exits;
static bool pm_delay_report;
#endif
#include "guest_mmio_mov.h"
#include "firmware_vars_dirty_transaction.h"
#include "firmware_dirty_cas.h"
#include "firmware_accessed_cas.h"
#include "firmware_apic_accessed_transaction.h"
#include "firmware_apic_svr_dirty_transaction.h"
static struct fsdt_state svr_dirty;
static struct faat_state apic_accessed;
static struct fvdt_state vars_dirty;
static struct fvr_state vars_rom;
#ifdef SYLE_FIRMWARE_CODE_ROM
static struct fcode_state code_rom;
#endif
static struct ftt_state tpm_absent_transaction;
extern bool fwb_kvm_environment(void);
extern bool fwb_kvm_maintain(void);
static struct fwb_state cache_transaction;
static bool cache_attempted;
#include "firmware_lint0_quiet.h"
#include "firmware_platform_id.h"
#include "firmware_fwcfg_transaction.h"
#include "firmware_piix_pm.h"
#include "firmware_preemption_resume.h"
#include "firmware_cmos.h"
#include "firmware_cmos_index_read.h"
#include "firmware_cmos_alarm_read.h"
#include "firmware_cmos_rate_same.h"
#include "firmware_cmos_valid_write.h"
#include "linux_host_clock_hv.h"
#include "firmware_vcpu_host.h"
#include "firmware_cpu_mode.h"
#include "firmware_cpu_profile.h"
#include "firmware_pci_probe.h"
#include "linux_pic_mask.h"
#include "firmware_active_cpu.h"
#include "firmware_clock_apic_transaction.h"
#include "host_clock_cpuid_diag.h"
#include "linux_vcpu_backend.h"
#include "linux_vcpu_native.h"
#include "memory.h"
#include "paging.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
extern const unsigned char firmware_probe_start[],firmware_probe_end[];
static struct firmware_memory mem;
static struct lvb_context backend;
static struct lvh_context host;
static struct lvn_context native;
static struct lvs_state state;
static struct lvs_ops state_ops;
static struct lvh_ops ops;
static struct fcm_state mode;
static struct fcp_host cpu;
static struct fpp_state pci;
static struct fpid_state platform_id;
static struct fcr_state cmos;
static struct fcri_guard cmos_index_guard;
static struct fcra_guard cmos_alarm_guard;
static struct fcrs_guard cmos_rate_guard;
static struct fcrv_guard cmos_valid_guard;
static struct fwc_state fwcfg;
static struct lpic_state pic;
static struct fct_state active_apic;
static struct lhcr_source clock_source;
static struct ftsc_state virtual_tsc;
static unsigned tsc_commits,rtc_data_reads;
static bool rtc_first_stack_done;
static bool tsc_source_ready(void){return virtual_tsc.ready&&!virtual_tsc.failed&&
 clock_source.sample&&clock_source.properties.verified&&clock_source.properties.invariant&&
 clock_source.properties.hz==10000000&&virtual_tsc.cpu==clock_source.properties.cpu&&
 virtual_tsc.epoch==clock_source.properties.epoch;}
#include "firmware_observer_compact.h"
static struct foc_trace observation_trace;
static char observation_bytes[2u*1024u*1024u]; /* NOBITS: checked against8MiB loader span. */
static bool observation_deferred,observation_finished;
static void fmg_emit(uint8_t);
static bool fmg_input(void);
/* Original runtime logger has no transmission acknowledgement. This true
 * means callback returned, not a guarantee that every UART byte arrived. */
static bool observation_sink(void*unused,const char*bytes,size_t count){
 (void)unused;char chunk[129];
 for(size_t off=0;off<count;){size_t n=count-off;if(n>128)n=128;
  for(size_t i=0;i<n;i++){chunk[i]=bytes[off+i];}
  chunk[n]=0;log_text(chunk);off+=n;}
 return true;
}
static bool console_sample_quiet;
static void observation_text(const char*s){if(console_sample_quiet)return;if(observation_deferred){size_t n=0;while(s[n])n++;(void)foc_text(&observation_trace,s,n);}else log_text(s);}
static void observation_hex(uint64_t value){if(console_sample_quiet)return;if(observation_deferred)(void)foc_hex(&observation_trace,value);else log_hex(value);}
static bool observation_start(void){
 if(!foc_defer(&observation_trace))return false;
 observation_deferred=true;return true;
}
static bool observation_ok(void){return observation_trace.ready&&!observation_trace.failed&&!observation_finished;}
static void observation_finish(void){
#ifdef SYLE_FIRMWARE_CONSOLE
 extern void log_serial_drain_mode(void);log_serial_drain_mode();
#endif
 if(observation_finished)return;
 observation_finished=true;observation_deferred=false;
 log_text("OVMF.TRACE guest_execution_stopped_deferred_dump_begin\n");
 (void)foc_flush(&observation_trace);
 log_text("OVMF.TRACE dump_end failed=");log_hex(observation_trace.failed);
 log_text(" incomplete=");log_hex(observation_trace.incomplete);
 log_text(" text_calls=");log_hex(observation_trace.text_calls);log_text(" hex_calls=");log_hex(observation_trace.hex_calls);
 log_text(" requested_bytes=");log_hex(observation_trace.requested_bytes);log_text(" accepted_logical_bytes=");log_hex(observation_trace.accepted_bytes);
 log_text(" sink_attempted_bytes=");log_hex(observation_trace.sink_attempted_bytes);log_text(" dropped_bytes=");log_hex(observation_trace.dropped_bytes);
 log_text(" encoded_bytes=");log_hex(observation_trace.encoded_bytes);
 log_text(" dictionary_entries=");log_hex(observation_trace.dict_count);
 log_text(" decoded_events=");log_hex(observation_trace.decoded_events);log_text(" decoded_bytes=");log_hex(observation_trace.decoded_bytes);
 log_text(" compact_v1_selected_records_only_same30s_sameVMslice_no_new_clock_sample\n");
}
static void observation_fatal(void){observation_deferred=false;log_text("OVMF.TRACE unsafe_host_fatal_no_drain_guarantee\n");}
#ifdef SYLE_FIRMWARE_CONSOLE
#include "firmware_console.h"
static struct fcon_state console_state;
static uint8_t console_output[65536];static unsigned console_count;
static void console_live_byte(uint8_t b){fmg_emit(b);}
#endif
#define log_text observation_text
#define log_hex observation_hex
#ifdef SYLE_NESTED_VMX_TIMER
#include "firmware_host_reference.inc"
#endif
static bool clock_init(void) {
 struct lhc_sample sample;
 if(!linux_host_clock_native_source(&clock_source)||clock_source.properties.hz!=10000000||
 !clock_source.properties.verified||!clock_source.properties.invariant||
 !clock_source.sample(clock_source.ctx,&sample))return false;
#ifdef SYLE_NESTED_VMX_TIMER
 if(!m2_hv_ref_start(&clock_source,sample.tsc))return false;
#endif
 active_apic.epoch=sample.epoch;active_apic.reference=sample.tsc;
 return ft100_mmio_init(&active_apic.clock,sample.epoch,sample.tsc,clock_source.max_gap);
}
static bool clock_sync(void) {
 struct lhc_sample sample={0};
 struct ft100 before=active_apic.clock.timer;
 const char*cause="already_failed";bool sampled=false;
 enum ft100_result timer_result=FT100_BAD;bool timer_called=false,pm_called=false;
 if(active_apic.failed)goto stop;
 cause="source_missing";if(!clock_source.sample)goto stop;
 cause="source_sample_failed";if(!clock_source.sample(clock_source.ctx,&sample))goto stop;
 sampled=true;cause="source_cpu_mismatch";if(sample.cpu!=clock_source.properties.cpu)goto stop;
 cause="source_epoch_mismatch";if(sample.epoch!=clock_source.properties.epoch)goto stop;
 cause="virtual_tsc_source_failed";
 if(!tsc_source_ready()||!ftsc_sample(&virtual_tsc,host.vmcs_page,sample.epoch,sample.cpu,sample.tsc))goto stop;
 active_apic.epoch=sample.epoch;active_apic.reference=sample.tsc;
 if(irq_session.ready){
  cause="irq_route_failed";
  if(!irq_identity()||!ftirt_sample(&irq_route,sample.tsc,0,0))goto stop;
 }else if(active_apic.clock.timer.lvt==0x20020&&active_apic.clock.timer.initial==10000000&&
          active_apic.clock.timer.divisor==1&&active_apic.clock.timer.armed){
  cause="irq_transfer_failed";
  if(!irq_identity()||!ftis_import(&irq_session,&irq_route,&active_apic.clock.timer,
      host.vmcs_page,host.vmcs_page,sample.epoch,sample.tsc,true))goto stop;
  if(!observation_start())goto stop;
  log_text("OVMF.IRQ old_timer_retired pending=");log_hex(irq_session.owner.pending);
  log_text(" deadline=");log_hex(irq_session.owner.deadline_bus);log_text("\n");
 }else {
 timer_called=true;timer_result=ft100_sample(&active_apic.clock.timer,sample.epoch,sample.tsc,0);
 cause=timer_result==FT100_STOP?"timer_unmasked_due":"timer_clock_or_shape_failed";
 if(timer_result!=FT100_OK)goto stop;
 }
 cause="pm_clock_or_shape_failed";
 if(pci.pm.ready){pm_called=true;if(!fpm_sample(&pci.pm,sample.epoch,sample.tsc))goto stop;}
 return true;
stop:
 observation_finish();
 active_apic.failed=true;if(irq_session.ready){irq_session.failed=true;fti_quarantine(&irq_session.owner);}else active_apic.clock.timer.terminal=true;
 log_text("OVMF.CLOCK_APIC stop_cause=");log_text(cause);
 log_text(" sampled=");log_hex(sampled);log_text(" sample_cpu=");log_hex(sample.cpu);
 log_text(" expected_cpu=");log_hex(clock_source.properties.cpu);
 log_text(" sample_epoch=");log_hex(sample.epoch);log_text(" expected_epoch=");log_hex(clock_source.properties.epoch);
 log_text(" sample_reference=");log_hex(sample.tsc);log_text(" source_maxgap=");log_hex(clock_source.max_gap);
 log_text(" timer_called=");log_hex(timer_called);log_text(" timer_result=");log_hex(timer_result);
 log_text(" pm_called=");log_hex(pm_called);log_text("\n");
 log_text("OVMF.CLOCK_APIC stop_timer_before epoch=");log_hex(before.epoch);
 log_text(" origin=");log_hex(before.origin);log_text(" last=");log_hex(before.last);
 log_text(" deadline_bus=");log_hex(before.deadline_bus);log_text(" period_bus=");log_hex(before.period_bus);
 log_text(" maxgap=");log_hex(before.max_gap);log_text(" initial=");log_hex(before.initial);
 log_text(" divisor=");log_hex(before.divisor);log_text(" lvt=");log_hex(before.lvt);
 log_text(" ready=");log_hex(before.ready);log_text(" armed=");log_hex(before.armed);log_text(" terminal=");log_hex(before.terminal);log_text("\n");
 log_text("OVMF.CLOCK_APIC stop_pm ready=");log_hex(pci.pm.ready);log_text(" failed=");log_hex(pci.pm.failed);
 log_text(" epoch=");log_hex(pci.pm.epoch);log_text(" origin=");log_hex(pci.pm.origin);
 log_text(" last=");log_hex(pci.pm.last);log_text(" maxgap=");log_hex(pci.pm.max_gap);log_text(" ticks=");log_hex(pci.pm.ticks);
 log_text(" status=");log_hex(pci.pm.timer_status);log_text("\n");
 log_text("OVMF.CLOCK_APIC clock_or_expiry_stop, event retained\n");return false;
}
static bool cpu_init(void){
 host_clock_cpuid_diag();
 uint32_t a,b,c,d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0),"c"(0));cpu.max_basic=a;
 if(a<1||b!=0x756e6547||c!=0x6c65746e||d!=0x49656e69)return false;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000000u),"c"(0));cpu.max_extended=a;
 if(a<0x80000008u)return false;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));cpu.basic_edx=d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000001u),"c"(0));cpu.extended_edx=d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000008u),"c"(0));cpu.physical_bits=a&255;cpu.virtual_bits=(a>>8)&255;
 return fcats_supported(&cpu);
}
static _Alignas(16) unsigned char fallback_stack[4096];
extern const unsigned char firmware_vars_start[],firmware_vars_end[];
static uint64_t saved_cr0,saved_cr3,saved_cr4;
static bool changed;
static __attribute__((noreturn)) void fatal(void){observation_fatal();panic("OVMF.CLOCK_APIC unsafe host/ownership");}
static __attribute__((noreturn)) void fallback(void){fatal();}
static uint64_t alloc(void*x){(void)x;return pmm_alloc_page();}
static bool release(void*x,uint64_t p){(void)x;return pmm_free_page(p)!=0;}
static void*map(void*x,uint64_t p){(void)x;return paging_phys_to_virt(p);}
static bool wb(void*x,uint64_t p){(void)x;return paging_page_rw_wb(p)&&vmx_memory_region_wb(p);}
static const struct sg_memory_ops memory_ops={0,alloc,release,map,wb};
#ifdef SYLE_M2_HOST_PARALLEL
enum fmp_operation { FMP_OPEN, FMP_CLOSE, FMP_READ, FMP_WRITE, FMP_SWITCH,
                     FMP_CLEAR, FMP_INVEPT, FMP_SHOOTDOWN, FMP_RESET, FMP_TSC, FMP_RUN,
#ifdef SYLE_M2_NESTED
                     FMP_READ_BATCH, FMP_WRITE_BATCH,
#endif
};
static bool fmp_call(enum fmp_operation,uint64_t,uint64_t);
static bool fmp_read(uint32_t,uint64_t *);
static bool fmp_write(uint32_t,uint64_t);
static bool fmp_quiescent(void);
static bool fmp_switch(uint64_t,uint64_t);
static bool fmp_shared_invept(void);
#ifdef SYLE_M2_NESTED
struct nested_vmcs_batch;
static bool fmp_vmcs_batch(struct nested_vmcs_batch *,bool);
static enum lvs_result fmp_nested_run(struct nested_l1_context *,bool);
static bool fmp_nested_invept(bool,uint64_t);
#endif
static enum lvs_result fmp_run_remote(void);
#define FW_SWITCH(from,to) fmp_switch(from,to)
#define FW_SHARED_INVEPT() fmp_shared_invept()
#define FW_AP_RUN() fmp_run_remote()
#define FW_QUIESCENT() fmp_quiescent()
#else
#define FW_SWITCH(from,to) lvb_switch_current(&backend,from,to)
#define FW_SHARED_INVEPT() ops.invept(ops.context,mem.eptp)
#define FW_AP_RUN() lvs_run(&firmware_ap.state,&firmware_ap.state_ops)
#define FW_QUIESCENT() (!paging_ap_start_attempted())
#endif
static bool rd(uint32_t f,uint64_t*v){
#ifdef SYLE_M2_HOST_PARALLEL
 return fmp_read(f,v);
#else
 unsigned char bad;__asm__ volatile("vmread %2,%1;setna %0":"=qm"(bad),"=r"(*v):"r"((uint64_t)f):"cc","memory");return !bad;
#endif
}
static bool traced_rd(uint32_t f,uint64_t*v){
 if(rd(f,v))return true;
 observation_finish();
 log_text("OVMF.CLOCK_APIC vmread_failed field=");log_hex(f);log_text("\n");return false;
}
static bool wr(uint32_t f,uint64_t v){
#ifdef SYLE_M2_HOST_PARALLEL
 return fmp_write(f,v);
#else
 return ops.vmwrite(ops.context,f,v);
#endif
}
static bool pic_write(void*x,uint32_t f,uint64_t v){(void)x;return wr(f,v);}
static bool safe(void){uint64_t a,b,c;__asm__ volatile("mov %%cr0,%0":"=r"(a));__asm__ volatile("mov %%cr3,%0":"=r"(b));__asm__ volatile("mov %%cr4,%0":"=r"(c));return !state.host_dirty&&a==saved_cr0&&b==saved_cr3&&c==(saved_cr4|(changed?0x2000:0));}
static bool irq_write(void*ctx,uint32_t field,uint64_t value){
 (void)ctx;uint64_t readback;
 return wr(field,value)&&rd(field,&readback)&&readback==value;
}
static bool irq_planned(uint32_t encoding,uint64_t*out){
 for(unsigned i=0;i<host.plan.count;i++)if(host.plan.fields[i].encoding==encoding){*out=host.plan.fields[i].value;return true;}
 return false;
}
/* Deliberately incomplete debug execution subset: ALL MOV DR still stop.
 * Debug register intercept has priority over DR4/5 #UD; never pass it through. */
static bool de_gate(struct fcmd_gate*g){
 uint64_t bitmap;
 *g=(struct fcmd_gate){.host_leaf1_edx=cpu.basic_edx,
 .cr4_fixed0=host.caps.cr4_fixed0,.cr4_fixed1=host.caps.cr4_fixed1};
 return (cpu.basic_edx&4)&&
 rd(0x4002,&g->primary_controls)&&(g->primary_controls&(UINT64_C(1)<<23))&&
 rd(0x2802,&g->debugctl)&&!g->debugctl&&rd(0x681a,&g->dr7)&&g->dr7==0x400&&
 rd(0x6822,&g->pending_debug)&&!g->pending_debug&&
 rd(0x4012,&g->entry_controls)&&(g->entry_controls&4)&&
 rd(0x400c,&g->exit_controls)&&(g->exit_controls&4)&&
 rd(0x4004,&bitmap)&&(bitmap&0x42)==0x42&&(host.caps.cr4_fixed1&8)&&
 g->primary_controls<=UINT32_MAX&&g->entry_controls<=UINT32_MAX&&g->exit_controls<=UINT32_MAX&&bitmap<=UINT32_MAX&&(!disk_pic_active||bitmap==0x42);
}
static bool de_mode_write(const struct fcm_state *current,enum fcm_register reg,uint64_t value,struct fcm_state*out){
 struct fcmd_gate gate;
 struct fcm_state base=*current;
#ifdef SYLE_M2_NESTED
 uint64_t vmxe=(reg==FCM_CR4?value:base.cr4)&0x2000;
 if(vmxe&&!nested_enabled)return false;
 if(!vmxe&&nested_current->vmcs.root)return false;
 base.cr4&=~UINT64_C(0x2000);
 if(reg==FCM_CR4)value&=~UINT64_C(0x2000);
#endif
 if(!de_gate(&gate)||!fcmd_write(&base,mem.layout.ram.size,reg,value,&gate,out))return false;
#ifdef SYLE_M2_NESTED
 out->cr4|=vmxe;
#endif
 return true;
}
static bool irq_identity(void){
 struct fcmd_gate gate;if(!de_gate(&gate))return false;
 uint64_t current=UINT64_MAX,primary,secondary,eptp,pin,entry,cr4,flags,expected_primary,expected_pin,expected_entry,efer,cs,cs_ar;unsigned char bad;
 __asm__ volatile("pushfq;pop %0":"=r"(flags));
 __asm__ volatile("vmptrst %1;setna %0":"=qm"(bad),"=m"(current)::"cc","memory");
 return irq_planned(0x4002,&expected_primary)&&irq_planned(0x4000,&expected_pin)&&
 irq_planned(0x4012,&expected_entry)&&!(expected_primary&4)&&
 !bad&&current==host.vmcs_page&&host.ready&&!host.quarantined&&safe()&&
 !(flags&0x200)&&!lvn_armed_rip&&FW_QUIESCENT()&&
 rd(0x4002,&primary)&&primary==(expected_primary|((irq_window.armed||disk_pic_window)?4:0))&&(primary&(1u<<31))&&
 rd(0x401e,&secondary)&&secondary==0xc2&&rd(0x201a,&eptp)&&eptp==mem.eptp&&
 rd(0x4000,&pin)&&pin==expected_pin&&!(pin&0x80)&&fag_host_events_exit(pin)&&
 rd(0x4012,&entry)&&entry==(expected_entry|0x200)&&!(entry&~UINT64_C(0xd3ff))&&
 rd(0x2806,&efer)&&(efer&~UINT64_C(1))==0x500&&rd(0x0802,&cs)&&cs&&cs<=UINT16_MAX&&
 rd(0x4816,&cs_ar)&&
#ifdef SYLE_FIRMWARE_DISK
 disk_cs_identity(cs,cs_ar)
#else
 !(cs&7)&&(cs_ar&~UINT64_C(1))==0xa09a
#endif
&&rd(0x6804,&cr4)&&!(cr4&~UINT64_C(0x26f8));
}
static bool irq_snapshot(struct lei_snapshot*out,uint32_t*primary){
 uint64_t flags,block,activity,event,vector=0,debug,dr7,p;
 if(!irq_identity()||!rd(0x6820,&flags)||!rd(0x4824,&block)||!rd(0x4826,&activity)||
 !rd(0x4016,&event)||!rd(0x6822,&debug)||!rd(0x681a,&dr7)||!rd(0x4002,&p)||
 (native.resume&&!rd(0x4408,&vector))||debug||dr7!=0x400||!(flags&2)||(flags&~UINT64_C(0x250ed7))||(flags&0x100)||
 (vector&0x80000000)||block>2||activity>1||event||p>UINT32_MAX)return false;
 *out=(struct lei_snapshot){.rflags=flags,.blocking=(uint32_t)block,.activity=(uint32_t)activity};
 *primary=(uint32_t)p;
if(!pm_delay_quiet || irq_session.owner.pending || irq_session.owner.in_service){
 log_text("OVMF.IRQ fresh_gate flags=");log_hex(flags);log_text(" blocking=");log_hex(block);
 log_text(" activity=");log_hex(activity);log_text(" vectoring_raw=");log_hex(vector);log_text("\n");
 }
 return true;
}
static bool cache_sample(void*ctx){(void)ctx;return safe()&&clock_sync();}
/* Fixed single stopped-vCPU fixture. Current VMCS, not just the planner,
 * must bind the owned EPT and disable VPID before guest-page-table mutation.
 * VM entry then invalidates VPID0 linear/combined translations. */
static bool dirty_admit(void){
 uint64_t secondary,eptp,primary,flags;
 __asm__ volatile("pushfq;pop %0":"=r"(flags));
 return safe()&&!(flags&0x200)&&!lvn_armed_rip&&FW_QUIESCENT()&&
  traced_rd(0x4002,&primary)&&(primary&(UINT64_C(1)<<31))&&
  traced_rd(0x401e,&secondary)&&secondary==0xc2&&
  traced_rd(0x201a,&eptp)&&eptp==mem.eptp&&(eptp&4095)==0x1e;
}
static enum fwd_cas_result dirty_cas(void*ctx,uint64_t*p,uint64_t old,uint64_t value){
 (void)ctx;return fwd_atomic_cas(p,old,value);
}
static enum fwd_cas_result accessed_cas(void*ctx,uint64_t*p,uint64_t old,uint64_t value){
 (void)ctx;return faa_atomic_cas(p,old,value);
}
/* Read-only observation: no source sample, no register/page-table writes.
 * Stack words are bounded heuristics, never authority to execute. */
static bool reg_read(unsigned,uint64_t*);
static bool rtc_observation(const char*phase){
 log_text("OVMF.RTC_OBSERVE phase=");log_text(phase);
 log_text(" reference=");log_hex(active_apic.reference);log_text(" epoch=");log_hex(active_apic.epoch);
 log_text(" tsc_origin=");log_hex(virtual_tsc.origin);log_text(" tsc_last=");log_hex(virtual_tsc.last);
 log_text(" tsc_commits=");log_hex(tsc_commits);log_text(" rtc_read_attempts=");log_hex(rtc_data_reads);
 log_text(" rtc_origin=");log_hex(cmos.origin);log_text(" rtc_last=");log_hex(cmos.last);
 log_text(" rtc_tick=");log_hex(cmos.rtc.tick);log_text(" rtc_seconds=");log_hex(cmos.rtc.tick/1193182);
 log_text(" index=");log_hex(cmos.rtc.index);log_text(" status_c=");log_hex(cmos.rtc.status_c);
 log_text(" pm_last=");log_hex(pci.pm.last);log_text(" pm_ticks=");log_hex(pci.pm.ticks);
 log_text(" owner_origin=");log_hex(irq_session.owner.origin);log_text(" owner_last=");log_hex(irq_session.owner.last);
 log_text(" deadline=");log_hex(irq_session.owner.deadline_bus);log_text(" expirations=");log_hex(irq_session.owner.expirations);
 log_text(" pending=");log_hex(irq_session.owner.pending);log_text(" ISR=");log_hex(irq_session.owner.in_service);
 log_text(" terminal=");log_hex(irq_session.owner.terminal);log_text("\n");
 struct fmm_memory m={.memory=&mem};uint64_t ar,cs,rbp,rsp,rip;
 if(!rd(0x6800,&m.cr0)||!rd(0x6802,&m.cr3)||!rd(0x6804,&m.cr4)||!rd(0x2806,&m.efer)||
 !rd(0x6808,&m.cs_base)||!rd(0x4816,&ar)||!rd(0x0802,&cs)||!rd(0x681e,&rip)||
 !reg_read(5,&rbp)||!reg_read(4,&rsp)){log_text("OVMF.RTC_STACK vmread_failed\n");return false;}
 m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)cs;struct fos_result result={0};
 bool ok=fos_observe(&m,rbp,rsp,true,&result);
 log_text("OVMF.RTC_STACK heuristic_only rip=");log_hex(rip);log_text(" rbp=");log_hex(rbp);
 log_text(" rsp=");log_hex(rsp);log_text(" ok=");log_hex(ok);log_text(" count=");log_hex(result.count);
 log_text(" stop=");log_hex(result.stop);log_text(" top_readable=");log_hex(result.top_readable);
 log_text(" top_word=");log_hex(result.top_return);log_text(" top_executable=");log_hex(result.top_executable);log_text("\n");
 for(unsigned i=0;i<result.count&&i<FOS_MAX_FRAMES;i++){
  log_text("OVMF.RTC_STACK frame=");log_hex(result.frames[i].frame);log_text(" previous=");log_hex(result.frames[i].previous);
  log_text(" return_word=");log_hex(result.frames[i].return_rip);log_text(" executable=");log_hex(result.frames[i].return_executable);log_text("\n");
 }

 /* Pinned Shell call site: MOV ECX,1000000; CALL [RAX+0xf8]. These reads
  * observe owned guest RAM only; a mismatch is diagnostic, never approval. */
 for(unsigned i=1;ok && i<result.count && i<FOS_MAX_FRAMES;i++){
  if(result.frames[i].return_rip!=0x685b97e || result.frames[i].frame<32 ||
     result.frames[i-1].return_rip!=0x7f1735e)continue;
  static const uint8_t expected[]={0xb9,0x40,0x42,0x0f,0,0xff,0x90,0xf8,0,0,0};
  uint8_t bytes[15];size_t n=fmm_fetch(&m,0x685b973,bytes);bool match=n>=sizeof expected;
  for(unsigned j=0;j<sizeof expected && match;j++)if(bytes[j]!=expected[j])match=false;
  uint64_t configured=0,remaining=0;
  bool readable=match && fos_read_word(&m,0x68ad080,&configured) &&
   fos_read_word(&m,result.frames[i].frame-32,&remaining);
  log_text("OVMF.PM_DELAY shell_countdown readable=");log_hex(readable);
  log_text(" configured_seconds=");log_hex(configured);log_text(" remaining_including_current=");log_hex(remaining);
  log_text(" pinned_callsite_readonly_no_clock_sample\n");
  break;
 }
 return true; /* Missing/non-frame-pointer chain is diagnostic, not a guest fault. */
}
static void io_instruction_diag(uint64_t rip);
/* Read-only rejection evidence. Never authorizes retry/resume or touches A/D. */
static void apic_id_diagnose(const struct fmm_memory*m,const struct fat_exit*e){
 static const unsigned mapregs[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:native.gpr[mapregs[i]];
 uint8_t bytes[15];size_t n=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov mov;
 bool decoded=n&&guest_mmio_mov_decode(bytes,n,e->rip,regs,&mov);
 io_instruction_diag(e->rip);
 log_text("OVMF.CACHE_KVM APIC_ID_diag decoded=");log_hex(decoded);
 if(decoded){log_text(" decoded_linear=");log_hex(mov.linear);log_text(" next_rip=");log_hex(mov.next_rip);}
 log_text(" observed_gla=");log_hex(e->linear);log_text(" observed_gpa=");log_hex(e->physical);
 log_text(" cr0=");log_hex(m->cr0);log_text(" cr3=");log_hex(m->cr3);log_text(" cr4=");log_hex(m->cr4);
 log_text(" efer=");log_hex(m->efer);log_text(" advanced=");log_hex(m->advanced_ept_info);log_text("\n");
 struct fmm_vars_diagnostic d;
 /* Reuse the owned generic walk half only; VARS backing fields are inapplicable. */
 if(fmm_diagnose_vars_write(m,e->linear,e->physical,&d)){
  log_text("OVMF.CACHE_KVM APIC_ID_walk mode=");log_hex(d.mode_ok);log_text(" canonical=");log_hex(d.canonical);
  log_text(" read_ok=");log_hex(d.read_ok);log_text(" read_gpa=");log_hex(d.read_gpa);
  log_text(" attrs=");log_hex(d.read_attributes);
  log_text(" write_ok=");log_hex(d.write_ok);log_text(" write_gpa=");log_hex(d.write_gpa);
  log_text(" write_attrs=");log_hex(d.write_attributes);log_text(" stop=");log_hex(d.stop);log_text("\n");
  for(unsigned i=0;i<d.count;i++){
   log_text("OVMF.CACHE_KVM APIC_ID_PTE level=");log_hex(i);log_text(" entry_gpa=");log_hex(d.entries[i].entry_gpa);
   log_text(" value=");log_hex(d.entries[i].value);log_text("\n");
  }
 }
}
/* Read-only evidence on the stopped-vCPU. No A/D, timer or device update. */
static void irq_lvt_diagnose(const struct fmm_memory*m,const struct fat_exit*e){
 static const unsigned rm[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:native.gpr[rm[i]];
 uint8_t bytes[15];size_t n=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 bool decoded=n&&guest_mmio_mov_decode(bytes,n,e->rip,regs,&plan);
 io_instruction_diag(e->rip);
 log_text("OVMF.IRQ LVT_same_diag decoded=");log_hex(decoded);
 if(decoded){log_text(" width=");log_hex(plan.width);log_text(" write=");log_hex(plan.write);
 log_text(" reg=");log_hex(plan.reg);log_text(" value=");log_hex(plan.store_value);
 log_text(" linear=");log_hex(plan.linear);log_text(" next_rip=");log_hex(plan.next_rip);}
 log_text(" observed_gla=");log_hex(e->linear);log_text(" observed_gpa=");log_hex(e->physical);
 log_text(" flags=");log_hex(e->rflags);log_text(" blocking=");log_hex(e->blocking);
 log_text(" debug=");log_hex(e->pending_debug);log_text(" entry=");log_hex(e->entry_event);log_text("\n");
 for(unsigned i=0;i<16;i++){log_text("OVMF.IRQ LVT_gpr index=");log_hex(i);log_text(" value=");log_hex(regs[i]);}
 log_text("OVMF.IRQ LVT_live fixed_value20020 last=");log_hex(irq_session.owner.last);
 log_text(" origin=");log_hex(irq_session.owner.origin);log_text(" deadline=");log_hex(irq_session.owner.deadline_bus);
 log_text(" period=");log_hex(FTI_PERIOD_BUS);log_text(" expirations=");log_hex(irq_session.owner.expirations);
 log_text(" pending=");log_hex(irq_session.owner.pending);log_text(" ISR=");log_hex(irq_session.owner.in_service);
 log_text(" reserved=");log_hex(irq_session.owner.reserved);log_text(" active_attempt=");log_hex(irq_session.active_attempt);
 log_text(" terminal=");log_hex(irq_session.owner.terminal);log_text(" source_epoch=");log_hex(active_apic.epoch);log_text("\n");
 struct fmm_vars_diagnostic d;
 if(fmm_diagnose_vars_write(m,e->linear,e->physical,&d)){
  log_text("OVMF.IRQ LVT_walk mode=");log_hex(d.mode_ok);log_text(" read_ok=");log_hex(d.read_ok);
  log_text(" write_ok=");log_hex(d.write_ok);log_text(" write_gpa=");log_hex(d.write_gpa);
  log_text(" write_attrs=");log_hex(d.write_attributes);log_text(" stop=");log_hex(d.stop);log_text(" VARS_backing_inapplicable\n");
  for(unsigned i=0;i<d.count;i++){log_text("OVMF.IRQ LVT_PTE gpa=");log_hex(d.entries[i].entry_gpa);log_text(" value=");log_hex(d.entries[i].value);}
 }
}
static bool cache_maintain(void*ctx){
 bool *attempted=ctx?ctx:&cache_attempted;
 if(*attempted||!safe()||lvn_armed_rip||!fwb_kvm_environment())return false;
 *attempted=true; /* Irreversible operation may start; never retry. */
 log_text("OVMF.CACHE_KVM attempting_L1_WBINVD outer_KVM_owns_physical_effect\n");
 bool maintained=fwb_kvm_maintain();
 log_text("OVMF.CACHE_KVM maintenance_returned=");log_hex(maintained);log_text("\n");
 return maintained&&safe();
}
static bool enable(void){
 uint64_t f,lo,hi,v;__asm__ volatile("pushfq;pop %0":"=r"(f));
 __asm__ volatile("mov %%cr0,%0":"=r"(saved_cr0));__asm__ volatile("mov %%cr3,%0":"=r"(saved_cr3));__asm__ volatile("mov %%cr4,%0":"=r"(saved_cr4));
 if((f&0x600)||(saved_cr4&0x2000)||!FW_QUIESCENT())return false;
 if(!lvn_read_msr(&native,0x3a,&v)||(v&5)!=5||!lvn_read_msr(&native,0x488,&lo)||!lvn_read_msr(&native,0x489,&hi))return false;
 v=saved_cr4|0x2000;if((v&lo)!=lo||(v&~hi))return false;
 __asm__ volatile("mov %0,%%cr4"::"r"(v):"memory");changed=true;return true;
}
static bool vcpu_reg_read(uint64_t g[15],unsigned n,uint64_t*v){static const unsigned ix[16]={0,2,3,1,0,6,4,5,7,8,9,10,11,12,13,14};if(n>15)return false;if(n==4)return rd(0x681c,v);*v=g[ix[n]];return true;}
static bool reg_read(unsigned n,uint64_t*v){return vcpu_reg_read(native.gpr,n,v);}
static bool vcpu_reg_write(uint64_t g[15],unsigned n,uint64_t v){static const unsigned ix[16]={0,2,3,1,0,6,4,5,7,8,9,10,11,12,13,14};if(n>15)return false;if(n==4)return wr(0x681c,v);g[ix[n]]=v;return true;}
static bool reg_write(unsigned n,uint64_t v){return vcpu_reg_write(native.gpr,n,v);}
static bool mode_commit_failed(void){log_text("OVMF.IRQ mode_commit_failed_effects_not_rolled_back_no_reentry\n");if(irq_session.ready){irq_session.failed=true;fti_quarantine(&irq_session.owner);}return false;}
static bool commit_vcpu_mode(struct fcm_state *current,const struct fcm_state*n){
 uint64_t entry,hw0=n->cr0|(host.caps.cr0_fixed0&~UINT64_C(0x80000001)),hw4=n->cr4|0x2000;
 if((hw0&~host.caps.cr0_fixed1)||(hw4&host.caps.cr4_fixed0)!=host.caps.cr4_fixed0||(hw4&~host.caps.cr4_fixed1)||!rd(0x4012,&entry))return false;
 entry=(entry&~UINT64_C(0x200))|(n->ia32e?0x200:0);
 /* Any partial failure is terminal; never re-enter a mixed-mode VMCS. */
 if(!wr(0x6800,hw0)||!wr(0x6004,n->cr0)||!wr(0x6802,n->cr3)||!wr(0x6804,hw4)||!wr(0x6006,n->cr4)||!wr(0x2806,n->efer)||!wr(0x4012,entry))return mode_commit_failed();
 *current=*n;return true;
}
static bool commit_mode(const struct fcm_state*n){return commit_vcpu_mode(&mode,n);}
static size_t io_instruction_fetch(uint64_t rip,uint8_t bytes[15]) {
 struct fmm_memory m={.memory=&mem};uint64_t ar,selector;
 if(!rd(0x6800,&m.cr0)||!rd(0x6802,&m.cr3)||!rd(0x6804,&m.cr4)||!rd(0x2806,&m.efer)||
 !rd(0x6808,&m.cs_base)||!rd(0x4816,&ar)||!rd(0x0802,&selector)) {
  log_text("OVMF.CLOCK_APIC io_bytes=vmread_failed\n");return 0;
 }
 m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)selector;
 return fmm_fetch(&m,rip,bytes);
}
static void io_instruction_diag(uint64_t rip) {
 uint8_t bytes[15];size_t count=io_instruction_fetch(rip,bytes);
 log_text("OVMF.CLOCK_APIC io_bytes rip=");log_hex(rip);log_text(" available=");log_hex(count);
 for(size_t i=0;i<count;i++){log_text(" byte=");log_hex(bytes[i]);}
 log_text("\n");
}
/* CR8 is the virtual task-priority class, not host CR8 or current PPR.
 * This isolated access cannot retire ISR or alter any timer ownership. */
static bool irq_cr8_access(uint64_t rip,uint64_t q,unsigned len,uint64_t flags,
 uint64_t blocking,uint64_t pending_debug,uint32_t vector){
 struct fmm_memory m={.memory=&mem};
 struct fat_exit e={.rip=rip,.reason=28,.qualification=q,.rflags=flags,
 .blocking=blocking,.pending_debug=pending_debug,.vectoring=vector};
 uint64_t ar,selector,event,primary;bool write=q==8;
 io_instruction_diag(rip);
 if(!irq_session.ready||!clock_sync()||!irq_identity()||
 !traced_rd(0x4002,&primary)||!(primary&(UINT64_C(1)<<(write?19:20)))||
 (primary&(UINT64_C(1)<<21))||
 !traced_rd(0x6800,&m.cr0)||!traced_rd(0x6802,&m.cr3)||
 !traced_rd(0x6804,&m.cr4)||!traced_rd(0x2806,&m.efer)||
 !traced_rd(0x6808,&m.cs_base)||!traced_rd(0x4816,&ar)||
 !traced_rd(0x0802,&selector)||!traced_rd(0x681a,&e.dr7)||
 !traced_rd(0x4016,&event)||ar>UINT32_MAX||selector>UINT16_MAX||event>UINT32_MAX)return false;
 m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)selector;e.entry_event=(uint32_t)event;
 log_text("OVMF.IRQ CR8_live value64=");log_hex(native.gpr[0]);
 log_text(" reserved=");log_hex(irq_session.owner.reserved);log_text(" attempt=");log_hex(irq_session.active_attempt);
 log_text(" phase=");log_hex(irq_session.event.phase);log_text(" last=");log_hex(irq_session.owner.last);
 log_text(" deadline=");log_hex(irq_session.owner.deadline_bus);log_text(" period=");log_hex(FTI_PERIOD_BUS);
 log_text(" fixed_live_timer_LVT20020_unmasked_no_reconfiguration\n");
 log_text("OVMF.IRQ CR8_access write=");log_hex(write);log_text(" primary=");log_hex(primary);
 log_text(" flags=");log_hex(flags);log_text(" tpr=");log_hex(active_apic.device.apic.irq.tpr);
 log_text(" ISR=");log_hex(irq_session.owner.in_service);log_text(" pending=");log_hex(irq_session.owner.pending);log_text("\n");
 enum fat_result result=write?fir8_write(&active_apic.device,&irq_session,native.gpr,&m,&e,len,true,0,irq_write):fir8_read(&active_apic.device,&irq_session,native.gpr,&m,&e,len,true,0,irq_write);
 log_text("OVMF.IRQ CR8_access result=");log_hex((uint64_t)(int64_t)result);
 log_text(" rax=");log_hex(native.gpr[0]);log_text(" tpr=");log_hex(active_apic.device.apic.irq.tpr);log_text(" ISR=");log_hex(irq_session.owner.in_service);
 log_text(" pending=");log_hex(irq_session.owner.pending);log_text(" terminal=");log_hex(irq_session.owner.terminal);log_text("\n");
 return result==FAT_COMMITTED;
}
/* Software clean bank only: NEVER source guest values from physical DRs. */
static bool debug_read_dispatch(uint64_t rip,uint64_t q,unsigned len,uint64_t flags,
 uint64_t blocking,uint64_t pending_debug,uint32_t vector){
 struct fmm_memory m={.memory=&mem};struct fcmd_gate controls;
 struct fat_exit e={.rip=rip,.reason=29,.qualification=q,.rflags=flags,
 .blocking=blocking,.pending_debug=pending_debug,.vectoring=vector};
 uint64_t ar,selector,event;
 io_instruction_diag(rip);
 if(!debug_bank.ready||debug_bank.failed||!clock_sync()||!irq_identity()||!de_gate(&controls)||
 !traced_rd(0x6800,&m.cr0)||!traced_rd(0x6802,&m.cr3)||!traced_rd(0x6804,&m.cr4)||
 !traced_rd(0x2806,&m.efer)||!traced_rd(0x6808,&m.cs_base)||!traced_rd(0x4816,&ar)||
 !traced_rd(0x0802,&selector)||!traced_rd(0x4016,&event)||ar>UINT32_MAX||selector>UINT16_MAX||event>UINT32_MAX)return false;
 m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)selector;e.entry_event=(uint32_t)event;e.dr7=controls.dr7;
 /* No exception exit has been resumed: reason0 is a permanent stop below. */
 struct fdr_gate gate={.domain=host.vmcs_page,.vmcs=host.vmcs_page,
 .primary_controls=controls.primary_controls,.debugctl=controls.debugctl,.debug_events_absent=true};
 enum fat_result r=fdr_read(&debug_bank,&gate,native.gpr,&m,&e,len,true,0,irq_write);
 log_text("OVMF.IRQ virtual_debug_read q=");log_hex(q);log_text(" result=");log_hex((uint64_t)(int64_t)r);
 log_text(" rax=");log_hex(native.gpr[0]);log_text(" actual_dr7=");log_hex(controls.dr7);
 log_text(" primary=");log_hex(controls.primary_controls);log_text(" failed=");log_hex(debug_bank.failed);
 log_text(" software_values_only_no_host_DR_source\n");
 if(r==FAT_FATAL)mode_commit_failed();
 return r==FAT_COMMITTED;
}
static bool fwcfg_ram_write(void*ctx,uint8_t*target,const uint8_t*bytes,unsigned count){
 (void)ctx;if(!target||!bytes||count>64)return false;
 for(unsigned i=0;i<count;i++)((volatile uint8_t*)target)[i]=bytes[i];
 return true;
}
static bool fwcfg_dispatch(uint64_t rip,uint64_t q,unsigned length,uint64_t flags,uint64_t blocking,uint64_t pending_debug,uint64_t vector){
 struct fmm_memory m={.memory=&mem};struct fwc_exit e={.rip=rip,.qualification=q,.length=length,.flags=flags,.blocking=blocking,.pending_debug=pending_debug,.vectoring=vector,.info_supported=!!(host.basic&(UINT64_C(1)<<54))};
 uint64_t ar,selector,info,es_ar;
 if(!traced_rd(0x6800,&m.cr0)||!traced_rd(0x6802,&m.cr3)||!traced_rd(0x6804,&m.cr4)||!traced_rd(0x2806,&m.efer)||
 !traced_rd(0x6808,&m.cs_base)||!traced_rd(0x4816,&ar)||!traced_rd(0x0802,&selector)||
 !traced_rd(0x681a,&e.dr7)||!traced_rd(0x4016,&e.entry_event)||!traced_rd(0x6806,&e.es_base)||
 !traced_rd(0x4814,&es_ar)||!traced_rd(0x640a,&e.linear)||!traced_rd(0x440e,&info))return false;
 m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)selector;e.info=(uint32_t)info;e.es_ar=(uint32_t)es_ar;
 if(!clock_sync())return false;
 bool ok=fwc_apply(&fwcfg,native.gpr,&m,&e,true,0,fwcfg_ram_write,pic_write);
 log_text("OVMF.CLOCK_APIC fwcfg ok=");log_hex(ok);log_text(" stage=");log_hex(fwcfg.stage);log_text(" info_supported=");log_hex(e.info_supported);log_text(" selector=");log_hex(fwcfg.stream.selector);log_text(" cursor=");log_hex(fwcfg.stream.cursor);log_text(" rcx=");log_hex(native.gpr[2]);log_text(" rdi=");log_hex(native.gpr[5]);log_text(" info=");log_hex(info);log_text(" es_ar=");log_hex(es_ar);log_text("\n");
 if(!ok)io_instruction_diag(rip);
 return ok;
}
#ifdef SYLE_FIRMWARE_PAUSE
static bool pause_read(void*x,uint32_t field,uint64_t*value){(void)x;return traced_rd(field,value);}
static bool pause_dispatch(void){
 struct fpa_trace trace={0};
#ifdef SYLE_M2_DUAL_VCPU
 if(!irq_session.ready){
  if(!dirty_admit()||!observation_ok())return false;
 }else
#endif
 if(!irq_session.ready||!irq_identity()||!observation_ok())return false;
 enum fat_result result=fpa_handle(&pause_state,&mem,true,0,pause_read,pic_write,&trace);
 if(result==FAT_COMMITTED && trace.rip==0x70c236e){
  pm_delay_pauses++;
  bool fresh=!pm_delay_seen || pm_delay_target!=(uint32_t)native.gpr[1];
  if(fresh){pm_delay_report=false;(void)rtc_observation("delay_new_target");}
  pm_delay_seen=true;pm_delay_target=(uint32_t)native.gpr[1];
  if(fresh || (pm_delay_pauses&255)==0){
   log_text("OVMF.PM_DELAY snapshot pauses=");log_hex(pm_delay_pauses);
   log_text(" target_ebx=");log_hex(native.gpr[1]);log_text(" remaining_eax=");log_hex(native.gpr[0]);
   log_text(" request_edi=");log_hex(native.gpr[5]);log_text(" chunks_r12=");log_hex(native.gpr[11]);
   log_text(" pm_ticks=");log_hex(pci.pm.ticks);log_text(" reference=");log_hex(active_apic.reference);
   log_text(" no_extra_clock_sample\n");
  }
 }
 if(!pm_delay_quiet || result!=FAT_COMMITTED){
 log_text("OVMF.PAUSE result=");log_hex((uint64_t)(int64_t)result);
 log_text(" rip=");log_hex(trace.rip);log_text(" length=");log_hex(trace.length);
 log_text(" flags=");log_hex(trace.flags);log_text(" primary=");log_hex(trace.primary);
 log_text(" secondary=");log_hex(trace.secondary);log_text(" available=");log_hex(trace.available);
 log_text(" byte0=");log_hex(trace.bytes[0]);log_text(" byte1=");log_hex(trace.bytes[1]);
 log_text(" completed=");log_hex(pause_state.completed);log_text(" terminal=");log_hex(pause_state.failed);
 log_text(" same_instruction_and_clock_semantics\n");
 }
 if(result==FAT_FATAL)mode_commit_failed();
 return result==FAT_COMMITTED;
}
#endif
#ifdef SYLE_FIRMWARE_HALT
#include "firmware_halt_observe.inc"
#endif
#ifdef SYLE_FIRMWARE_CONSOLE
#include "firmware_console_observe.inc"
#endif
#ifdef SYLE_FIRMWARE_DISK
#include "firmware_persistent_observe.inc"
#endif
#ifdef SYLE_M2_NESTED
#ifdef SYLE_M2_DUAL_VCPU
static bool nested_l1_irq_poll(uint8_t *vector);
static bool nested_l1_irq_accept(uint8_t vector);
#endif
#include "firmware_kvm_init.inc"
#endif
#ifdef SYLE_M2_DUAL_VCPU
/* AP starts on guest SIPI. Both VMCS01s share the guest EPT. */
static struct {
 uint64_t page,last_reason,last_qual,last_rip,entries,starts;
 struct fcm_state mode;
 struct guest_apic apic,bsp_linux;
 bool handoff;
 unsigned svr_changes;
 uint64_t timer_backlog[2];
 uint64_t hpet_started;
 uint32_t hpet_count,hpet_compare[3];
 bool hpet_enabled;
 struct fpa_state pause;
 struct fbd_bank debug;
 struct fwb_state cache;
 struct lmc_state microcode;
 struct ft100_mmio timer;
 uint64_t last_operand;
 uint32_t icr_high,icr_low,mmio_value;
 uint8_t elcr[2],shutdown_code,imcr_index,imcr;
 bool initialized,runnable,gp_pending,halt_if;
 struct lvn_context native;
 struct lvs_state state;
 struct lvs_ops state_ops;
} firmware_ap;
#ifdef SYLE_M2_HOST_PARALLEL
#undef log_text
#undef log_hex
#include "firmware_host_parallel.inc"
#define log_text observation_text
#define log_hex observation_hex
#endif
#include "firmware_ap_start.inc"
#endif
static bool dispatch(uint64_t reason){
 uint64_t rip,len,q,vector,cs;
 if(!traced_rd(0x681e,&rip)||!traced_rd(0x440c,&len)||!traced_rd(0x6400,&q)||!traced_rd(0x4408,&vector))return false;
 if(irq_session.ready&&!(vector&0x80000000)){
  if(vector){log_text("OVMF.IRQ vectoring_invalid_payload=");log_hex(vector);log_text(" ignored_per_valid_bit\n");}
  vector=0; /* Logical helper input only; hardware field and raw receipt unchanged. */
 }
 if(vector){log_text("OVMF.CLOCK_APIC reject=vectoring value=");log_hex(vector);log_text("\n");return false;}
 if(!traced_rd(0x0802,&cs))return false;
 if(cs&3){
#ifdef SYLE_FIRMWARE_DISK
  uint64_t ar;if((reason!=10&&reason!=16&&reason!=52)||!rd(0x4816,&ar)||!disk_cs_identity(cs,ar))return false;
#else
  log_text("OVMF.CLOCK_APIC reject=cpl cs=");log_hex(cs);log_text("\n");return false;
#endif
 }
 uint64_t flags,blocking,pending_debug;
 if(!traced_rd(0x6820,&flags)||!traced_rd(0x4824,&blocking)||!traced_rd(0x6822,&pending_debug))return false;
 if(reason==48||reason==52){log_text("OVMF.CLOCK_APIC instruction_gate reason=");log_hex(reason);log_text(" flags=");log_hex(flags);log_text(" blocking=");log_hex(blocking);log_text(" pending_debug=");log_hex(pending_debug);log_text(" cs=");log_hex(cs);log_text("\n");}
 if(reason==52){
  uint64_t dr7,event;if(!traced_rd(0x681a,&dr7)||!traced_rd(0x4016,&event))return false;
  /* For CPL3, identity above authenticated the flat user CS/SS. The pure
   * preemption flag/event predicate is privilege-independent. */
  bool resume;
#ifdef SYLE_FIRMWARE_DISK
  resume=fboot_preemption_resume(reason,cs,flags,blocking,pending_debug,dr7,event,vector);
#else
  resume=fpr_resume(reason,cs,flags,blocking,pending_debug,dr7,event,vector);
#endif
  log_text("OVMF.CLOCK_APIC preemption_resume_unchanged=");log_hex(resume);log_text(" dr7=");log_hex(dr7);log_text(" entry_event=");log_hex(event);log_text("\n");
  return resume; /* No RIP/RFLAGS write: this was not an emulated instruction. */
 }
#ifdef SYLE_FIRMWARE_HALT
 if(reason==12)return halt_dispatch();
#endif
#ifdef SYLE_FIRMWARE_PAUSE
 if(reason==40)return pause_dispatch(); /* Own RF retirement; outer clock/IRQ accounting unchanged. */
#endif
 /* Only exact CR8 accesses use their own RF retirement and strict instruction gate. */
 if(reason==0){debug_bank.failed=true;log_text("OVMF.IRQ exception_exit_no_debug_state_resume\n");return false;}
 if(reason==29){if(len>15)return false;return disk_debug(rip,q,(unsigned)len,flags,blocking,pending_debug,(uint32_t)vector);}
 if(reason==28&&(q&15)==8){if(len>15)return false;return irq_cr8_access(rip,q,(unsigned)len,flags,blocking,pending_debug,(uint32_t)vector);}
 if((flags&(UINT64_C(1)<<8))||((flags&(UINT64_C(1)<<16))&&reason!=48)||blocking||pending_debug){log_text("OVMF.CLOCK_APIC reject=instruction_state\n");return false;}
#ifdef SYLE_FIRMWARE_DISK
#ifdef SYLE_M2_NESTED
 if(nested_enabled&&((reason>=19&&reason<=27)||reason==50))return nested_root_instruction(reason,rip,len,q,flags);
 if(nested_enabled&&reason==31&&nested_msr_supported(native.gpr[2]))return nested_read_msr(native.gpr,rip,len);
#endif
 if(reason==31&&(uint32_t)native.gpr[2]==0xce)return disk_inject_gp(rip,(unsigned)len);
 if(reason==14)return disk_invlpg(rip,q,(unsigned)len);
 if((reason==31||reason==32)&&disk_linux_msr_supported((uint32_t)native.gpr[2]))return disk_gs_msr(rip,(unsigned)reason,(unsigned)len);
#endif
 if(reason==16){
  if(!clock_sync()||!irq_identity()||!tsc_source_ready())return false;
  struct fmm_memory m={.memory=&mem};struct fat_exit e={.rip=rip,.qualification=q,.reason=16,
   .rflags=flags,.blocking=blocking,.pending_debug=pending_debug,.vectoring=(uint32_t)vector};
  uint64_t ar,selector,event,primary;
  if(!rd(0x6800,&m.cr0)||!rd(0x6802,&m.cr3)||!rd(0x6804,&m.cr4)||!rd(0x2806,&m.efer)||
     !rd(0x6808,&m.cs_base)||!rd(0x4816,&ar)||!rd(0x0802,&selector)||
     !rd(0x681a,&e.dr7)||!rd(0x4016,&event)||event>UINT32_MAX||
     !rd(0x4002,&primary)||!(primary&(1u<<12)))return false;
  m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)selector;e.entry_event=(uint32_t)event;
  uint8_t bytes[15];size_t available=fmm_fetch(&m,rip,bytes);
  log_text("OVMF.TSC actual_bytes available=");log_hex(available);
  for(size_t i=0;i<available;i++){log_text(" byte=");log_hex(bytes[i]);}
  log_text(" len=");log_hex(len);log_text(" primary=");log_hex(primary);
  log_text(" cr4=");log_hex(m.cr4);log_text(" cs=");log_hex(selector);
  log_text(" flags=");log_hex(flags);log_text(" reference=");log_hex(active_apic.reference);
  log_text(" origin=");log_hex(virtual_tsc.origin);log_text(" last=");log_hex(virtual_tsc.last);
  log_text(" epoch=");log_hex(virtual_tsc.epoch);log_text(" cpu=");log_hex(virtual_tsc.cpu);log_text("\n");
  if(!observation_ok())return false;
  struct ftsc_sample sample={.domain=host.vmcs_page,.epoch=active_apic.epoch,
   .reference=active_apic.reference,.cpu=clock_source.properties.cpu,.source_admitted=tsc_source_ready()};
  enum fat_result result=ftsc_read(&virtual_tsc,&sample,native.gpr,&m,&e,(unsigned)len,true,0,pic_write);
  log_text("OVMF.TSC result=");log_hex((uint64_t)(int64_t)result);
  log_text(" eax=");log_hex(native.gpr[0]);log_text(" edx=");log_hex(native.gpr[3]);
  log_text(" terminal=");log_hex(virtual_tsc.failed);log_text("\n");
  if(result==FAT_FATAL)mode_commit_failed();
  if(result==FAT_COMMITTED)tsc_commits++;
  return result==FAT_COMMITTED;
 }
 if(reason==48){
  if(active_apic.device.lint0_extint_deasserted&&!flq_pic_quiet(&pic)){
   log_text("OVMF.CLOCK_APIC reject=lint0_pic_not_quiet\n");return false;
  }
  struct fmm_memory m={.memory=&mem,.advanced_ept_info=!!(host.caps.ept&(UINT64_C(1)<<22))};struct fat_exit e={.rip=rip,.qualification=q,.reason=48,.vectoring=(uint32_t)vector};
  uint64_t ar,selector,event;struct fcp_result cpu_result;
  if(!traced_rd(0x6800,&m.cr0)||!traced_rd(0x6802,&m.cr3)||!traced_rd(0x6804,&m.cr4)||!traced_rd(0x2806,&m.efer)||
   !traced_rd(0x6808,&m.cs_base)||!traced_rd(0x4816,&ar)||!traced_rd(0x0802,&selector)||!traced_rd(0x681c,&e.rsp)||
   !traced_rd(0x680c,&e.ds_base)||!traced_rd(0x6806,&e.es_base)||!traced_rd(0x680a,&e.ss_base)||
   !traced_rd(0x640a,&e.linear)||!traced_rd(0x2400,&e.physical)||!traced_rd(0x6820,&e.rflags)||
   !traced_rd(0x4824,&e.blocking)||!traced_rd(0x6822,&e.pending_debug)||!traced_rd(0x681a,&e.dr7)||!traced_rd(0x4016,&event))return false;
  if(!fcats_query(&cpu,tsc_source_ready(),1,0,&cpu_result)){log_text("OVMF.CLOCK_APIC reject=cpu_query\n");return false;}
  log_text("OVMF.CLOCK_APIC ept_snapshot_ready cr0=");log_hex(m.cr0);log_text(" cr3=");log_hex(m.cr3);log_text(" cr4=");log_hex(m.cr4);log_text(" cs_ar=");log_hex(ar);log_text(" ept_caps=");log_hex(host.caps.ept);log_text("\n");
  m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)selector;e.entry_event=(uint32_t)event;
#ifdef SYLE_M2_DUAL_VCPU
  if((e.physical&~UINT64_C(4095))==0xfed00000)return linux_hpet(&m,&e,native.gpr);
  if(e.physical==0xfee00300||e.physical==0xfee00310)return firmware_icr(&m,&e);
  if((firmware_ap.handoff||(e.rip>>63&&e.physical==0xfee000f0))&&
     e.physical>=0xfee00000&&e.physical<0xfee01000)return linux_bsp_mmio(&m,&e);
#endif
  if(!clock_sync())return false;
  if(irq_session.ready){log_text("OVMF.IRQ live_owner last=");log_hex(irq_session.owner.last);log_text(" deadline=");log_hex(irq_session.owner.deadline_bus);log_text(" following_legacy_fields_are_retired_snapshot\n");}
  log_text("OVMF.CLOCK_APIC before_mmio svr=");log_hex(active_apic.device.apic.irq.svr);
  log_text(" reference=");log_hex(active_apic.reference);log_text(" origin=");log_hex(active_apic.clock.timer.origin);
  log_text(" timer_last=");log_hex(active_apic.clock.timer.last);log_text(" deadline_bus=");log_hex(active_apic.clock.timer.deadline_bus);
  log_text(" period_bus=");log_hex(active_apic.clock.timer.period_bus);log_text(" divisor=");log_hex(active_apic.clock.timer.divisor);log_text("\n");
  log_text("OVMF.CLOCK_APIC lint0_quiet_profile=");log_hex(active_apic.device.lint0_extint_deasserted);
  log_text(" lint1_nmi_quiet_profile=");log_hex(active_apic.device.lint1_nmi_deasserted);
  log_text(" pic_master=");log_hex(pic.master);log_text(" slave=");log_hex(pic.slave);
  log_text(" master_phase=");log_hex(pic.chip[0].phase);log_text(" slave_phase=");log_hex(pic.chip[1].phase);log_text("\n");
#ifdef SYLE_FIRMWARE_CODE_ROM
  if(fwl_contains(&mem.layout.code,e.physical,1)){
   io_instruction_diag(rip);
   enum fat_result result=fcode_mmio(&code_rom,native.gpr,&m,&e,true,0,pic_write);
   log_text("OVMF.CODE_ROM result=");log_hex((uint64_t)(int64_t)result);
   log_text(" address=");log_hex(code_rom.address);log_text(" original=");log_hex(code_rom.original);
   log_text(" stage=");log_hex(code_rom.stage);log_text(" terminal=");log_hex(code_rom.failed);
   log_text(" immutable_backing_no_flash_write\n");
   if(result==FAT_FATAL)mode_commit_failed();
   return result==FAT_COMMITTED;
  }
#endif
  /* Diagnostic stop only: do not complete/ignore the CODE write or change
   * EPT, guest page tables, flags, RIP, GPR, or any virtual device state. */
  if(e.physical==UINT64_C(0xffc84010)){
   struct fmm_code_diagnostic d;
   io_instruction_diag(rip);
   log_text("OVMF.CODE_DIAG gla=");log_hex(e.linear);log_text(" gpa=");log_hex(e.physical);
   log_text(" rax=");log_hex(native.gpr[0]);log_text(" ds=");log_hex(e.ds_base);
   log_text(" flags=");log_hex(e.rflags);log_text(" vectoring=");log_hex(e.vectoring);log_text("\n");
   for(unsigned i=0;i<15;i++){log_text("OVMF.CODE_DIAG gpr_index=");log_hex(i);log_text(" value=");log_hex(native.gpr[i]);log_text("\n");}
   if(fmm_diagnose_code_write(&m,e.linear,e.physical,&d)){
    log_text("OVMF.CODE_DIAG mode=");log_hex(d.walk.mode_ok);log_text(" canonical=");log_hex(d.walk.canonical);
    log_text(" read_ok=");log_hex(d.walk.read_ok);log_text(" read_gpa=");log_hex(d.walk.read_gpa);
    log_text(" read_attrs=");log_hex(d.walk.read_attributes);log_text(" write_ok=");log_hex(d.walk.write_ok);
    log_text(" write_gpa=");log_hex(d.walk.write_gpa);log_text(" write_attrs=");log_hex(d.walk.write_attributes);
    log_text(" code_owner=");log_hex(d.code_backing_ok);log_text(" byte_ok=");log_hex(d.byte_ok);
    log_text(" original=");log_hex(d.original);log_text(" backing_stop=");log_hex(d.walk.backing_stop);
    log_text(" walk_stop=");log_hex(d.walk.stop);log_text("\n");
    for(unsigned i=0;i<d.walk.ept_count;i++){log_text("OVMF.CODE_DIAG ept_level=");log_hex(i);log_text(" permissions=");log_hex(d.walk.ept_permissions[i]);log_text(" match=");log_hex(d.walk.ept_matches[i]);log_text("\n");}
    for(unsigned i=0;i<d.walk.count;i++){log_text("OVMF.CODE_DIAG pt_level=");log_hex(i);log_text(" table_gpa=");log_hex(d.walk.entries[i].table_gpa);log_text(" entry_gpa=");log_hex(d.walk.entries[i].entry_gpa);log_text(" value=");log_hex(d.walk.entries[i].value);log_text("\n");}
   }
   log_text("OVMF.CODE_DIAG stop_only_no_store_no_commit\n");
   return false;
  }
  if(fwl_contains(&mem.layout.vars,e.physical,1)){
   io_instruction_diag(rip);
   struct fvr_trace rom_trace;
   enum fat_result rom_result=fvr_mmio_traced(&vars_rom,native.gpr,&m,&e,true,0,pic_write,&rom_trace);
   log_text("OVMF.CACHE_KVM ROM_trace stage=");log_hex(rom_trace.stage);
   log_text(" available=");log_hex(rom_trace.available);log_text(" width=");log_hex(rom_trace.width);
   log_text(" write=");log_hex(rom_trace.write);log_text(" linear=");log_hex(rom_trace.linear);
   log_text(" value=");log_hex(rom_trace.value);log_text(" dr7=");log_hex(e.dr7);
   log_text(" entry_event=");log_hex(e.entry_event);log_text(" ds=");log_hex(e.ds_base);
   log_text(" es=");log_hex(e.es_base);log_text(" ss=");log_hex(e.ss_base);log_text("\n");
   if(rom_result==FAT_REJECTED){
    struct fmm_vars_diagnostic d;
    if(fmm_diagnose_vars_write(&m,e.linear,e.physical,&d)){
     log_text("OVMF.CACHE_KVM ROM_walk mode=");log_hex(d.mode_ok);log_text(" canonical=");log_hex(d.canonical);
     log_text(" read_ok=");log_hex(d.read_ok);log_text(" read_gpa=");log_hex(d.read_gpa);
     log_text(" read_attrs=");log_hex(d.read_attributes);log_text(" write_ok=");log_hex(d.write_ok);
     log_text(" write_gpa=");log_hex(d.write_gpa);log_text(" write_attrs=");log_hex(d.write_attributes);
     log_text(" backing_ok=");log_hex(d.vars_backing_ok);log_text(" backing_stop=");log_hex(d.backing_stop);
     log_text(" stop=");log_hex(d.stop);log_text("\n");
     for(unsigned i=0;i<d.ept_count;i++){
      log_text("OVMF.CACHE_KVM ROM_EPT level=");log_hex(i);log_text(" permission_bits=");log_hex(d.ept_permissions[i]);
      log_text(" expected_match=");log_hex(d.ept_matches[i]);log_text("\n");
     }
     for(unsigned i=0;i<d.count;i++){
      log_text("OVMF.CACHE_KVM ROM_PTE level=");log_hex(i);log_text(" table_gpa=");log_hex(d.entries[i].table_gpa);
      log_text(" entry_gpa=");log_hex(d.entries[i].entry_gpa);log_text(" value=");log_hex(d.entries[i].value);log_text("\n");
     }
    }
   }
   if(rom_result==FAT_REJECTED){
    bool admission=dirty_admit();
    rom_result=fvdt_apply(&vars_dirty,&vars_rom,native.gpr,&m,&e,
      admission,admission,0,cache_sample,dirty_cas,pic_write);
    log_text("OVMF.CACHE_KVM ROM_dirty admission=");log_hex(admission);
    log_text(" result=");log_hex((uint64_t)(int64_t)rom_result);
    log_text(" consumed=");log_hex(vars_dirty.transaction.consumed);
    log_text(" CAS_confirmed=");log_hex(vars_dirty.transaction.dirty_changed);
    log_text(" complete=");log_hex(vars_dirty.transaction.complete);
    log_text(" terminal=");log_hex(vars_dirty.transaction.failed);
    log_text(" failed_effect_not_rolled_back\n");
   }
   if(rom_result==FAT_FATAL){active_apic.failed=true;if(irq_session.ready){irq_session.failed=true;fti_quarantine(&irq_session.owner);}else active_apic.clock.timer.terminal=true;}
   log_text("OVMF.CACHE_KVM vars_ROM_probe result=");log_hex((uint64_t)(int64_t)rom_result);
   log_text(" address=");log_hex(vars_rom.address);log_text(" original=");log_hex(vars_rom.original);
   log_text(" stage=");log_hex(vars_rom.stage);log_text(" terminal=");log_hex(vars_rom.failed);
   log_text(" immutable_backing_no_flash_write\n");
   return rom_result==FAT_COMMITTED;
  }
  if(e.physical==UINT64_C(0xfed40000)){
   io_instruction_diag(rip);
   uint64_t before_rax=native.gpr[0];
   enum fat_result tpm_result=ftt_mmio(&tpm_absent_transaction,native.gpr,&m,&e,true,0,pic_write);
   if(tpm_result==FAT_FATAL){active_apic.failed=true;if(irq_session.ready){irq_session.failed=true;fti_quarantine(&irq_session.owner);}else active_apic.clock.timer.terminal=true;}
   log_text("OVMF.CACHE_KVM tpm_absent_byte result=");log_hex((uint64_t)(int64_t)tpm_result);
   log_text(" before_rax=");log_hex(before_rax);log_text(" after_rax=");log_hex(native.gpr[0]);
   log_text(" terminal=");log_hex(tpm_absent_transaction.failed);log_text(" no_TPM_security_claim\n");
   return tpm_result==FAT_COMMITTED;
  }
  if(irq_session.ready){
#ifdef SYLE_FIRMWARE_HALT
   bool halt_was_in_service=irq_session.owner.in_service;
#endif
   enum fat_result ir=fiap_mmio(&active_apic.device,&irq_session,native.gpr,&m,&e,0,pic_write);
   if(ir==FAT_REJECTED&&e.physical==UINT64_C(0xfee00320)){
    irq_lvt_diagnose(&m,&e);
    bool admitted=irq_identity()&&observation_ok();
    ir=film_write(&active_apic.device,&irq_session,native.gpr,&m,&e,admitted,0,irq_write);
    log_text("OVMF.IRQ LVT_same_result=");log_hex((uint64_t)(int64_t)ir);
    log_text(" admitted=");log_hex(admitted);log_text(" last=");log_hex(irq_session.owner.last);
    log_text(" deadline=");log_hex(irq_session.owner.deadline_bus);log_text(" pending=");log_hex(irq_session.owner.pending);
    log_text(" ISR=");log_hex(irq_session.owner.in_service);log_text(" terminal=");log_hex(irq_session.owner.terminal);log_text("\n");
   }
#ifdef SYLE_FIRMWARE_HALT
   if(ir==FAT_COMMITTED&&e.physical==UINT64_C(0xfee000b0)&&halt_was_in_service&&
      !irq_session.owner.in_service&&halt_state.wakeups>halt_last_eoi_wake){
    halt_last_eoi_wake=halt_state.wakeups;halt_eois++;
    log_text("OVMF.HLT wake_EOI wakeups=");log_hex(halt_state.wakeups);
    log_text(" eois=");log_hex(halt_eois);log_text(" rip=");log_hex(e.rip);log_text(" committed_guest_EOI\n");
   }
#endif
   log_text("OVMF.IRQ mmio result=");log_hex((uint64_t)(int64_t)ir);
   log_text(" gpa=");log_hex(e.physical);log_text(" pending=");log_hex(irq_session.owner.pending);
   log_text(" ISR=");log_hex(irq_session.owner.in_service);log_text("\n");
   return ir==FAT_COMMITTED;
  }
  struct fat_trace trace;
  enum fat_result result=fct_mmio_traced(&active_apic,native.gpr,&m,&e,cpu_result.c,cpu_result.d,true,0,pic_write,&trace);
  log_text("OVMF.CLOCK_APIC decode stage=");log_hex(trace.stage);log_text(" width=");log_hex(trace.width);log_text(" reg=");log_hex(trace.reg);log_text(" write=");log_hex(trace.write);log_text(" value=");log_hex(trace.store_value);log_text(" lvt=");log_hex(trace.lvt);log_text(" initial=");log_hex(trace.initial);log_text(" armed=");log_hex(trace.armed);log_text("\n");
  log_text("OVMF.CLOCK_APIC mmio_result=");log_hex((uint64_t)(int64_t)result);log_text(" cs_ar=");log_hex(ar);log_text(" ept_caps=");log_hex(host.caps.ept);log_text(" gla=");log_hex(e.linear);log_text("\n");
  if(result==FAT_REJECTED&&trace.stage==3&&e.physical>=UINT64_C(0xfee00000)&&
     e.physical<UINT64_C(0xfee01000)&&e.physical!=UINT64_C(0xfee00020)){
   log_text("OVMF.CACHE_KVM APIC_other_rejection read_only_walk\n");
   apic_id_diagnose(&m,&e); /* Generic owned walk; ID-tagged fields identify reuse. */
   if(e.physical==UINT64_C(0xfee000f0)){
    bool admission=dirty_admit();
    result=fsdt_apply(&svr_dirty,&active_apic,native.gpr,&m,&e,cpu_result.c,cpu_result.d,
       admission,admission,0,cache_sample,dirty_cas,pic_write);
    log_text("OVMF.CACHE_KVM APIC_SVR_dirty admission=");log_hex(admission);
    log_text(" result=");log_hex((uint64_t)(int64_t)result);
    log_text(" consumed=");log_hex(svr_dirty.consumed);
    log_text(" CAS_confirmed=");log_hex(svr_dirty.dirty_known);
    log_text(" complete=");log_hex(svr_dirty.complete);
    log_text(" terminal=");log_hex(svr_dirty.failed);
    log_text(" svr=");log_hex(active_apic.device.apic.irq.svr);
    log_text(" timer_last=");log_hex(active_apic.clock.timer.last);
    log_text(" deadline_bus=");log_hex(active_apic.clock.timer.deadline_bus);
    log_text(" timer_terminal=");log_hex(active_apic.clock.timer.terminal);
    log_text(" failed_effect_not_rolled_back\n");
   }
  }
  if(result==FAT_REJECTED&&trace.stage==3&&e.physical==UINT64_C(0xfee00020)){
   apic_id_diagnose(&m,&e);
   bool admission=dirty_admit(); /* Same fixed EPT, VPID-off, exclusive-vCPU contract. */
   result=faat_apply(&apic_accessed,&active_apic,native.gpr,&m,&e,cpu_result.c,cpu_result.d,
      admission,admission,0,cache_sample,accessed_cas,pic_write);
   log_text("OVMF.CACHE_KVM APIC_ID_accessed admission=");log_hex(admission);
   log_text(" result=");log_hex((uint64_t)(int64_t)result);
   log_text(" consumed=");log_hex(apic_accessed.consumed);
   log_text(" CAS_confirmed=");log_hex(apic_accessed.accessed_known);
   log_text(" complete=");log_hex(apic_accessed.complete);
   log_text(" terminal=");log_hex(apic_accessed.failed);
   log_text(" rax=");log_hex(native.gpr[0]);
   log_text(" failed_effect_not_rolled_back\n");
  }
  if(e.physical==UINT64_C(0xfee003e0)){
   log_text("OVMF.CACHE_KVM virtual_divider_transition result=");log_hex((uint64_t)(int64_t)result);
   log_text(" dcr=");log_hex(active_apic.clock.dcr);log_text(" divisor=");log_hex(active_apic.clock.timer.divisor);
   log_text(" initial=");log_hex(active_apic.clock.timer.initial);log_text(" last=");log_hex(active_apic.clock.timer.last);
   log_text(" period_bus=");log_hex(active_apic.clock.timer.period_bus);log_text(" deadline_bus=");log_hex(active_apic.clock.timer.deadline_bus);
   log_text(" terminal=");log_hex(active_apic.clock.timer.terminal);log_text(" explicit_Syle_nondefault_contract\n");
  }
  return result==FAT_COMMITTED;
 }
 if(reason==54){
  uint64_t secondary,cr0,cr3,cr4,efer;
  if(!rd(0x401e,&secondary)||!rd(0x6800,&cr0)||!rd(0x6802,&cr3)||
   !rd(0x6804,&cr4)||!rd(0x2806,&efer))return false;
  log_text("OVMF.CLOCK_APIC cache_exit reason54 q=");log_hex(q);log_text(" len=");log_hex(len);
  log_text(" secondary=");log_hex(secondary);log_text(" cr0=");log_hex(cr0);log_text(" cr3=");log_hex(cr3);
  log_text(" cr4=");log_hex(cr4);log_text(" efer=");log_hex(efer);log_text(" flags=");log_hex(flags);log_text("\n");
  io_instruction_diag(rip);
  uint64_t dr7,event;uint8_t bytes[15];size_t available=io_instruction_fetch(rip,bytes);
  if(!rd(0x681a,&dr7)||dr7!=0x400||!rd(0x4016,&event)||event||
   !(secondary&(UINT64_C(1)<<6))||!(host.caps.secondary&(UINT64_C(1)<<38))||cache_attempted||!fwb_kvm_environment())return false;
  log_text("OVMF.CACHE_KVM exported_secondary_caps=");log_hex(host.caps.secondary);log_text("\n");
  bool result=fwb_apply(&cache_transaction,(uint32_t)reason,q,rip,(unsigned)len,
   (unsigned)(cs&3),bytes,available,true,true,0,cache_sample,cache_maintain,pic_write);
  log_text("OVMF.CACHE_KVM transaction_result=");log_hex(result);
  log_text(" attempted=");log_hex(cache_attempted);log_text(" terminal=");log_hex(cache_transaction.failed);log_text("\n");
  return result;
 }
 if(len<1||len>15||rip>UINT64_MAX-len)return false;
 if(reason==30){
  unsigned port=(unsigned)((q>>16)&0xffff);
#ifdef SYLE_FIRMWARE_CONSOLE
#ifdef SYLE_FIRMWARE_DISK
#ifdef SYLE_M2_DUAL_VCPU
  if(port==0x22||port==0x23)return linux_imcr_io(native.gpr,rip,q,len);
  if(port==0x4d0||port==0x4d1){
   unsigned chip=port-0x4d0;
   if((q&0xffff)!=0&&(q&0xffff)!=8)return false;
   if((uint16_t)native.gpr[3]!=port)return false;
   if(q&8)native.gpr[0]=(native.gpr[0]&~UINT64_C(255))|firmware_ap.elcr[chip];
   else{
    uint8_t programmed=native.gpr[0]&(chip?0xde:0xf8);
    /* The current UART and disk routes have no programmable ELCR mode. */
    if(!chip&&(programmed&0x30))return false;
    firmware_ap.elcr[chip]=programmed;
   }
   return wr(0x681e,rip+len);
  }
#endif
  if(port==0xcfb)return persistent_config_probe(rip,q,(unsigned)len);
  if(port==0x64||port==0xcf9)return persistent_reset(rip,q,(unsigned)len);
  if(port==0x80||(port>=0x3f8&&port<=0x3ff)||(port>=0x2f8&&port<=0x2ff)||(port>=0x3e8&&port<=0x3ef)||(port>=0x2e8&&port<=0x2ef))return disk_legacy_io(rip,q,(unsigned)len,flags);
  if(port==0x40||(port==0x43&&((uint8_t)native.gpr[0]>>6)!=2))return disk_legacy_io(rip,q,(unsigned)len,flags);
  if((port==0x20||port==0x21||port==0xa0||port==0xa1)&&(disk_pic_active||(!(q&8)&&port==0x20&&(uint8_t)native.gpr[0]==0x11&&irq_session.ready&&irq_session.owner.masked)))return disk_legacy_io(rip,q,(unsigned)len,flags);
  if(port==0x61||port==0x42||(port==0x43&&((uint8_t)native.gpr[0]>>6)==2))return disk_pit2_io(rip,q,(unsigned)len);
  if(disk_port((uint16_t)port))return disk_dispatch(rip,q,(unsigned)len,false);
  if(port>=0xcfc&&port<=0xcff&&(pci.pci.address&0x80ffff00)==0x80001800)return disk_dispatch(rip,q,(unsigned)len,true);
#endif
  if(fcon_port(&console_state,(uint16_t)port))return console_dispatch(rip,q,(unsigned)len);
  if(port>=0xcfc&&port<=0xcff&&(pci.pci.address&0x80ffff00)==0x80001000)
   return console_config(rip,q,(unsigned)len);
#endif
  if(port==0x510||port==0x511||(port>=0x514&&port<=0x51b))return fwcfg_dispatch(rip,q,(unsigned)len,flags,blocking,pending_debug,vector);
  if(port==0x70||port==0x71){
#ifdef SYLE_M2_DUAL_VCPU
   if(port==0x71&&cmos.rtc.index==15&&firmware_ap.handoff){
    uint8_t op[15];size_t count=io_instruction_fetch(rip,op);
    if((q&7)||!fpp_encoding(q,len,op,count)||(!(q&64)&&(uint16_t)native.gpr[3]!=port))return false;
    if(!wr(0x681e,rip+len))return false;
    if(q&8)native.gpr[0]=(native.gpr[0]&~UINT64_C(255))|firmware_ap.shutdown_code;
    else firmware_ap.shutdown_code=(uint8_t)native.gpr[0];
    return true;
   }
#endif
   if(!clock_sync())return false;
   if(q==UINT64_C(0x710008)){
    rtc_data_reads++;
    if(tsc_commits&&!rtc_first_stack_done&&cmos.rtc.index==0){
     rtc_first_stack_done=true;if(!rtc_observation("first_postTSC_seconds"))return false;
    }
   }
   if(q==UINT64_C(0x700008)){
    struct fat_exit e={.rip=rip,.qualification=q,.reason=30,.rflags=flags,
     .blocking=blocking,.pending_debug=pending_debug,.vectoring=(uint32_t)vector};
    uint64_t dr7,event;uint8_t bytes[15];size_t available=io_instruction_fetch(rip,bytes);
    log_text("OVMF.CMOS_INDEX actual_bytes available=");log_hex(available);
    for(size_t i=0;i<available;i++){log_text(" byte=");log_hex(bytes[i]);}
    log_text(" len=");log_hex(len);log_text(" actual_dx=");log_hex(native.gpr[3]&0xffff);
    log_text(" index=");log_hex(cmos.rtc.index);log_text(" private_nmi_mask=");log_hex(cmos.rtc.nmi_mask);
    log_text(" flags=");log_hex(flags);log_text(" blocking=");log_hex(blocking);
    log_text(" pending_debug=");log_hex(pending_debug);log_text(" before_rax=");log_hex(native.gpr[0]);log_text("\n");
    if(!irq_session.ready||!irq_identity()||!observation_ok()||
       !rd(0x681a,&dr7)||!rd(0x4016,&event))return false;
    e.dr7=dr7;e.entry_event=(uint32_t)event;
    enum fat_result result=fcri_read(&cmos_index_guard,&cmos,native.gpr,&e,(unsigned)len,
      (unsigned)(cs&3),bytes,available,true,0,pic_write);
    log_text("OVMF.CMOS_INDEX result=");log_hex((uint64_t)(int64_t)result);
    log_text(" after_rax=");log_hex(native.gpr[0]);log_text(" index=");log_hex(cmos.rtc.index);
    log_text(" private_nmi_mask=");log_hex(cmos.rtc.nmi_mask);
    log_text(" owner_pending=");log_hex(irq_session.owner.pending);
    log_text(" owner_ISR=");log_hex(irq_session.owner.in_service);log_text("\n");
    if(result==FAT_FATAL){irq_session.failed=true;fti_quarantine(&irq_session.owner);}
    return result==FAT_COMMITTED;
   }
   if(q==UINT64_C(0x710008)&&(cmos.rtc.index==1||cmos.rtc.index==3||cmos.rtc.index==5)){
    struct fat_exit e={.rip=rip,.qualification=q,.reason=30,.rflags=flags,
     .blocking=blocking,.pending_debug=pending_debug,.vectoring=(uint32_t)vector};
    uint64_t dr7,event;uint8_t bytes[15];size_t available=io_instruction_fetch(rip,bytes);
    log_text("OVMF.CMOS_ALARM_READ actual_bytes available=");log_hex(available);
    for(size_t i=0;i<available;i++){log_text(" byte=");log_hex(bytes[i]);}
    log_text(" len=");log_hex(len);log_text(" actual_dx=");log_hex(native.gpr[3]&0xffff);
    log_text(" index=");log_hex(cmos.rtc.index);log_text(" private_nmi_mask=");log_hex(cmos.rtc.nmi_mask);
    log_text(" flags=");log_hex(flags);log_text(" blocking=");log_hex(blocking);
    log_text(" pending_debug=");log_hex(pending_debug);log_text(" before_rax=");log_hex(native.gpr[0]);log_text("\n");
    if(!irq_session.ready||!irq_identity()||!observation_ok()||
       !rd(0x681a,&dr7)||!rd(0x4016,&event))return false;
    e.dr7=dr7;e.entry_event=(uint32_t)event;
    enum fat_result result=fcra_read(&cmos_alarm_guard,&cmos,native.gpr,&e,(unsigned)len,
      (unsigned)(cs&3),bytes,available,true,0,pic_write);
    log_text("OVMF.CMOS_ALARM_READ result=");log_hex((uint64_t)(int64_t)result);
    log_text(" after_rax=");log_hex(native.gpr[0]);log_text(" index=");log_hex(cmos.rtc.index);
    log_text(" private_nmi_mask=");log_hex(cmos.rtc.nmi_mask);
    log_text(" owner_pending=");log_hex(irq_session.owner.pending);
    log_text(" owner_ISR=");log_hex(irq_session.owner.in_service);log_text("\n");
    if(result==FAT_FATAL){irq_session.failed=true;fti_quarantine(&irq_session.owner);}
    return result==FAT_COMMITTED;
   }
   if(q==UINT64_C(0x710000)&&cmos.rtc.index!=15){
    struct fat_exit e={.rip=rip,.qualification=q,.reason=30,.rflags=flags,
     .blocking=blocking,.pending_debug=pending_debug,.vectoring=(uint32_t)vector};
    uint64_t dr7,event;uint8_t bytes[15];size_t available=io_instruction_fetch(rip,bytes);
    log_text("OVMF.CMOS_FIXED_WRITE actual_bytes available=");log_hex(available);
    for(size_t i=0;i<available;i++){log_text(" byte=");log_hex(bytes[i]);}
    log_text(" len=");log_hex(len);log_text(" actual_dx=");log_hex(native.gpr[3]&0xffff);
    log_text(" index=");log_hex(cmos.rtc.index);log_text(" private_nmi_mask=");log_hex(cmos.rtc.nmi_mask);
    log_text(" flags=");log_hex(flags);log_text(" blocking=");log_hex(blocking);
    log_text(" pending_debug=");log_hex(pending_debug);log_text(" before_rax=");log_hex(native.gpr[0]);log_text("\n");
    if(!irq_session.ready||!irq_identity()||!observation_ok()||
       !rd(0x681a,&dr7)||!rd(0x4016,&event))return false;
    e.dr7=dr7;e.entry_event=(uint32_t)event;
    enum fat_result result=cmos.rtc.index==13?
      fcrv_write(&cmos_valid_guard,&cmos,native.gpr,&e,(unsigned)len,(unsigned)(cs&3),bytes,available,true,0,pic_write):
      fcrs_write(&cmos_rate_guard,&cmos,native.gpr,&e,(unsigned)len,(unsigned)(cs&3),bytes,available,true,0,pic_write);
    log_text("OVMF.CMOS_FIXED_WRITE result=");log_hex((uint64_t)(int64_t)result);
    log_text(" after_rax=");log_hex(native.gpr[0]);log_text(" index=");log_hex(cmos.rtc.index);
    log_text(" private_nmi_mask=");log_hex(cmos.rtc.nmi_mask);
    log_text(" owner_pending=");log_hex(irq_session.owner.pending);
    log_text(" owner_ISR=");log_hex(irq_session.owner.in_service);log_text("\n");
    if(result==FAT_FATAL){irq_session.failed=true;fti_quarantine(&irq_session.owner);}
    return result==FAT_COMMITTED;
   }
   return fcr_io(&cmos,native.gpr,rip,(unsigned)(cs&3),reason,q,(unsigned)len,active_apic.epoch,active_apic.reference,0,pic_write);
  }
  if(port==0x20||port==0x21||port==0xa0||port==0xa1){
   uint64_t flags;if(!rd(0x6820,&flags))return false;
   /* lpic commits RIP itself; do not advance it a second time below. */
   return lpic_io_flags(&pic,native.gpr,rip,(unsigned)(cs&3),reason,q,(unsigned)len,flags,0,pic_write);
  }
  if(!clock_sync())return false;
  uint64_t io_ar;if(!traced_rd(0x4816,&io_ar)||
   (io_ar&0x6000)!=0x2000){
   log_text("OVMF.CLOCK_APIC reject=io_operand_default\n");return false;
  }
  uint8_t io_bytes[15];unsigned io_available=(unsigned)io_instruction_fetch(rip,io_bytes);
  uint64_t before=native.gpr[0];bool accepted=fpp_io(&pci,native.gpr,rip,(unsigned)(cs&3),q,(unsigned)len,active_apic.epoch,active_apic.reference,io_bytes,io_available,true,0,pic_write);
if(!pm_delay_quiet || !accepted){
  log_text("OVMF.CLOCK_APIC pci_cycle port=");log_hex(port);log_text(" q=");log_hex(q);
  log_text(" config_address=");log_hex(pci.pci.address);log_text(" before_rax=");log_hex(before);
  log_text(" after_rax=");log_hex(native.gpr[0]);log_text(" accepted=");log_hex(accepted);log_text("\n");
 }
  if(!accepted){io_instruction_diag(rip);return false;}
if(!pm_delay_quiet){
  log_text("OVMF.CLOCK_APIC pm_state base=");log_hex(pci.pm.pmba);log_text(" enabled=");log_hex(pci.pm.enable);log_text(" ticks=");log_hex(pci.pm.ticks);log_text(" status=");log_hex(pci.pm.timer_status);log_text("\n");
 }
  if(port==0xb008 && rip==0x70c231b && q==0xb008000b){
   pm_delay_reads++;
   /* At this post-IN boundary EBX is the guest target and EAX the actual
    * returned count. Only report crossing after the known PAUSE loop bound it.
    * It proves timer progress, not execution of the following guest branch. */
   if(pm_delay_seen && !pm_delay_report &&
      (uint32_t)native.gpr[1]==pm_delay_target &&
      (((uint32_t)native.gpr[1]-(uint32_t)native.gpr[0])&0x800000)){
    pm_delay_report=true;
    log_text("OVMF.PM_DELAY target_crossed target=");log_hex(native.gpr[1]);
    log_text(" returned=");log_hex(native.gpr[0]);log_text(" pm_ticks=");log_hex(pci.pm.ticks);
    log_text(" reference=");log_hex(active_apic.reference);log_text(" reads=");log_hex(pm_delay_reads);
    log_text(" pauses=");log_hex(pm_delay_pauses);log_text(" branch_not_yet_observed\n");
   }
  }
  return true; /* fpp committed RIP exactly once. */
 }else if(reason==10){
  if(len!=2)return false;
  struct fcp_result r;
  if(!fcats_query(&cpu,tsc_source_ready(),(uint32_t)native.gpr[0],(uint32_t)native.gpr[2],&r))return false;
#ifdef SYLE_M2_DUAL_VCPU
  firmware_cpu_identity((uint32_t)native.gpr[0],0,&r);
#endif
#ifdef SYLE_M2_NESTED
  if(nested_enabled && (uint32_t)native.gpr[0]==1)r.c|=1u<<5;
#endif
  native.gpr[0]=r.a;native.gpr[1]=r.b;native.gpr[2]=r.c;native.gpr[3]=r.d;
  log_text("OVMF.CLOCK_APIC CPUID minimal startup profile\n");
 }else if(reason==28){
  if(mode.ia32e&&!disk_cr_encoding(rip,q,(unsigned)len))return false;
  unsigned cr=q&15,access=(q>>4)&3,reg=(q>>8)&15;
  if((q&~UINT64_C(0xf3f))||(cr!=0&&cr!=3&&cr!=4)||access>1)return false;
  if(access==0){uint64_t v;struct fcm_state n;if(!reg_read(reg,&v))return false;
   bool de_transition=cr==4&&((mode.cr4^v)&8);
   if(de_transition){
    uint8_t bytes[15];size_t count=io_instruction_fetch(rip,bytes);uint64_t hw0,hw4,shadow0,shadow4,mask0,mask4;
    io_instruction_diag(rip);
    if(!irq_session.ready||!irq_identity()||!fboot_cr_encoding(q,(unsigned)len,bytes,count)||!rd(0x6800,&hw0)||!rd(0x6804,&hw4)||
     !rd(0x6004,&shadow0)||!rd(0x6006,&shadow4)||!rd(0x6000,&mask0)||!rd(0x6002,&mask4)||
     mask0!=UINT64_MAX||mask4!=UINT64_MAX||shadow0!=mode.cr0||shadow4!=mode.cr4||
     hw0!=(mode.cr0|(host.caps.cr0_fixed0&~UINT64_C(0x80000001)))||hw4!=(mode.cr4|0x2000))return false;
    log_text("OVMF.IRQ DE_config actual_hw4=");log_hex(hw4);log_text(" shadow4=");log_hex(shadow4);
    log_text(" host_edx=");log_hex(cpu.basic_edx);log_text(" fixed1=");log_hex(host.caps.cr4_fixed1);log_text(" all_MOVDR_failstop\n");
   }
   if(!de_mode_write(&mode,cr==0?FCM_CR0:(cr==3?FCM_CR3:FCM_CR4),v,&n)||!commit_mode(&n))return false;
   if(de_transition){uint64_t hw,shadow;if(!rd(0x6804,&hw)||!rd(0x6006,&shadow)||hw!=(v|0x2000)||shadow!=v)return mode_commit_failed();
    log_text("OVMF.IRQ DE_config readback_hw4=");log_hex(hw);log_text(" shadow4=");log_hex(shadow);log_text(" ISR=");log_hex(irq_session.owner.in_service);log_text(" pending=");log_hex(irq_session.owner.pending);log_text(" configuration_only\n");}
  }
  else if(!reg_write(reg,cr==0?mode.cr0:(cr==3?mode.cr3:mode.cr4)))return false;

 }else if(reason==31||reason==32){
  if((uint32_t)native.gpr[2]==0x17){
   struct fcp_result vendor,signature;uint64_t dr7,event;
   if(!fcats_query(&cpu,tsc_source_ready(),0,0,&vendor)||!fcats_query(&cpu,tsc_source_ready(),1,0,&signature)||
    !traced_rd(0x681a,&dr7)||!traced_rd(0x4016,&event)||dr7!=0x400||event)return false;
   bool intel=vendor.b==0x756e6547&&vendor.d==0x49656e69&&vendor.c==0x6c65746e;
   uint8_t bytes[15];size_t available=io_instruction_fetch(rip,bytes);
   io_instruction_diag(rip);
   if(!clock_sync())return false;
   bool accepted=fpid_apply(&platform_id,signature.a,intel,native.gpr,rip,(unsigned)len,
    (unsigned)(cs&3),(uint32_t)reason,bytes,available,true,0,pic_write);
   log_text("OVMF.CLOCK_APIC virtual_platform_id signature=");log_hex(signature.a);
   log_text(" intel=");log_hex(intel);log_text(" accepted=");log_hex(accepted);log_text("\n");
   return accepted;
  }
  if((uint32_t)native.gpr[2]==0x1b){
   if(len!=2||active_apic.failed)return false;
   uint64_t value=(uint32_t)native.gpr[0]|((uint64_t)(uint32_t)native.gpr[3]<<32);
   struct fag_state staged=active_apic.device;
   if(!fag_base(&staged,reason==32,&value))return false;
   if(!wr(0x681e,rip+len)){active_apic.failed=true;return false;}
   active_apic.device=staged;
   if(reason==31){native.gpr[0]=(uint32_t)value;native.gpr[3]=(uint32_t)(value>>32);}
   return true;
  }
  if((uint32_t)native.gpr[2]!=0xc0000080){
#ifdef SYLE_M2_NESTED
   /* Unsupported MSRs fault in L1; they never reach host RDMSR/WRMSR. */
   if(nested_enabled && disk_pic_active && len==2 && !disk_gp.pending && !disk_pic_run.inflight &&
      !irq_session.active_attempt && !halt_state.halted){
    uint64_t event;
    if(!rd(0x4016,&event)||event||!wr(0x4018,0)||!wr(0x401a,0)||
       !wr(0x6820,flags|0x10000)||!wr(0x4016,0x80000b0d))return false;
    disk_gp.pending=true;
    return true;
   }
#endif
   return false;
  }
  if(reason==31){native.gpr[0]=(uint32_t)mode.efer;native.gpr[3]=(uint32_t)(mode.efer>>32);}
  else {struct fcm_state n;uint64_t v=(uint32_t)native.gpr[0]|((uint64_t)(uint32_t)native.gpr[3]<<32);if(!fcp_efer_allowed(v)||!de_mode_write(&mode,FCM_EFER,v,&n)||!commit_mode(&n))return false;}
 }else return false;
 if(!wr(0x681e,rip+len))return mode_commit_failed();
 return true;
}
static uint64_t fmg_observed,fmg_reason;
static bool fmg_io_failed;
static int fmg_failed_step(unsigned);
#ifdef SYLE_M2_DUAL_VCPU
static bool prepare_parked_ap(void){
 if(firmware_ap.page||!lvs_init(&firmware_ap.state))return false;
#ifdef SYLE_M2_HOST_PARALLEL
 if(!fmp_call(FMP_OPEN,0,0)){
  log_text("FMG.HOST_AP_OPEN_FAILED stage=");log_hex(fmp.host.open_stage);log_text("\n");
  return false;
 }
 firmware_ap.page=fmp.host.vmcs_page;
 fmp_calibrate_tsc();
 if(firmware_ap.page==host.vmcs_page||fmp.host.vmxon_page==host.vmxon_page||
    fmp.host.host.tr_base==host.host.tr_base||fmp.host.host.idt_base==host.host.idt_base)fatal();
 log_text("FMG.HOST_AP_OWNER vmcs=");log_hex(firmware_ap.page);
 log_text(" vmxon=");log_hex(fmp.host.vmxon_page);
 log_text(" ept=");log_hex(fmp.host.eptp);log_text("\n");
#else
 if(!lvn_bind(&firmware_ap.native,&firmware_ap.state_ops))return false;
 uint64_t page=alloc(0);
 if(!page)return false;
 if(page==host.vmxon_page||page==host.vmcs_page||fwm_owned_page(&mem,page))fatal();
 uint32_t *region=map(0,page);
 unsigned bits=(host.basic&(1ull<<48))?32:host.caps.physical_bits;
 if(!region||(page&4095)||(page>>bits)||!wb(0,page)){
  if(!release(0,page))fatal();
  return false;
 }
 for(unsigned i=0;i<1024;i++)region[i]=0;
 region[0]=host.basic&0x7fffffff;
 if(!ops.vmclear(ops.context,page))fatal();
 firmware_ap.page=page;
#endif
 firmware_ap.mode.cr0=0x60000010;
 guest_apic_init_mode(&firmware_ap.apic,0,false);
 if(!ft100_mmio_init(&firmware_ap.timer,active_apic.epoch,active_apic.reference,clock_source.max_gap))return false;
#ifdef SYLE_M2_HOST_PARALLEL
 for(unsigned i=0;i<15;i++)firmware_ap.native.gpr[i]=fmp.host.plan.gpr[i];
 return true;
#else
 if(!lvb_switch_current(&backend,host.vmcs_page,page))return false;
 bool ok=fwh_program_reset(&host,(uintptr_t)(fallback_stack+sizeof fallback_stack),(uintptr_t)fallback);
 for(unsigned i=0;i<15;i++)firmware_ap.native.gpr[i]=host.plan.gpr[i];
 if(!lvb_switch_current(&backend,page,host.vmcs_page))fatal();
 return ok;
#endif
}
#endif
static bool fmg_prepare_runtime(void){
#ifdef SYLE_M2_NESTED
 for(unsigned i=0;i<NESTED_L1_CPUS;i++){
  struct nested_l1_context *ctx=&nested_contexts[i];
  *ctx=(struct nested_l1_context){.vmcs={.vmcs_region=NVMX_NONE,.current=NVMX_NONE,.physical_bits=32},
   .last_field=NVMX_NONE,.l1_mode=&mode,.l1_native=&native,.l1_state=&state,.l1_ops=&state_ops,
   .l1_page=&host.vmcs_page,.debug=&disk_debug_bank};
#ifdef SYLE_M2_DUAL_VCPU
  if(i){ctx->l1_mode=&firmware_ap.mode;ctx->l1_native=&firmware_ap.native;
   ctx->l1_state=&firmware_ap.state;ctx->l1_ops=&firmware_ap.state_ops;ctx->l1_page=&firmware_ap.page;ctx->debug=&firmware_ap.debug;}
#endif
 }
 nested_current=nested_contexts;
#endif
 if(!foc_init(&observation_trace,observation_bytes,sizeof observation_bytes,0,observation_sink))return false;
 log_text("OVMF.CACHE_KVM nondefault_single_maintenance_fixture\n");
 log_text("OVMF.CLOCK_APIC begin NONDEFAULT timer32 IRQ experiment; host-tested ownership, native delivery unverified\n");
 if(!lvn_bind(&native,&state_ops)){log_text("OVMF.CLOCK_APIC fail=native_bind\n");return false;}
 if(!lvs_init(&state)){log_text("OVMF.CLOCK_APIC fail=state_init\n");return false;}
#ifdef SYLE_M2_HOST_PARALLEL
 if(!lvb_bind_owned(&backend,&memory_ops,&ops,false)||!lvb_is_owner(&backend)||
    backend.apic_id!=fmp.bsp_id)return false;
#else
 if(!lvb_bind(&backend,&memory_ops,&ops)){log_text("OVMF.CLOCK_APIC fail=backend_bind\n");return false;}
#endif
 if(!cpu_init()){log_text("OVMF.CLOCK_APIC fail=cpu_profile_host\n");return false;}
 lpic_init(&pic);
 if(!fag_init(&active_apic.device)||!clock_init()){log_text("OVMF.CLOCK_APIC source_admission_failed\n");return false;}
 /* This target has no PIC/PIT/UART IRQ producer, LINT signal source or
  * router. Enabling this contract is NOT an interrupt-delivery feature. */
 if(!flq_pic_quiet(&pic)||!fag_lint0_quiet_enable(&active_apic.device)||
 !fag_lint1_quiet_enable(&active_apic.device))return false;
 if(!fpp_init(&pci,active_apic.epoch,active_apic.reference,clock_source.max_gap))return false;
 log_text("OVMF.CLOCK_APIC bindings ready\n");
 if(!fwm_prepare(&mem,&memory_ops,firmware_probe_start,(size_t)(firmware_probe_end-firmware_probe_start),firmware_vars_start,(size_t)(firmware_vars_end-firmware_vars_start),ops.physical_bits)){log_text("OVMF.CLOCK_APIC fail=memory_prepare\n");if(!fwm_release(&mem))fatal();return false;}
#ifdef SYLE_FIRMWARE_DISK
 struct vro_io disk_io;
 if(!disk_legacy_init()||!disk_bytes||!vfr_bind(&disk_ram,&mem,&disk_io)||!vrt_init(&disk_state,disk_io,disk_bytes,disk_size))return false;
#endif
 if(!fvr_init(&vars_rom,&mem)){log_text("OVMF.CACHE_KVM fail=vars_ROM_contract\n");return false;}
 log_text("OVMF.CACHE_KVM vars_ROM_candidate=");log_hex(vars_rom.address);log_text(" original=");log_hex(vars_rom.original);log_text("\n");
#ifdef SYLE_FIRMWARE_CODE_ROM
 if(!fcode_init(&code_rom,&mem)){log_text("OVMF.CODE_ROM fail=contract\n");return false;}
 log_text("OVMF.CODE_ROM candidate=");log_hex(code_rom.address);
 log_text(" original=");log_hex(code_rom.original);log_text("\n");
#endif
 if(!fcr_init(&cmos,&mem.layout,active_apic.epoch,active_apic.reference)){log_text("OVMF.CLOCK_APIC fail=cmos_memory_layout\n");return false;}
 if(!enable())return false;
 if(!fwh_open(&host,&ops,&mem,(uint64_t)(uintptr_t)(fallback_stack+sizeof fallback_stack),(uint64_t)(uintptr_t)fallback))return false;
 unsigned cpu_count=1;
#ifdef SYLE_M2_DUAL_VCPU
 if(!prepare_parked_ap())return false;
 cpu_count++;
#endif
 if(!ffc_init(&fwcfg.stream,mem.layout.ram.size,cpu_count))return false;
#ifdef SYLE_FIRMWARE_HALT
 uint64_t halt_misc;
 if(!ops.read_msr(ops.context,0x485,&halt_misc))return false;
 halt_supported=(halt_misc&0x40)!=0;
 log_text("OVMF.HLT capability misc=");log_hex(halt_misc);log_text(" supported=");log_hex(halt_supported);log_text("\n");
 if(!halt_supported)return false;
#endif
 uint64_t pin_controls;if(!rd(0x4000,&pin_controls)||!fag_host_events_exit(pin_controls))return false;
 log_text("OVMF.CLOCK_APIC host_external_nmi_exiting pin=");log_hex(pin_controls);log_text("\n");
 if(!FW_SHARED_INVEPT()||host.caps.timer_shift>24)return false;
 for(unsigned i=0;i<15;i++)native.gpr[i]=host.plan.gpr[i];
 mode=(struct fcm_state){.cr0=0x60000010};
 if(!fdr_init(&debug_bank,host.vmcs_page,host.vmcs_page)||
    !ftsc_init(&virtual_tsc,host.vmcs_page,active_apic.epoch,active_apic.clock.timer.origin,clock_source.max_gap,clock_source.properties.cpu))return false;
 log_text("OVMF.TSC virtual_hz=10MHz boot_origin=");log_hex(virtual_tsc.origin);log_text(" epoch=");log_hex(virtual_tsc.epoch);log_text(" cpu=");log_hex(virtual_tsc.cpu);log_text("\n");
 fmg_observed=0;
 log_text("M2 continuous runtime, 64 entries per management call\n");
 return true;
}
static int fmg_run_entries(void){
 for(unsigned slice=0;slice<64;slice++){
#ifdef SYLE_M2_DUAL_VCPU
  if(firmware_ap.handoff&&!firmware_ap.initialized&&!firmware_ap.runnable){
   uint64_t now;uint8_t v;
   if(!linux_lapic_poll(&firmware_ap.apic,&now))return fmg_failed_step(__LINE__);
   firmware_ap.runnable=firmware_ap.halt_if&&guest_apic_select(&firmware_ap.apic,&v);
  }
  bool pair_l1=false;
#ifdef SYLE_M2_HOST_PARALLEL
  pair_l1=firmware_ap.runnable&&firmware_ap.handoff&&disk_pic_active;
#ifdef SYLE_M2_NESTED
  pair_l1=pair_l1&&(!nested_enabled||(!nested_contexts[0].entry.suspended&&!nested_contexts[1].entry.suspended));
#endif
#endif
  if(firmware_ap.runnable&&!pair_l1&&!firmware_ap_step(NULL))return fmg_failed_step(__LINE__);
#endif
#if defined(SYLE_M2_DUAL_VCPU) && defined(SYLE_M2_NESTED)
  if(nested_enabled&&nested_current->entry.suspended){
   if(!clock_sync()||!nested_resume_slice())return fmg_failed_step(__LINE__);
   continue;
  }
#endif
  if(fmg_observed==UINT64_MAX)return fmg_failed_step(__LINE__);

  console_sample_quiet=irq_session.ready;
  /* Bounded diagnostic expansion only; same per-entry execution slice. */
  if(!observation_ok()||!clock_sync()||!disk_tick()||active_apic.failed||!fag_valid(&active_apic.device))return fmg_failed_step(__LINE__);
  struct fcmd_gate entry_debug;
  uint64_t flags;if(debug_bank.failed||!de_gate(&entry_debug)||!rd(0x6820,&flags)||(flags&0x100))return fmg_failed_step(__LINE__);
#ifdef SYLE_FIRMWARE_DISK
  if(!(disk_pic_active?disk_pic_entry_ok(flags):lpic_entry_ok(&pic,flags)))return fmg_failed_step(__LINE__);
#else
  if(!lpic_entry_ok(&pic,flags))return fmg_failed_step(__LINE__);
#endif
  uint64_t entry_event,vectoring,pending;
  if(active_apic.device.lint0_extint_deasserted&&(!flq_pic_quiet(&pic)||
   !rd(0x4016,&entry_event)||entry_event||(native.resume&&(!rd(0x4408,&vectoring)||(vectoring&(irq_session.ready?UINT64_C(0x80000000):UINT64_MAX))))||
   !rd(0x6822,&pending)||pending)){log_text("OVMF.CLOCK_APIC reject=lint0_entry_contract\n");return fmg_failed_step(__LINE__);}
#ifdef SYLE_FIRMWARE_DISK
  if(disk_pic_active&&!fmg_input()){log_text("OVMF.DISK input_before_reservation_failed\n");return fmg_failed_step(__LINE__);}
#endif
  bool pic_connected=true;
  unsigned lapic_vector=0;
#ifdef SYLE_M2_DUAL_VCPU
  /* IMCR reset mode wires the 8259 directly to BSP INTR. In APIC mode
   * the same physical line instead enters the BSP through LINT0. */
  if(firmware_ap.handoff&&firmware_ap.imcr)pic_connected=(firmware_ap.bsp_linux.irq.svr&0x100)&&((firmware_ap.bsp_linux.lvt[3]&0x10700)==0x700);
#endif
  (void)pic_connected;
  uint64_t irq_attempt=0,window_attempt=0;bool staged=false,windowed=false;
#ifdef SYLE_FIRMWARE_HALT
  bool halt_receipt=false;
#endif
#ifdef SYLE_FIRMWARE_DISK
  if(disk_gp_pending()){if(!disk_gp_entry())return fmg_failed_step(__LINE__);}else
  if(disk_pic_active){
#ifdef SYLE_M2_DUAL_VCPU
   if(firmware_ap.handoff&&!disk_pic_window&&!linux_lapic_before(&firmware_ap.bsp_linux,&lapic_vector))return fmg_failed_step(__LINE__);
#endif
   if(!disk_tick()||(!lapic_vector&&pic_connected&&!disk_pic_before(&staged)))return fmg_failed_step(__LINE__);
  }else
#endif
  if(irq_session.ready){
   struct lei_snapshot snapshot;uint32_t primary;bool priority;
   if(!irq_snapshot(&snapshot,&primary)||!fiap_select(&active_apic.device,&irq_session,&priority))return fmg_failed_step(__LINE__);
   if(!irq_window.ready&&!fiw_init(&irq_window,host.vmcs_page,host.vmcs_page,primary,host.caps.primary))return fmg_failed_step(__LINE__);
   enum fiw_result gate=fiw_before(&irq_window,host.vmcs_page,host.vmcs_page,primary,
      irq_session.owner.pending,priority,&snapshot,0,irq_write,&window_attempt);
if(!pm_delay_quiet || gate!=FIW_WAIT || irq_session.owner.pending || irq_session.owner.in_service){
   log_text("OVMF.IRQ window_gate=");log_hex((uint64_t)(int64_t)gate);
   log_text(" pending=");log_hex(irq_session.owner.pending);log_text(" priority_ready=");log_hex(priority);
   log_text(" actual_primary=");log_hex(primary);log_text("\n");
 }
   if(gate==FIW_FATAL)return fmg_failed_step(__LINE__);
   if(gate==FIW_ARMED)windowed=true;
   if(gate==FIW_READY){
    if(ftis_before(&irq_session,host.vmcs_page,host.vmcs_page,&snapshot,0,irq_write,&irq_attempt)!=FTIS_STAGED)return fmg_failed_step(__LINE__);
    staged=true;
   }
  }
  uint64_t quantum=UINT64_C(1)<<24;
#ifdef SYLE_M2_DUAL_VCPU
  if(firmware_ap.handoff)quantum=UINT64_C(1)<<20;
#endif
  if(!wr(0x482e,quantum>>host.caps.timer_shift?quantum>>host.caps.timer_shift:1))return fmg_failed_step(__LINE__);
  if(!disk_pic_active&&irq_session.ready){
   uint64_t entry_info,error,length;
   if(!irq_identity()||!rd(0x4016,&entry_info)||!rd(0x4018,&error)||!rd(0x401a,&length)||
      entry_info!=(staged?UINT64_C(0x80000020):0)||error||length)return fmg_failed_step(__LINE__);
   if(staged){log_text("OVMF.IRQ staged_entry_readback=");log_hex(entry_info);log_text(" error_length_zero1 VMCS_profile_verified1\n");}
  }
  if(!observation_ok()){mode_commit_failed();return fmg_failed_step(__LINE__);}
#ifdef SYLE_FIRMWARE_HALT
  if(!halt_entry_check()){log_text("OVMF.HLT entry_state_rejected\n");mode_commit_failed();return fmg_failed_step(__LINE__);}
  bool entered_halted=halt_state.halted;
#endif
#ifdef SYLE_FIRMWARE_CONSOLE
#ifdef SYLE_FIRMWARE_DISK
  if(!disk_pic_active&&!fmg_input()){log_text("OVMF.CONSOLE input_error\n");return fmg_failed_step(__LINE__);}
#else
  if(!console_poll()){log_text("OVMF.CONSOLE input_error\n");return fmg_failed_step(__LINE__);}
#endif
#endif
  native.entry_status=UINT32_MAX;
  enum lvs_result result;
#ifdef SYLE_M2_HOST_PARALLEL
  /* A staged BSP receipt must commit before an AP exit can enqueue a
   * higher-priority IPI or touch the same device. */
  const bool run_ap_after_receipt=pair_l1&&(staged||windowed||lapic_vector||
      disk_pic_window||disk_gp_pending()||entered_halted);
  if(pair_l1&&!run_ap_after_receipt){
   if(!firmware_ap_step(&result))return fmg_failed_step(__LINE__);
  }else
#endif
  result=lvs_run(&state,&state_ops);
  if(!safe())fatal();
#ifdef SYLE_M2_NESTED
  /* A register-only VMCS12 access with L1 interrupts disabled cannot
   * consume a PIC event. Share an empty PIC reservation across a bounded
   * burst. Every entry still saves/restores host state. Return the first
   * other exit, including interrupt-window and preemption, to normal dispatch. */
  if(nested_enabled&&nested_current->vmcs.active&&disk_pic_active&&!lapic_vector&&!staged&&!windowed&&
     !disk_pic_window&&!disk_gp_pending()&&!entered_halted){
   for(unsigned burst=0;burst<16&&slice<63&&result==LVS_EXIT&&native.entry_status==1;burst++){
    uint64_t reason;
#ifdef SYLE_M2_DUAL_VCPU
    if(!rd(0x4402,&reason)||!linux_nested_register_exit(reason))break;
#else
    uint64_t info,rflags;
    if(!rd(0x4402,&reason)||(reason!=23&&reason!=25)||
       !rd(0x440e,&info)||!(info&(1u<<10))||
       !rd(0x6820,&rflags)||(rflags&0x200))break;
#endif
    if(fmg_observed>=UINT64_MAX-1||!dispatch(reason))return fmg_failed_step(__LINE__);
    fmg_observed++;slice++;
    native.resume=1;native.entry_status=UINT32_MAX;
    result=lvs_run(&state,&state_ops);
    if(!safe())fatal();
   }
  }
#endif
  uint64_t raw_reason=0,raw_vector=0,raw_interrupt=0;bool exit_fields=false;
  if(native.entry_status==1&&result==LVS_EXIT){
   exit_fields=rd(0x4402,&raw_reason)&&rd(0x4408,&raw_vector)&&rd(0x4404,&raw_interrupt)&&
      raw_reason<=UINT32_MAX&&raw_vector<=UINT32_MAX&&raw_interrupt<=UINT32_MAX;
  }
#ifdef SYLE_FIRMWARE_DISK
  if(disk_gp_pending()){
   if(!disk_gp_after(native.entry_status==1&&result==LVS_EXIT&&exit_fields&&safe(),(uint32_t)raw_reason,(uint32_t)raw_vector,(uint32_t)raw_interrupt))return fmg_failed_step(__LINE__);
  }else
  if(disk_pic_active){
#ifdef SYLE_M2_DUAL_VCPU
   if(lapic_vector){
    if(!exit_fields||result!=LVS_EXIT||!linux_lapic_after(&firmware_ap.bsp_linux,lapic_vector,raw_reason,raw_vector))return fmg_failed_step(__LINE__);
    staged=halt_receipt=true;
   }else
#endif
   if(pic_connected&&!disk_pic_after(native.entry_status==1&&result==LVS_EXIT&&exit_fields&&safe()&&irq_identity(),(uint32_t)raw_reason,(uint32_t)(raw_vector&0x80000000?raw_vector:0),&halt_receipt))return fmg_failed_step(__LINE__);
  }else
#endif
  if(staged){
   struct fine_outcome evidence={.evidence={
    .observed={host.vmcs_page,host.vmcs_page,irq_attempt},.staged_info=0x80000020,
    .native_status=native.entry_status,.reason=(uint32_t)raw_reason,.vectoring=(uint32_t)raw_vector,
    .exit_interruption=(uint32_t)raw_interrupt,.run_reported_exit=result==LVS_EXIT,
    .host_restored=!state.host_dirty&&safe(),.profile_admitted=irq_identity()},
    .run_result=result,.status_sentinel_set=true,.exit_fields_read=exit_fields};
   enum ftis_result receipt=ftid_after(&irq_session,&evidence,0,irq_write);
   log_text("OVMF.IRQ receipt=");log_hex((uint64_t)(int64_t)receipt);
   log_text(" attempt=");log_hex(irq_attempt);log_text(" native_status=");log_hex(native.entry_status);
   log_text(" reason=");log_hex(raw_reason);log_text(" vectoring=");log_hex(raw_vector);log_text("\n");
   log_text("OVMF.IRQ receipt_owner pending=");log_hex(irq_session.owner.pending);
   log_text(" ISR=");log_hex(irq_session.owner.in_service);log_text(" reserved=");log_hex(irq_session.owner.reserved);
   log_text(" terminal=");log_hex(irq_session.owner.terminal);log_text("\n");
   if(receipt!=FTIS_ACCOUNTED)return fmg_failed_step(__LINE__);
#ifdef SYLE_FIRMWARE_HALT
   halt_receipt=true;
#endif
  }
  if(windowed){
   uint64_t actual_primary;
   if(!exit_fields||!irq_identity()||!rd(0x4002,&actual_primary)||actual_primary>UINT32_MAX||
    fiw_after(&irq_window,host.vmcs_page,host.vmcs_page,window_attempt,(uint32_t)actual_primary,
      (uint32_t)raw_reason,true,0,irq_write)!=FIW_RECHECK)return fmg_failed_step(__LINE__);
  }
  if(result!=LVS_EXIT||!exit_fields){log_text("OVMF.IRQ unproven_exit_stop\n");return fmg_failed_step(__LINE__);}
#ifdef SYLE_FIRMWARE_HALT
  if(entered_halted&&!halt_after_entry(native.entry_status==1&&!state.host_dirty&&safe()&&irq_identity(),staged,halt_receipt))return fmg_failed_step(__LINE__);
#endif
#ifdef SYLE_M2_DUAL_VCPU
  /* Receipts are settled. Finish a short HPET/TSC read sequence here
   * even at the end of a management slice, before running the AP. */
  if(firmware_ap.handoff&&disk_pic_active&&!windowed&&!disk_pic_window&&!disk_gp_pending()){
   for(unsigned burst=0;burst<3&&linux_clock_read_exit(raw_reason);burst++){
    if(fmg_observed>=UINT64_MAX-1||!dispatch(raw_reason))return fmg_failed_step(__LINE__);
    fmg_observed++;slice++;
    native.resume=1;native.entry_status=UINT32_MAX;
    result=lvs_run(&state,&state_ops);
    if(!safe())fatal();
    if(result!=LVS_EXIT||native.entry_status!=1||!rd(0x4402,&raw_reason)||raw_reason>UINT32_MAX)
     return fmg_failed_step(__LINE__);
   }
  }
#endif
#ifdef SYLE_M2_HOST_PARALLEL
  if(run_ap_after_receipt&&!firmware_ap_step(NULL))return fmg_failed_step(__LINE__);
#endif
  fmg_reason=raw_reason;
  if(!clock_sync())return fmg_failed_step(__LINE__);
  if(fmg_reason==7){
#ifdef SYLE_FIRMWARE_DISK
   if(disk_pic_active){
    if(!disk_pic_window||!gw_window_exit(0,disk_read_field,irq_write,disk_pic_run.must_one,disk_pic_run.allowed_one))return fmg_failed_step(__LINE__);
    disk_pic_window=false;native.resume=1;goto fmg_accounted;
   }
#endif
   if(!windowed)return fmg_failed_step(__LINE__);
   native.resume=1;goto fmg_accounted; /* Fresh next gate, no RIP/RF write or receipt. */
  }
  uint64_t rip=0,q=0;rd(0x681e,&rip);rd(0x6400,&q);
  pm_delay_quiet=pm_delay_seen && (fmg_observed&255)!=0 &&
   ((fmg_reason==40 && rip==0x70c236e) || (fmg_reason==30 && rip==0x70c231b && q==0xb008000b));
  if(pm_delay_quiet)pm_delay_quiet_exits++;
  if(!pm_delay_quiet){
  log_text("OVMF.CLOCK_APIC exit reason=");log_hex(fmg_reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text(" cr0=");log_hex(mode.cr0);log_text(" cr4=");log_hex(mode.cr4);log_text(" efer=");log_hex(mode.efer);log_text(" rax=");log_hex(native.gpr[0]);log_text(" rcx=");log_hex(native.gpr[2]);if(fmg_reason==48){uint64_t gpa=0;rd(0x2400,&gpa);log_text(" gpa=");log_hex(gpa);}log_text("\n");
  }
  if(fmg_reason>UINT16_MAX||!dispatch(fmg_reason)){
   console_sample_quiet=false;io_instruction_diag(rip);log_text("OVMF.DISK rejected_rax=");log_hex(native.gpr[0]);log_text(" rcx=");log_hex(native.gpr[2]);log_text(" rdx=");log_hex(native.gpr[3]);log_text("\n");log_text("OVMF.PM_DELAY dispatch_rejected reason=");log_hex(fmg_reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text("\n");return fmg_failed_step(__LINE__);
  }

  native.resume=1;
fmg_accounted:
  fmg_observed++;
  if(fmg_io_failed||persistent_reboot)return fmg_failed_step(__LINE__);
 }
 return 0;
}

#include "firmware_managed.inc"
