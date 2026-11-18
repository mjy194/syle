/* End-of-translation-unit lifecycle adapter for the frozen firmware engine. */
#undef log_text
#undef log_hex
#include "firmware_managed.h"
#ifdef SYLE_FIRMWARE_GRAPHICS
#include "firmware_display.h"
#endif
#ifdef SYLE_M2_NESTED
static void nested_irq0_report(void){
 log_text("NESTED.irq0 samples=");log_hex(irq0_cost.samples);
 log_text(" ticks_100ns=");log_hex(irq0_cost.elapsed);
 log_text(" at_least_4ms=");log_hex(irq0_cost.over_period);
 log_text(" l1_vmx_ops=");log_hex(irq0_cost.ops_total);log_text("\n");
}
#endif
#include "guest_control.h"
#include "ring3.h"
#include "managed_host_context.h"
static struct {
 bool installed,owned,prepared,running,failed,paused;
 uint64_t generation,baseline_pages,entries_at_stop,stop_hash;
 uint64_t rx_sent,rx_delivered,tx_produced,tx_received;
 unsigned rx_head,rx_count,tx_head,tx_count;
 uint8_t rx[16],tx[4096];
} fmg;
static void fmg_emit(uint8_t b){
 if(fmg.tx_count==sizeof fmg.tx){fmg_io_failed=true;return;}
 fmg.tx[(fmg.tx_head+fmg.tx_count)%sizeof fmg.tx]=b;fmg.tx_count++;fmg.tx_produced++;
}
static bool fmg_input(void){
 (void)disk_input; /* The managed profile never consumes the host serial port. */
 if(!fmg.rx_count)return true;
 uint8_t b=fmg.rx[fmg.rx_head];
 if(disk_pic_active){
  if(disk_legacy.uart.uart.rc)return true;
  bool accepted=false;if(!lpd_send(&disk_legacy,b,&accepted)||!accepted)return false;
  disk_rx_sent++;
 }else{
  if(!console_state.command||console_state.uart.rc||(console_state.uart.mcr&16))return true;
  if(!fcon_receive(&console_state,b))return false;
 }
 fmg.rx_head=(fmg.rx_head+1)%sizeof fmg.rx;fmg.rx_count--;fmg.rx_delivered++;return true;
}
static uint64_t fmg_root(void*x){(void)x;uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static void fmg_set_root(void*x,uint64_t v){(void)x;__asm__ volatile("mov %0,%%cr3"::"r"(v):"memory");}
static struct mhc_selectors fmg_selectors(void*x){(void)x;struct mhc_selectors s;__asm__ volatile("mov %%ds,%0;mov %%es,%1;mov %%ss,%2":"=r"(s.ds),"=r"(s.es),"=r"(s.ss));return s;}
static void fmg_set_selectors(void*x,struct mhc_selectors s){(void)x;__asm__ volatile("mov %0,%%ds;mov %1,%%es;mov %2,%%ss"::"r"(s.ds),"r"(s.es),"r"(s.ss):"memory");}
static const struct mhc_ops fmg_context_ops={0,fmg_root,fmg_set_root,fmg_selectors,fmg_set_selectors};
static struct mhc_saved fmg_enter(void){struct mhc_saved s={0};if(!mhc_enter(&s,&fmg_context_ops,paging_root()))fatal();return s;}
static void fmg_leave(struct mhc_saved*s){if(!mhc_leave(s,&fmg_context_ops))fatal();}
static int fmg_failed_step(unsigned line){
 fmg.failed=true;console_sample_quiet=false;
#ifdef SYLE_M2_NESTED
 if(nested_enabled&&nested_vmcs.prepared){
  log_text("NESTED.walk=");log_hex(nested_entry.walk_status);log_text(" fill=");log_hex(nested_entry.fill_status);
  log_text(" attempt=");log_hex(nested_entry.attempt);log_text(" vectoring=");log_hex(nested_entry.vectoring);
  log_text(" exit_rip=");log_hex(nested_entry.exit_rip);
  log_text("NESTED.stage=");log_hex(nested_entry.stage);log_text(" field=");log_hex(nested_entry.field);
  log_text(" en=");log_hex(nested_entry.en);log_text(" sec=");log_hex(nested_entry.sec);
  log_text(" event=");log_hex(nested_entry.event);log_text(" rip=");log_hex(nested_entry.rip);log_text(" link=");log_hex(nested_entry.link);
  log_text("NESTED.entry eptp12=");log_hex(nested_entry.eptp12);
  log_text(" entry_loads=");log_hex(nested_entry.entry_count);log_text(" exit_loads=");log_hex(nested_entry.exit_count);
  log_text(" exit_stores=");log_hex(nested_entry.store_count);log_text(" cr0=");log_hex(nested_entry.guest_cr0);
  log_text(" cr3=");log_hex(nested_entry.guest_cr3);log_text(" cr4=");log_hex(nested_entry.guest_cr4);
  log_text(" entered=");log_hex(nested_entry.entered);log_text(" exit02=");log_hex(nested_entry.reason);
  log_text(" qualification=");log_hex(nested_entry.qualification);log_text(" l2_gpa=");log_hex(nested_entry.gpa);
  log_text(" instruction_error=");log_hex(nested_entry.vm_instruction_error);log_text("\n");
 }
 if(nested_enabled){log_text("NESTED.last_field=");log_hex(nested_last_field);log_text("\n");}
#endif
 log_text("FMG.STEP_FAILED generation=");log_hex(fmg.generation);log_text(" line=");log_hex(line);
 log_text(" exits=");log_hex(fmg_observed);log_text(" reason=");log_hex(fmg_reason);
 if(host.active){uint64_t v=0;rd(0x681e,&v);log_text(" rip=");log_hex(v);rd(0x6400,&v);log_text(" qual=");log_hex(v);}
 log_text("\n");return 1;
}
static uint64_t fmg_hash(uint64_t h,const void*p,size_t n){const uint8_t*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}return h;}
/* A diagnostic integrity digest, not a cryptographic security claim. Includes
 * all owned guest RAM, exact current VMCS fields, GPRs and saved guest FX/MSRs.
 * No sampling, dispatch, clock advancement or event mutation occurs here. */
static bool fmg_snapshot(uint64_t*out){
 if(!out||!host.ready||!host.active||!mem.prepared||!safe()||lvn_armed_rip||
 (disk_pic_active&&disk_pic_run.inflight)||irq_session.active_attempt)return false;
 uint64_t current=UINT64_MAX;unsigned char bad;
 __asm__ volatile("vmptrst %1;setna %0":"=qm"(bad),"=m"(current)::"cc","memory");
 if(bad||current!=host.vmcs_page)return false;
 uint64_t h=UINT64_C(1469598103934665603);
 h=fmg_hash(h,&current,sizeof current);h=fmg_hash(h,&mem.eptp,sizeof mem.eptp);
 const uint32_t fields[]={0x681e,0x681c,0x6820,0x6800,0x6802,0x6804,0x2806,0x680e,0x6810,0x0802,0x0804,0x4816,0x4818,0x4016,0x4018,0x401a,0x4824,0x4826,0x4002,0x401e,0x201a};
 for(unsigned i=0;i<sizeof fields/sizeof*fields;i++){uint64_t v;if(!rd(fields[i],&v))return false;h=fmg_hash(h,&v,sizeof v);}
 h=fmg_hash(h,native.gpr,sizeof native.gpr);h=fmg_hash(h,&native.resume,sizeof native.resume);h=fmg_hash(h,&state.guest,sizeof state.guest);
 for(unsigned p=0;p<FWM_RAM_PAGES;p++){
  if(!fwm_owned_page(&mem,mem.ram[p]))return false;
  const void*v=mem.owner.map(mem.owner.context,mem.ram[p]);if(!v)return false;
  h=fmg_hash(h,v,4096);
 }
 for(unsigned p=0;p<mem.flash_pages;p++){
  if(!fwm_owned_page(&mem,mem.flash[p]))return false;
  const void*v=mem.owner.map(mem.owner.context,mem.flash[p]);if(!v)return false;h=fmg_hash(h,v,4096);
 }
 for(unsigned p=0;p<FWM_TABLE_PAGES;p++)if(mem.tables[p]){
  if(!fwm_owned_page(&mem,mem.tables[p]))return false;
  const void*v=mem.owner.map(mem.owner.context,mem.tables[p]);if(!v)return false;h=fmg_hash(h,v,4096);
 }
#define HASH(x) h=fmg_hash(h,&x,sizeof x)
#ifdef SYLE_M2_NESTED
 HASH(nested_enabled);if(nested_enabled)HASH(nested_vmcs);
#endif
 HASH(mode);HASH(irq_session);HASH(irq_window);HASH(halt_state);HASH(pause_state);HASH(active_apic);HASH(virtual_tsc);HASH(pci);HASH(cmos);HASH(fwcfg);HASH(debug_bank);HASH(disk_state);HASH(disk_legacy);HASH(disk_pic_run);HASH(disk_gp);HASH(disk_pit2);HASH(disk_debug_bank);
#undef HASH
 *out=h;return true;
}
/* Legal only after VMCLEAR/INVEPT/VMXOFF, store close and complete RAM release.
 * This is fresh-domain initialization, never a reset of live IRQ/VMCS owners. */
static void fmg_fresh_domain(void){
#define ZERO(x) x=(__typeof__(x)){0}
#ifdef SYLE_M2_NESTED
 ZERO(irq0_cost);
#endif
 ZERO(debug_bank);ZERO(irq_session);ZERO(irq_route);ZERO(irq_window);ZERO(pause_state);ZERO(halt_state);halt_supported=false;halt_eois=halt_last_eoi_wake=0;
 pm_delay_seen=pm_delay_quiet=pm_delay_report=false;pm_delay_target=pm_delay_reads=pm_delay_pauses=pm_delay_quiet_exits=0;
 ZERO(svr_dirty);ZERO(apic_accessed);ZERO(vars_dirty);ZERO(vars_rom);ZERO(code_rom);ZERO(tpm_absent_transaction);ZERO(cache_transaction);cache_attempted=false;
 ZERO(mem);ZERO(backend);ZERO(host);ZERO(native);ZERO(state);ZERO(state_ops);ZERO(ops);ZERO(mode);ZERO(cpu);ZERO(pci);ZERO(platform_id);ZERO(cmos);ZERO(cmos_index_guard);ZERO(cmos_alarm_guard);ZERO(cmos_rate_guard);ZERO(cmos_valid_guard);ZERO(fwcfg);ZERO(pic);ZERO(active_apic);ZERO(clock_source);ZERO(virtual_tsc);
 tsc_commits=rtc_data_reads=0;rtc_first_stack_done=false;ZERO(observation_trace);observation_deferred=observation_finished=console_sample_quiet=false;
 ZERO(console_state);console_count=0;ZERO(disk_state);ZERO(disk_ram);disk_notifies=disk_completions=0;ZERO(disk_microcode);ZERO(disk_pit2);disk_pit2_ready=false;ZERO(disk_debug_bank);ZERO(disk_legacy);ZERO(disk_pic_run);disk_legacy_ready=disk_pic_active=disk_pic_window=false;disk_pic_receipts=disk_irq0_receipts=disk_irq4_receipts=disk_irq5_receipts=disk_rx_sent=disk_rx_read=0;
 ZERO(disk_post);ZERO(disk_gp);ZERO(persistent_store);persistent_reboot=false;persistent_cfb=0;
 saved_cr0=saved_cr3=saved_cr4=0;changed=false;fmg_observed=fmg_reason=0;fmg_io_failed=false;
 fmg.rx_head=fmg.rx_count=fmg.tx_head=fmg.tx_count=0;fmg.rx_sent=fmg.rx_delivered=fmg.tx_produced=fmg.tx_received=0;
#undef ZERO
}
static int fmg_prepare(void*ctx,uint64_t profile){
 (void)ctx;struct mhc_saved saved=fmg_enter();int result=1;
 if(fmg.owned||fmg.generation==UINT64_MAX)goto done;
#ifdef SYLE_M2_NESTED
 if(profile!=SYLE_GUEST_BOOT_PROFILE&&profile!=SYLE_GUEST_NESTED_BOOT_PROFILE)goto done;
 nested_enabled=profile==SYLE_GUEST_NESTED_BOOT_PROFILE;
#else
 if(profile!=SYLE_GUEST_BOOT_PROFILE)goto done;
#endif
 fmg_fresh_domain();
#ifdef SYLE_FIRMWARE_GRAPHICS
 firmware_display_reset();
#endif
 fmg.owned=true;fmg.failed=false;fmg.paused=false;fmg.running=false;fmg.baseline_pages=pmm_get_stats().allocated_pages;fmg.generation++;
 if(!fmg_prepare_runtime()){fmg.failed=true;log_text("FMG.CREATE_FAILED stage=");log_hex(host.open_stage);log_text("\n");goto done;}
 fmg.prepared=true;result=0;
 log_text("FMG.CREATE generation=");log_hex(fmg.generation);log_text(" vmcs=");log_hex(host.vmcs_page);log_text(" ept=");log_hex(mem.eptp);log_text(" allocated_delta=");log_hex(pmm_get_stats().allocated_pages-fmg.baseline_pages);log_text("\n");
 done:fmg_leave(&saved);return result;
}
static int fmg_start(void*ctx){
 (void)ctx;struct mhc_saved saved=fmg_enter();int result=1;
 if(!fmg.prepared||fmg.failed||!safe())goto done;
 if(fmg.paused){
  uint64_t hash;if(fmg_observed!=fmg.entries_at_stop||!fmg_snapshot(&hash)||hash!=fmg.stop_hash){fmg_failed_step(__LINE__);goto done;}
  log_text("FMG.RESUME same_guest=1 generation=");log_hex(fmg.generation);log_text(" entries_unchanged=");log_hex(fmg_observed);log_text(" state_hash=");log_hex(hash);log_text("\n");fmg.paused=false;
 }
 result=fmg_run_entries();
#ifdef SYLE_FIRMWARE_GRAPHICS
 if(!result&&!firmware_display_present(&mem,&fwcfg.graphics,active_apic.reference,false))result=fmg_failed_step(__LINE__);
#endif
 fmg.running=result==0;
 done:if(state.host_dirty||(changed&&!safe()))fatal();fmg_leave(&saved);return result;
}
static int fmg_stop(void*ctx){
 (void)ctx;struct mhc_saved saved=fmg_enter();int result=0;
 if(state.host_dirty||(changed&&!safe()))fatal();
 if(fmg.prepared&&!fmg.failed&&!fmg.paused){
#ifdef SYLE_M2_NESTED
  nested_drop_cpu();nested_drop_ept();
#endif
  if(
#ifdef SYLE_FIRMWARE_GRAPHICS
   !firmware_display_present(&mem,&fwcfg.graphics,active_apic.reference,true)||
#endif
   !persistent_store.flush(persistent_store.ctx)||!fmg_snapshot(&fmg.stop_hash)){result=1;fmg.failed=true;goto done;}
  fmg.entries_at_stop=fmg_observed;fmg.paused=true;
  log_text("FMG.STOP generation=");log_hex(fmg.generation);log_text(" entries=");log_hex(fmg_observed);log_text(" state_hash=");log_hex(fmg.stop_hash);log_text(" running=0 retained=1\n");
 }
 fmg.running=false;
 done:fmg_leave(&saved);return result;
}
static int fmg_release(void*ctx){
 (void)ctx;struct mhc_saved saved=fmg_enter();int result=1;
 if(state.host_dirty||(changed&&!safe()))fatal();
 if(!fmg.owned){result=0;goto done;}
 /* All operations are synchronous; no vCPU or producer is in flight. Disk
  * close failures retain ownership before any backing RAM can be released. */
 if(persistent_store.sectors&&!syle_ata_close())goto done;
#ifdef SYLE_M2_NESTED
 nested_drop_cpu();nested_drop_ept();
#endif
 if(!lvh_close(&host))goto done;
 if(changed){if(!safe())fatal();__asm__ volatile("mov %0,%%cr4"::"r"(saved_cr4):"memory");changed=false;}
#ifdef SYLE_FIRMWARE_GRAPHICS
 struct fdp_stats display=firmware_display_stats();firmware_display_revoke();
 log_text("GFX.RELEASE generation=");log_hex(fmg.generation);log_text(" configs=");log_hex(fwcfg.graphics.configurations);log_text(" dma=");log_hex(fwcfg.graphics.transfers);log_text(" frames=");log_hex(display.frames);log_text(" pixels=");log_hex(display.pixels);log_text(" last_hash=");log_hex(display.last_hash);log_text(" scanout_revoked=");log_hex(!firmware_display_stats().active);log_text("\n");
#endif
 vfr_abort(&disk_ram);
 if(!fwm_release(&mem))goto done;
 if(pmm_get_stats().allocated_pages!=fmg.baseline_pages){log_text("FMG.RELEASE page_balance_failed\n");goto done;}
 observation_deferred=false;observation_finished=true;console_sample_quiet=false;
 log_text("FMG.RELEASE generation=");log_hex(fmg.generation);log_text(" pages_baseline=");log_hex(fmg.baseline_pages);log_text(" pages_after=");log_hex(pmm_get_stats().allocated_pages);
 log_text(" vmx_active=");log_hex(host.active);log_text(" vmxon=");log_hex(host.vmxon_page);log_text(" vmcs=");log_hex(host.vmcs_page);log_text(" ept_released=");log_hex(!mem.prepared);log_text(" adapter_revoked=");log_hex(!disk_ram.active);
 log_text(" reads=");log_hex(disk_state.reads);log_text(" writes=");log_hex(disk_state.writes);log_text(" flushes=");log_hex(disk_state.flushes);log_text(" irq5=");log_hex(disk_irq5_receipts);
 log_text(" rx_sent=");log_hex(fmg.rx_sent);log_text(" rx_delivered=");log_hex(fmg.rx_delivered);log_text(" tx_produced=");log_hex(fmg.tx_produced);log_text(" tx_received=");log_hex(fmg.tx_received);
 log_text(" trace_failed=");log_hex(observation_trace.failed);log_text(" trace_dropped=");log_hex(observation_trace.dropped_bytes);log_text(" trace_encoded=");log_hex(observation_trace.encoded_bytes);log_text(" trace_summary_only1\n");
 fmg.owned=fmg.prepared=fmg.running=fmg.paused=false;result=0;
 done:fmg_leave(&saved);return result;
}
static int fmg_send(void*ctx,uint8_t b){(void)ctx;if(!fmg.prepared||fmg.failed)return -SYLE_GUEST_EBACKEND;if(fmg.rx_count==sizeof fmg.rx)return -SYLE_GUEST_EBUSY;fmg.rx[(fmg.rx_head+fmg.rx_count)%sizeof fmg.rx]=b;fmg.rx_count++;fmg.rx_sent++;return 0;}
static int fmg_recv(void*ctx){(void)ctx;if(!fmg.prepared)return -SYLE_GUEST_EBACKEND;if(!fmg.tx_count)return -SYLE_GUEST_EBUSY;uint8_t b=fmg.tx[fmg.tx_head];fmg.tx_head=(fmg.tx_head+1)%sizeof fmg.tx;fmg.tx_count--;fmg.tx_received++;return b;}
static int fmg_space(void*ctx){(void)ctx;return fmg.prepared&&!fmg.failed?(int)(sizeof fmg.rx-fmg.rx_count):-SYLE_GUEST_EBACKEND;}
static const struct guest_backend fmg_ops={fmg_prepare,fmg_start,fmg_stop,fmg_release,fmg_send,fmg_recv,fmg_space};
bool firmware_managed_install(void){if(fmg.installed||!disk_bytes||!ring3_guest_bind(&fmg_ops,0))return false;fmg.installed=true;log_text("FMG.BACKEND_INSTALLED fixed_UEFI_writable_disk_profile1\n");return true;}
