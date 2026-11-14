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
#include "firmware_tsc_read.h"
#include "firmware_observer_stack.h"
#include "firmware_debug_read.h"
static struct fdr_bank debug_bank;
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
/* Original runtime logger has no transmission acknowledgement. This true
 * means callback returned, not a guarantee that every UART byte arrived. */
static bool observation_sink(void*unused,const char*bytes,size_t count){
 (void)unused;char chunk[129];
 for(size_t off=0;off<count;){size_t n=count-off;if(n>128)n=128;
  for(size_t i=0;i<n;i++){chunk[i]=bytes[off+i];}
  chunk[n]=0;log_text(chunk);off+=n;}
 return true;
}
static void observation_text(const char*s){if(observation_deferred){size_t n=0;while(s[n])n++;(void)foc_text(&observation_trace,s,n);}else log_text(s);}
static void observation_hex(uint64_t value){if(observation_deferred)(void)foc_hex(&observation_trace,value);else log_hex(value);}
static bool observation_start(void){
 if(!foc_defer(&observation_trace))return false;
 observation_deferred=true;return true;
}
static bool observation_ok(void){return observation_trace.ready&&!observation_trace.failed&&!observation_finished;}
static void observation_finish(void){
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
 log_text(" lossless_compact_v1_same30s_timer_VMbudget_exitcap6144_no_new_clock_sample\n");
}
static void observation_fatal(void){observation_deferred=false;log_text("OVMF.TRACE unsafe_host_fatal_no_drain_guarantee\n");}
#define log_text observation_text
#define log_hex observation_hex
static bool clock_init(void) {
 struct lhc_sample sample;
 if(!linux_host_clock_native_source(&clock_source)||clock_source.properties.hz!=10000000||
 !clock_source.properties.verified||!clock_source.properties.invariant||
 !clock_source.sample(clock_source.ctx,&sample))return false;
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
static bool rd(uint32_t f,uint64_t*v){unsigned char bad;__asm__ volatile("vmread %2,%1;setna %0":"=qm"(bad),"=r"(*v):"r"((uint64_t)f):"cc","memory");return !bad;}
static bool traced_rd(uint32_t f,uint64_t*v){
 if(rd(f,v))return true;
 observation_finish();
 log_text("OVMF.CLOCK_APIC vmread_failed field=");log_hex(f);log_text("\n");return false;
}
static bool wr(uint32_t f,uint64_t v){return ops.vmwrite(ops.context,f,v);}
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
 g->primary_controls<=UINT32_MAX&&g->entry_controls<=UINT32_MAX&&g->exit_controls<=UINT32_MAX&&bitmap<=UINT32_MAX;
}
static bool de_mode_write(enum fcm_register reg,uint64_t value,struct fcm_state*out){
 struct fcmd_gate gate;return de_gate(&gate)&&fcmd_write(&mode,mem.layout.ram.size,reg,value,&gate,out);
}
static bool irq_identity(void){
 struct fcmd_gate gate;if(!de_gate(&gate))return false;
 uint64_t current=UINT64_MAX,primary,secondary,eptp,pin,entry,cr4,flags,expected_primary,expected_pin,expected_entry,efer,cs,cs_ar;unsigned char bad;
 __asm__ volatile("pushfq;pop %0":"=r"(flags));
 __asm__ volatile("vmptrst %1;setna %0":"=qm"(bad),"=m"(current)::"cc","memory");
 return irq_planned(0x4002,&expected_primary)&&irq_planned(0x4000,&expected_pin)&&
 irq_planned(0x4012,&expected_entry)&&!(expected_primary&4)&&
 !bad&&current==host.vmcs_page&&host.ready&&!host.quarantined&&safe()&&
 !(flags&0x200)&&!lvn_armed_rip&&!paging_ap_start_attempted()&&
 rd(0x4002,&primary)&&primary==(expected_primary|(irq_window.armed?4:0))&&(primary&(1u<<31))&&
 rd(0x401e,&secondary)&&secondary==0xc2&&rd(0x201a,&eptp)&&eptp==mem.eptp&&
 rd(0x4000,&pin)&&pin==expected_pin&&!(pin&0x80)&&fag_host_events_exit(pin)&&
 rd(0x4012,&entry)&&entry==(expected_entry|0x200)&&!(entry&~UINT64_C(0xd3ff))&&
 rd(0x2806,&efer)&&efer==0x500&&rd(0x0802,&cs)&&cs&&!(cs&7)&&
 rd(0x4816,&cs_ar)&&(cs_ar&~UINT64_C(1))==0xa09a&&rd(0x6804,&cr4)&&!(cr4&~UINT64_C(0x2668));
}
static bool irq_snapshot(struct lei_snapshot*out,uint32_t*primary){
 uint64_t flags,block,activity,event,vector=0,debug,dr7,p;
 if(!irq_identity()||!rd(0x6820,&flags)||!rd(0x4824,&block)||!rd(0x4826,&activity)||
 !rd(0x4016,&event)||!rd(0x6822,&debug)||!rd(0x681a,&dr7)||!rd(0x4002,&p)||
 (native.resume&&!rd(0x4408,&vector))||debug||dr7!=0x400||!(flags&2)||(flags&~UINT64_C(0x250ed7))||(flags&0x100)||
 (vector&0x80000000)||block>2||activity>1||event||p>UINT32_MAX)return false;
 *out=(struct lei_snapshot){.rflags=flags,.blocking=(uint32_t)block,.activity=(uint32_t)activity};
 *primary=(uint32_t)p;
 log_text("OVMF.IRQ fresh_gate flags=");log_hex(flags);log_text(" blocking=");log_hex(block);
 log_text(" activity=");log_hex(activity);log_text(" vectoring_raw=");log_hex(vector);log_text("\n");
 return true;
}
static bool cache_sample(void*ctx){(void)ctx;return safe()&&clock_sync();}
/* Fixed single stopped-vCPU fixture. Current VMCS, not just the planner,
 * must bind the owned EPT and disable VPID before guest-page-table mutation.
 * VM entry then invalidates VPID0 linear/combined translations. */
static bool dirty_admit(void){
 uint64_t secondary,eptp,primary,flags;
 __asm__ volatile("pushfq;pop %0":"=r"(flags));
 return safe()&&!(flags&0x200)&&!lvn_armed_rip&&!paging_ap_start_attempted()&&
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
 (void)ctx;if(cache_attempted||!safe()||lvn_armed_rip||!fwb_kvm_environment())return false;
 cache_attempted=true; /* Irreversible operation may start; never retry. */
 log_text("OVMF.CACHE_KVM attempting_L1_WBINVD outer_KVM_owns_physical_effect\n");
 bool maintained=fwb_kvm_maintain();
 log_text("OVMF.CACHE_KVM maintenance_returned=");log_hex(maintained);log_text("\n");
 return maintained&&safe();
}
static bool enable(void){
 uint64_t f,lo,hi,v;__asm__ volatile("pushfq;pop %0":"=r"(f));
 __asm__ volatile("mov %%cr0,%0":"=r"(saved_cr0));__asm__ volatile("mov %%cr3,%0":"=r"(saved_cr3));__asm__ volatile("mov %%cr4,%0":"=r"(saved_cr4));
 if((f&0x600)||(saved_cr4&0x2000)||paging_ap_start_attempted())return false;
 if(!lvn_read_msr(&native,0x3a,&v)||(v&5)!=5||!lvn_read_msr(&native,0x488,&lo)||!lvn_read_msr(&native,0x489,&hi))return false;
 v=saved_cr4|0x2000;if((v&lo)!=lo||(v&~hi))return false;
 __asm__ volatile("mov %0,%%cr4"::"r"(v):"memory");changed=true;return true;
}
static bool reg_read(unsigned n,uint64_t*v){static const unsigned ix[16]={0,2,3,1,0,6,4,5,7,8,9,10,11,12,13,14};if(n>15)return false;if(n==4)return rd(0x681c,v);*v=native.gpr[ix[n]];return true;}
static bool reg_write(unsigned n,uint64_t v){static const unsigned ix[16]={0,2,3,1,0,6,4,5,7,8,9,10,11,12,13,14};if(n>15)return false;if(n==4)return wr(0x681c,v);native.gpr[ix[n]]=v;return true;}
static bool mode_commit_failed(void){log_text("OVMF.IRQ mode_commit_failed_effects_not_rolled_back_no_reentry\n");if(irq_session.ready){irq_session.failed=true;fti_quarantine(&irq_session.owner);}return false;}
static bool commit_mode(const struct fcm_state*n){
 uint64_t entry,hw0=n->cr0|(host.caps.cr0_fixed0&~UINT64_C(0x80000001)),hw4=n->cr4|0x2000;
 if((hw0&~host.caps.cr0_fixed1)||(hw4&host.caps.cr4_fixed0)!=host.caps.cr4_fixed0||(hw4&~host.caps.cr4_fixed1)||!rd(0x4012,&entry))return false;
 entry=(entry&~UINT64_C(0x200))|(n->ia32e?0x200:0);
 /* Any partial failure is terminal; never re-enter a mixed-mode VMCS. */
 if(!wr(0x6800,hw0)||!wr(0x6004,n->cr0)||!wr(0x6802,n->cr3)||!wr(0x6804,hw4)||!wr(0x6006,n->cr4)||!wr(0x2806,n->efer)||!wr(0x4012,entry))return mode_commit_failed();
 mode=*n;return true;
}
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
static bool dispatch(uint64_t reason){
 uint64_t rip,len,q,vector,cs;
 if(!traced_rd(0x681e,&rip)||!traced_rd(0x440c,&len)||!traced_rd(0x6400,&q)||!traced_rd(0x4408,&vector))return false;
 if(irq_session.ready&&!(vector&0x80000000)){
  if(vector){log_text("OVMF.IRQ vectoring_invalid_payload=");log_hex(vector);log_text(" ignored_per_valid_bit\n");}
  vector=0; /* Logical helper input only; hardware field and raw receipt unchanged. */
 }
 if(vector){log_text("OVMF.CLOCK_APIC reject=vectoring value=");log_hex(vector);log_text("\n");return false;}
 if(!traced_rd(0x0802,&cs))return false;
 if(cs&3){log_text("OVMF.CLOCK_APIC reject=cpl cs=");log_hex(cs);log_text("\n");return false;}
 uint64_t flags,blocking,pending_debug;
 if(!traced_rd(0x6820,&flags)||!traced_rd(0x4824,&blocking)||!traced_rd(0x6822,&pending_debug))return false;
 if(reason==48||reason==52){log_text("OVMF.CLOCK_APIC instruction_gate reason=");log_hex(reason);log_text(" flags=");log_hex(flags);log_text(" blocking=");log_hex(blocking);log_text(" pending_debug=");log_hex(pending_debug);log_text(" cs=");log_hex(cs);log_text("\n");}
 if(reason==52){
  uint64_t dr7,event;if(!traced_rd(0x681a,&dr7)||!traced_rd(0x4016,&event))return false;
  bool resume=fpr_resume(reason,cs,flags,blocking,pending_debug,dr7,event,vector);
  log_text("OVMF.CLOCK_APIC preemption_resume_unchanged=");log_hex(resume);log_text(" dr7=");log_hex(dr7);log_text(" entry_event=");log_hex(event);log_text("\n");
  return resume; /* No RIP/RFLAGS write: this was not an emulated instruction. */
 }
 /* Only exact CR8 accesses use their own RF retirement and strict instruction gate. */
 if(reason==0){debug_bank.failed=true;log_text("OVMF.IRQ exception_exit_no_debug_state_resume\n");return false;}
 if(reason==29){if(len>15)return false;return debug_read_dispatch(rip,q,(unsigned)len,flags,blocking,pending_debug,(uint32_t)vector);}
 if(reason==28&&(q&15)==8){if(len>15)return false;return irq_cr8_access(rip,q,(unsigned)len,flags,blocking,pending_debug,(uint32_t)vector);}
 if((flags&(UINT64_C(1)<<8))||((flags&(UINT64_C(1)<<16))&&reason!=48)||blocking||pending_debug){log_text("OVMF.CLOCK_APIC reject=instruction_state\n");return false;}
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
   enum fat_result ir=fiap_mmio(&active_apic.device,&irq_session,native.gpr,&m,&e,0,pic_write);
   if(ir==FAT_REJECTED&&e.physical==UINT64_C(0xfee00320)){
    irq_lvt_diagnose(&m,&e);
    bool admitted=irq_identity()&&observation_ok();
    ir=fils_write(&active_apic.device,&irq_session,native.gpr,&m,&e,admitted,0,irq_write);
    log_text("OVMF.IRQ LVT_same_result=");log_hex((uint64_t)(int64_t)ir);
    log_text(" admitted=");log_hex(admitted);log_text(" last=");log_hex(irq_session.owner.last);
    log_text(" deadline=");log_hex(irq_session.owner.deadline_bus);log_text(" pending=");log_hex(irq_session.owner.pending);
    log_text(" ISR=");log_hex(irq_session.owner.in_service);log_text(" terminal=");log_hex(irq_session.owner.terminal);log_text("\n");
   }
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
  if(port==0x510||port==0x511||(port>=0x514&&port<=0x51b))return fwcfg_dispatch(rip,q,(unsigned)len,flags,blocking,pending_debug,vector);
  if(port==0x70||port==0x71){
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
  log_text("OVMF.CLOCK_APIC pci_cycle port=");log_hex(port);log_text(" q=");log_hex(q);
  log_text(" config_address=");log_hex(pci.pci.address);log_text(" before_rax=");log_hex(before);
  log_text(" after_rax=");log_hex(native.gpr[0]);log_text(" accepted=");log_hex(accepted);log_text("\n");
  if(!accepted){io_instruction_diag(rip);return false;}
  log_text("OVMF.CLOCK_APIC pm_state base=");log_hex(pci.pm.pmba);log_text(" enabled=");log_hex(pci.pm.enable);log_text(" ticks=");log_hex(pci.pm.ticks);log_text(" status=");log_hex(pci.pm.timer_status);log_text("\n");
  return true; /* fpp committed RIP exactly once. */
 }else if(reason==10){
  if(len!=2)return false;
  struct fcp_result r;
  if(!fcats_query(&cpu,tsc_source_ready(),(uint32_t)native.gpr[0],(uint32_t)native.gpr[2],&r))return false;
  native.gpr[0]=r.a;native.gpr[1]=r.b;native.gpr[2]=r.c;native.gpr[3]=r.d;
  log_text("OVMF.CLOCK_APIC CPUID minimal startup profile\n");
 }else if(reason==28){
  unsigned cr=q&15,access=(q>>4)&3,reg=(q>>8)&15;
  if((q&~UINT64_C(0xf3f))||(cr!=0&&cr!=3&&cr!=4)||access>1)return false;
  if(access==0){uint64_t v;struct fcm_state n;if(!reg_read(reg,&v))return false;
   bool de_transition=cr==4&&((mode.cr4^v)&8);
   if(de_transition){
    uint8_t bytes[15];size_t count=io_instruction_fetch(rip,bytes);uint64_t hw0,hw4,shadow0,shadow4,mask0,mask4;
    io_instruction_diag(rip);
    if(!irq_session.ready||!irq_identity()||q!=4||len!=3||count<3||bytes[0]!=0x0f||bytes[1]!=0x22||bytes[2]!=0xe0||
     mode.cr4!=0x660||v!=0x668||!rd(0x6800,&hw0)||!rd(0x6804,&hw4)||
     !rd(0x6004,&shadow0)||!rd(0x6006,&shadow4)||!rd(0x6000,&mask0)||!rd(0x6002,&mask4)||
     mask0!=UINT64_MAX||mask4!=UINT64_MAX||shadow0!=mode.cr0||shadow4!=mode.cr4||
     hw0!=(mode.cr0|(host.caps.cr0_fixed0&~UINT64_C(0x80000001)))||hw4!=(mode.cr4|0x2000))return false;
    log_text("OVMF.IRQ DE_config actual_hw4=");log_hex(hw4);log_text(" shadow4=");log_hex(shadow4);
    log_text(" host_edx=");log_hex(cpu.basic_edx);log_text(" fixed1=");log_hex(host.caps.cr4_fixed1);log_text(" all_MOVDR_failstop\n");
   }
   if(!de_mode_write(cr==0?FCM_CR0:(cr==3?FCM_CR3:FCM_CR4),v,&n)||!commit_mode(&n))return false;
   if(de_transition){uint64_t hw,shadow;if(!rd(0x6804,&hw)||!rd(0x6006,&shadow)||hw!=0x2668||shadow!=0x668)return mode_commit_failed();
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
  if((uint32_t)native.gpr[2]!=0xc0000080)return false;
  if(reason==31){native.gpr[0]=(uint32_t)mode.efer;native.gpr[3]=(uint32_t)(mode.efer>>32);}
  else {struct fcm_state n;uint64_t v=(uint32_t)native.gpr[0]|((uint64_t)(uint32_t)native.gpr[3]<<32);if(!fcp_efer_allowed(v)||!de_mode_write(FCM_EFER,v,&n)||!commit_mode(&n))return false;}
 }else return false;
 if(!wr(0x681e,rip+len))return mode_commit_failed();
 return true;
}
bool firmware_probe_run(void){
 bool ok=false;uint64_t reason=0;
 if(!foc_init(&observation_trace,observation_bytes,sizeof observation_bytes,0,observation_sink))return false;
 log_text("OVMF.CACHE_KVM nondefault_single_maintenance_fixture\n");
 log_text("OVMF.CLOCK_APIC begin NONDEFAULT timer32 IRQ experiment; host-tested ownership, native delivery unverified\n");
 if(!lvn_bind(&native,&state_ops)){log_text("OVMF.CLOCK_APIC fail=native_bind\n");return false;}
 if(!lvs_init(&state)){log_text("OVMF.CLOCK_APIC fail=state_init\n");return false;}
 if(!lvb_bind(&backend,&memory_ops,&ops)){log_text("OVMF.CLOCK_APIC fail=backend_bind\n");return false;}
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
 if(!fvr_init(&vars_rom,&mem)){log_text("OVMF.CACHE_KVM fail=vars_ROM_contract\n");goto cleanup;}
 log_text("OVMF.CACHE_KVM vars_ROM_candidate=");log_hex(vars_rom.address);log_text(" original=");log_hex(vars_rom.original);log_text("\n");
 if(!fcr_init(&cmos,&mem.layout,active_apic.epoch,active_apic.reference)){log_text("OVMF.CLOCK_APIC fail=cmos_memory_layout\n");goto cleanup;}
 if(!ffc_init(&fwcfg.stream,mem.layout.ram.size,1))goto cleanup;
 if(!enable())goto cleanup;
 if(!fwh_open(&host,&ops,&mem,(uint64_t)(uintptr_t)(fallback_stack+sizeof fallback_stack),(uint64_t)(uintptr_t)fallback))goto cleanup;
 uint64_t pin_controls;if(!rd(0x4000,&pin_controls)||!fag_host_events_exit(pin_controls))goto cleanup;
 log_text("OVMF.CLOCK_APIC host_external_nmi_exiting pin=");log_hex(pin_controls);log_text("\n");
 if(!ops.invept(ops.context,mem.eptp)||host.caps.timer_shift>24)goto cleanup;
 for(unsigned i=0;i<15;i++)native.gpr[i]=host.plan.gpr[i];
 mode=(struct fcm_state){.cr0=0x60000010};
 if(!fdr_init(&debug_bank,host.vmcs_page,host.vmcs_page)||
    !ftsc_init(&virtual_tsc,host.vmcs_page,active_apic.epoch,active_apic.clock.timer.origin,clock_source.max_gap,clock_source.properties.cpu))goto cleanup;
 log_text("OVMF.TSC virtual_hz=10MHz boot_origin=");log_hex(virtual_tsc.origin);log_text(" epoch=");log_hex(virtual_tsc.epoch);log_text(" cpu=");log_hex(virtual_tsc.cpu);log_text("\n");
 unsigned observed=0;
 for(;observed<6144;observed++){
  /* Bounded diagnostic expansion only; same per-entry execution slice. */
  if(!observation_ok()||!clock_sync()||active_apic.failed||!fag_valid(&active_apic.device))break;
  if(active_apic.reference-active_apic.clock.timer.origin>=UINT64_C(300000000)){
   log_text("OVMF.CLOCK_APIC observation_time_budget=30s reached\n");break;
  }
  struct fcmd_gate entry_debug;
  uint64_t flags;if(debug_bank.failed||!de_gate(&entry_debug)||!rd(0x6820,&flags)||(flags&0x100)||!lpic_entry_ok(&pic,flags))break;
  uint64_t entry_event,vectoring,pending;
  if(active_apic.device.lint0_extint_deasserted&&(!flq_pic_quiet(&pic)||
   !rd(0x4016,&entry_event)||entry_event||(native.resume&&(!rd(0x4408,&vectoring)||(vectoring&(irq_session.ready?UINT64_C(0x80000000):UINT64_MAX))))||
   !rd(0x6822,&pending)||pending)){log_text("OVMF.CLOCK_APIC reject=lint0_entry_contract\n");break;}
  uint64_t irq_attempt=0,window_attempt=0;bool staged=false,windowed=false;
  if(irq_session.ready){
   struct lei_snapshot snapshot;uint32_t primary;bool priority;
   if(!irq_snapshot(&snapshot,&primary)||!fiap_select(&active_apic.device,&irq_session,&priority))break;
   if(!irq_window.ready&&!fiw_init(&irq_window,host.vmcs_page,host.vmcs_page,primary,host.caps.primary))break;
   enum fiw_result gate=fiw_before(&irq_window,host.vmcs_page,host.vmcs_page,primary,
      irq_session.owner.pending,priority,&snapshot,0,irq_write,&window_attempt);
   log_text("OVMF.IRQ window_gate=");log_hex((uint64_t)(int64_t)gate);
   log_text(" pending=");log_hex(irq_session.owner.pending);log_text(" priority_ready=");log_hex(priority);
   log_text(" actual_primary=");log_hex(primary);log_text("\n");
   if(gate==FIW_FATAL)break;
   if(gate==FIW_ARMED)windowed=true;
   if(gate==FIW_READY){
    if(ftis_before(&irq_session,host.vmcs_page,host.vmcs_page,&snapshot,0,irq_write,&irq_attempt)!=FTIS_STAGED)break;
    staged=true;
   }
  }
  if(!wr(0x482e,UINT64_C(1)<<(24-host.caps.timer_shift)))break;
  if(irq_session.ready){
   uint64_t entry_info,error,length;
   if(!irq_identity()||!rd(0x4016,&entry_info)||!rd(0x4018,&error)||!rd(0x401a,&length)||
      entry_info!=(staged?UINT64_C(0x80000020):0)||error||length)break;
   if(staged){log_text("OVMF.IRQ staged_entry_readback=");log_hex(entry_info);log_text(" error_length_zero1 VMCS_profile_verified1\n");}
  }
  if(!observation_ok()){mode_commit_failed();break;}
  native.entry_status=UINT32_MAX;
  enum lvs_result result=lvs_run(&state,&state_ops);if(!safe())fatal();
  uint64_t raw_reason=0,raw_vector=0,raw_interrupt=0;bool exit_fields=false;
  if(native.entry_status==1&&result==LVS_EXIT){
   exit_fields=rd(0x4402,&raw_reason)&&rd(0x4408,&raw_vector)&&rd(0x4404,&raw_interrupt)&&
      raw_reason<=UINT32_MAX&&raw_vector<=UINT32_MAX&&raw_interrupt<=UINT32_MAX;
  }
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
   if(receipt!=FTIS_ACCOUNTED)break;
  }
  if(windowed){
   uint64_t actual_primary;
   if(!exit_fields||!irq_identity()||!rd(0x4002,&actual_primary)||actual_primary>UINT32_MAX||
    fiw_after(&irq_window,host.vmcs_page,host.vmcs_page,window_attempt,(uint32_t)actual_primary,
      (uint32_t)raw_reason,true,0,irq_write)!=FIW_RECHECK)break;
  }
  if(result!=LVS_EXIT||!exit_fields){log_text("OVMF.IRQ unproven_exit_stop\n");break;}
  reason=raw_reason;
  if(!clock_sync())break;
  if(reason==7){
   if(!windowed)break;
   native.resume=1;continue; /* Fresh next gate, no RIP/RF write or receipt. */
  }
  uint64_t rip=0,q=0;rd(0x681e,&rip);rd(0x6400,&q);
  log_text("OVMF.CLOCK_APIC exit reason=");log_hex(reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text(" cr0=");log_hex(mode.cr0);log_text(" cr4=");log_hex(mode.cr4);log_text(" efer=");log_hex(mode.efer);log_text(" rax=");log_hex(native.gpr[0]);log_text(" rcx=");log_hex(native.gpr[2]);if(reason==48){uint64_t gpa=0;rd(0x2400,&gpa);log_text(" gpa=");log_hex(gpa);}log_text("\n");
  if(reason>UINT16_MAX||!dispatch(reason))break;
  native.resume=1;
 }
 if(safe())(void)rtc_observation("terminal_no_new_sample");
 observation_finish();
 if(observed==6144)log_text("OVMF.CLOCK_APIC observation_exit_budget=6144 reached\n");
 log_text("OVMF.CLOCK_APIC completed_dispatches=");log_hex(observed);log_text("\n");
 /* Observation only: never report boot success, even after the exit budget. */
 if(!ok){uint64_t rip=0,err=0,q=0;rd(0x681e,&rip);rd(0x4400,&err);rd(0x6400,&q);log_text("OVMF.CLOCK_APIC failed reason=");log_hex(reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text(" vmerr=");log_hex(err);log_text("\n");}
cleanup:
 observation_finish();
 if(!ok){log_text("OVMF.CLOCK_APIC open_stage=");log_hex(host.open_stage);log_text("\n");}
 if(!lvh_close(&host))fatal();
 if(changed){if(!safe())fatal();__asm__ volatile("mov %0,%%cr4"::"r"(saved_cr4):"memory");changed=false;}
 if(!fwm_release(&mem))fatal();
 log_text("OVMF.CLOCK_APIC STOPPED cleanup, boot not completed\n");return false;
}
