#include "smp_acpi.h"
#include "../include/syle/boot.h"
#include "smp_cpu.h"
#include "smp_boot_platform.inc"

static struct {
 _Alignas(64) uint64_t request,done;
 unsigned online;
 unsigned ap_id,bsp_id,host_ipis,previous_ipis;
 volatile uint32_t *lapic;
 struct smp_cpu_fault fault;
 struct lvb_context backend;
 struct lvh_ops ops;
 struct lvh_context host;
 uint64_t saved_cr4,a,b,value,run_reason,tsc_uncertainty;
 enum fmp_operation op;
 enum lvs_result run;
 bool ok,selected,waking;
 uint64_t entries,l1_pairs,nested_pairs,overlap_ticks,shootdowns,cross_ipis;
 uint64_t nested_flushes[2];
} fmp;
static _Alignas(16) unsigned char fmp_stack[32768];

static uint64_t fmp_tsc(void){
 uint32_t low,high;
 __asm__ volatile("lfence;rdtsc;lfence":"=a"(low),"=d"(high)::"memory");
 return ((uint64_t)high<<32)|low;
}
static bool fmp_open_ap(void){
 uint64_t cr0,cr4,low,high;
 if(!lvb_bind_owned(&fmp.backend,&memory_ops,&fmp.ops,true)||
    fmp.backend.apic_id!=fmp.ap_id||!lvb_is_owner(&fmp.backend)||
    !lvn_bind_ap(&firmware_ap.native,&firmware_ap.state_ops)||
    !fmp.ops.read_msr(fmp.ops.context,0x488,&low)||
    !fmp.ops.read_msr(fmp.ops.context,0x489,&high))return false;
 __asm__ volatile("mov %%cr4,%0":"=r"(fmp.saved_cr4));
 cr4=fmp.saved_cr4|0x2000;
 if((cr4&low)!=low||(cr4&~high)||(fmp.saved_cr4&0x2000))return false;
 __asm__ volatile("mov %0,%%cr4"::"r"(cr4):"memory");
 if(!fwh_open(&fmp.host,&fmp.ops,&mem,(uintptr_t)(fmp_stack+sizeof fmp_stack),
              (uintptr_t)fallback))return false;
 __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
 const struct lvp_caps *bsp=&host.caps,*ap=&fmp.host.caps;
 return cr0==fmp.host.host.cr0&&fmp.host.basic==host.basic&&
        ap->pin==bsp->pin&&ap->primary==bsp->primary&&ap->secondary==bsp->secondary&&
        ap->entry==bsp->entry&&ap->exit==bsp->exit&&ap->ept==bsp->ept&&
        ap->cr0_fixed0==bsp->cr0_fixed0&&ap->cr0_fixed1==bsp->cr0_fixed1&&
        ap->cr4_fixed0==bsp->cr4_fixed0&&ap->cr4_fixed1==bsp->cr4_fixed1&&
        ap->physical_bits==bsp->physical_bits&&ap->timer_shift==bsp->timer_shift;
}
static bool fmp_close_ap(void){
 if(fmp.backend.bound&&(!lvb_is_owner(&fmp.backend)||!lvh_close(&fmp.host)))return false;
 if(fmp.saved_cr4){
  uint64_t cr4;
  __asm__ volatile("mov %1,%%cr4;mov %%cr4,%0":"=r"(cr4):"r"(fmp.saved_cr4):"memory");
  if(cr4!=fmp.saved_cr4)return false;
 }
 fmp.saved_cr4=0;
 fmp.backend=(struct lvb_context){0};
 return true;
}
static bool fmp_run_ap(void){
 if(!fmp.host.ready||!fmp.backend.current)return false;
 fmp.run=lvs_run(&firmware_ap.state,&firmware_ap.state_ops);
 fmp.run_reason=UINT64_MAX;
 if(fmp.a&&fmp.run==LVS_EXIT&&!lvb_read(&fmp.backend,0x4402,&fmp.run_reason))return false;
 uint64_t cr0,cr3,cr4;
 __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
 __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
 __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
 return !firmware_ap.state.host_dirty&&cr0==fmp.host.host.cr0&&
        cr3==fmp.host.host.cr3&&cr4==fmp.host.host.cr4;
}
static bool fmp_dispatch(void){
 switch(fmp.op){
#ifdef SYLE_M2_NESTED
 case FMP_READ_BATCH: case FMP_WRITE_BATCH: {
  struct nested_vmcs_batch *batch=(void*)(uintptr_t)fmp.a;
  if(batch->count>LVP_MAX_FIELDS)return false;
  for(unsigned i=0;i<batch->count;i++){
   uint32_t f=batch->entry[i].field;
   if(fmp.op==FMP_WRITE_BATCH?!fmp.ops.vmwrite(fmp.ops.context,f,batch->entry[i].value):
                              !lvb_read(&fmp.backend,f,&batch->entry[i].value))return false;
  }
  return true;
 }
#endif
 case FMP_OPEN:return fmp_open_ap();
 case FMP_CLOSE:return fmp_close_ap();
 case FMP_READ:return lvb_read(&fmp.backend,fmp.a,&fmp.value);
 case FMP_WRITE:return fmp.ops.vmwrite(fmp.ops.context,fmp.a,fmp.b);
 case FMP_SWITCH:return lvb_switch_current(&fmp.backend,fmp.a,fmp.b);
 case FMP_CLEAR:return fmp.ops.vmclear(fmp.ops.context,fmp.a);
 case FMP_SHOOTDOWN:
  if(!lvb_is_owner(&fmp.backend))return false;
  /* fall through */
 case FMP_INVEPT:return fmp.ops.invept(fmp.ops.context,fmp.a);
 case FMP_RESET:return fwh_program_reset(&fmp.host,(uintptr_t)(fmp_stack+sizeof fmp_stack),
                                         (uintptr_t)fallback);
 case FMP_TSC:fmp.value=fmp_tsc();return true;
 case FMP_RUN:return fmp_run_ap();
 }
 return false;
}
static void fmp_interrupt(void *unused){
 (void)unused;
 fmp.lapic[0xb0/4]=0;
 fmp.host_ipis++;
}
static __attribute__((noreturn)) void fmp_ap(void *unused){
 (void)unused;
 smp_cpu_init(&fmp.fault);
 uint64_t cr0,cr4;
 __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
 __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
 cr0|=0x20;
 cr4|=0x600;
 __asm__ volatile("mov %0,%%cr0;mov %1,%%cr4"::"r"(cr0),"r"(cr4):"memory");
 if(cpu_id()!=fmp.ap_id||
    (fmp.lapic[0x20/4]>>24)!=fmp.ap_id)for(;;)__asm__ volatile("cli;hlt");
 smp_cpu_set_ipi(fmp_interrupt,0);
 fmp.lapic[0x80/4]=0;
 fmp.lapic[0xf0/4]=(fmp.lapic[0xf0/4]&~UINT32_C(0x1ff))|0x1ff;
 __atomic_store_n(&fmp.online,1,__ATOMIC_RELEASE);
 uint64_t last=0;
 for(;;){
  uint64_t request=__atomic_load_n(&fmp.request,__ATOMIC_ACQUIRE);
  if(request==last){
   if(fmp.host.active)__asm__ volatile("pause");
   else __asm__ volatile("sti;hlt;cli":::"memory");
   continue;
  }
  /* Consume the hardware IPI before acknowledging its command. All
   * other AP root and VM-entry paths keep host interrupts disabled. */
  if(fmp.waking||fmp.op==FMP_SHOOTDOWN){
   while(fmp.host_ipis==fmp.previous_ipis)
    __asm__ volatile("sti;hlt;cli":::"memory");
   if(fmp.host_ipis!=fmp.previous_ipis+1)break;
  }
  fmp.ok=fmp_dispatch();
  last=request;
  __atomic_store_n(&fmp.done,last,__ATOMIC_RELEASE);
 }
 for(;;)__asm__ volatile("cli;hlt");
}
static bool fmp_faulted(void){
 if(!__atomic_load_n(&fmp.fault.valid,__ATOMIC_ACQUIRE))return false;
 log_text("FMG.HOST_AP_FAULT vector=");log_hex(fmp.fault.vector);
 log_text(" error=");log_hex(fmp.fault.error);
 log_text(" rip=");log_hex(fmp.fault.rip);
 log_text(" operation=");log_hex(fmp.op);log_text("\n");
 return true;
}
bool firmware_managed_host_start(const struct syle_boot_info *boot){
 uint32_t a,b,c,d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
 if(!(c&(1u<<5))||!(d&(1u<<9)))return false;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000000),"c"(0));
 if(a<0x80000001)return false;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000001),"c"(0));
 if((d&0x20100000)!=0x20100000)return false;
 const uint64_t apic=msr(0x1b);
 struct smp_acpi_topology top;
 fmp.bsp_id=cpu_id();
 if((apic&0xd00)!=0x900||!low_owned(boot)||
    !smp_acpi_discover(boot->acpi_rsdp,fmp.bsp_id,read_physical,(void*)boot,&top)||
    top.lapic_base!=(apic&UINT64_C(0xfffff000)))return false;
 fmp.ap_id=top.ap_id;
 fmp.lapic=(void*)(uintptr_t)top.lapic_base;
 const uint64_t low=boot->ap_trampoline.base;
 if(!smp_bootstrap_prepare(paging_phys_to_virt(low),low,paging_root(),
    (uintptr_t)(fmp_stack+sizeof fmp_stack),fmp_ap,0)||
    !paging_smp_prepare(low,top.lapic_base)||
    (fmp.lapic[0x20/4]>>24)!=fmp.bsp_id)return false;
 paging_note_ap_start();
 const uint32_t sipi=0x600|(low>>12);
 if(!icr(fmp.lapic,fmp.ap_id,0xc500)||!delay(11932)||
    !icr(fmp.lapic,fmp.ap_id,0x8500)||!delay(239)||
    !icr(fmp.lapic,fmp.ap_id,sipi)||!delay(239)||
    !icr(fmp.lapic,fmp.ap_id,sipi))return false;
 for(unsigned i=0;i<2000;i++){
  if(__atomic_load_n(&fmp.online,__ATOMIC_ACQUIRE)){
   log_text("FMG.HOST_CPUS bsp=");log_hex(fmp.bsp_id);log_text(" ap=");log_hex(fmp.ap_id);log_text("\n");
   return true;
  }
  if(fmp_faulted()||!delay(1193))return false;
 }
 return false;
}
static bool fmp_idle(void){
 return __atomic_load_n(&fmp.done,__ATOMIC_ACQUIRE)==fmp.request;
}
/* BSP is the only producer. Acknowledgement is the quiescence boundary
 * before shared guest memory, EPT or allocator state can change. */
static void fmp_submit(enum fmp_operation op,uint64_t a,uint64_t b){
 if(!fmp_idle()||fmp.request==UINT64_MAX||fmp_faulted())fatal();
 fmp.op=op;fmp.a=a;fmp.b=b;
 fmp.waking=!fmp.host.active;
 bool interrupt=fmp.waking||op==FMP_SHOOTDOWN;
 if(interrupt)fmp.previous_ipis=fmp.host_ipis;
 __atomic_store_n(&fmp.request,fmp.request+1,__ATOMIC_RELEASE);
 if(interrupt&&!icr(fmp.lapic,fmp.ap_id,0x40))fatal();
}
static bool fmp_wait(void){
 for(uint64_t i=0;i<UINT64_C(100000000);i++){
  if(fmp_idle())return fmp.ok;
  if(fmp_faulted())fatal();
  __asm__ volatile("pause");
 }
 log_text("FMG.HOST_AP_TIMEOUT resources_retained\n");
 fatal();
}
static bool fmp_call(enum fmp_operation op,uint64_t a,uint64_t b){
 fmp_submit(op,a,b);
 return fmp_wait();
}
/* Bound cross-core timestamp skew with the shortest successful round trip.
 * A paired run can claim overlap only above this bound plus entry padding. */
static void fmp_calibrate_tsc(void){
 fmp.tsc_uncertainty=UINT64_MAX;
 for(unsigned i=0;i<16;i++){
  const uint64_t before=fmp_tsc();
  if(!fmp_call(FMP_TSC,0,0))fatal();
  const uint64_t after=fmp_tsc();
  if(before<=fmp.value&&fmp.value<=after&&after-before<fmp.tsc_uncertainty)
   fmp.tsc_uncertainty=after-before;
 }
}
static void fmp_report(void){
 log_text("FMG.HOST_PARALLEL ap_entries=");log_hex(fmp.entries);
 log_text(" l1_overlap_pairs=");log_hex(fmp.l1_pairs);
 log_text(" l2_overlap_pairs=");log_hex(fmp.nested_pairs);
 log_text(" l2_overlap_tsc=");log_hex(fmp.overlap_ticks);
 log_text(" host_tsc_uncertainty=");log_hex(fmp.tsc_uncertainty);
 log_text(" cross_ipis=");log_hex(fmp.cross_ipis);
 log_text(" shared_ept_ack=");log_hex(fmp.shootdowns);
 log_text(" host_ipi_ack=");log_hex(fmp.host_ipis);
 log_text(" bsp_l2_invept=");log_hex(fmp.nested_flushes[0]);
 log_text(" ap_l2_invept=");log_hex(fmp.nested_flushes[1]);
 log_text(" ap_vmx_active=");log_hex(fmp.host.active);
 log_text(" ap_vmxon=");log_hex(fmp.host.vmxon_page);
 log_text(" ap_vmcs=");log_hex(fmp.host.vmcs_page);log_text("\n");
}

static bool fmp_quiescent(void){
 return fmp_idle()&&__atomic_load_n(&fmp.online,__ATOMIC_ACQUIRE)&&
        fmp.bsp_id==(fmp.lapic[0x20/4]>>24)&&!fmp_faulted();
}
static bool fmp_read(uint32_t field,uint64_t *value){
 if(!fmp.selected)return lvb_read(&backend,field,value);
 if(!fmp_call(FMP_READ,field,0))return false;
 *value=fmp.value;
 return true;
}
static bool fmp_write(uint32_t field,uint64_t value){
 return fmp.selected?fmp_call(FMP_WRITE,field,value):ops.vmwrite(ops.context,field,value);
}
static bool fmp_switch(uint64_t from,uint64_t to){
 if(from==host.vmcs_page&&to==firmware_ap.page){
  if(fmp.selected||backend.current!=from||fmp.backend.current!=to)return false;
  fmp.selected=true;
  return true;
 }
 if(from==firmware_ap.page&&to==host.vmcs_page){
  if(!fmp.selected||fmp.backend.current!=from||backend.current!=to)return false;
  fmp.selected=false;
  return true;
 }
 return fmp.selected?fmp_call(FMP_SWITCH,from,to):lvb_switch_current(&backend,from,to);
}
static bool fmp_shared_invept(void){
 if(!fmp_idle()||!ops.invept(ops.context,mem.eptp))return false;
 if(fmp.host.active){
  if(!fmp_call(FMP_SHOOTDOWN,mem.eptp,0))return false;
  fmp.shootdowns++;
 }
 return true;
}
static enum lvs_result fmp_run_remote(void){
 if(!fmp.selected||!fmp_call(FMP_RUN,0,0))fatal();
 if(fmp.run==LVS_EXIT)fmp.entries++;
 return fmp.run;
}

/* Both callers finish VMCS programming before launching either CPU.
 * The AP receipt closes the execution window before exit emulation. */
static enum lvs_result fmp_pair_run(enum lvs_result *bsp_outcome,bool nested){
 fmp_submit(FMP_RUN,1,0);
 *bsp_outcome=lvs_run(&state,&state_ops);
 if(!fmp_wait()||!safe())fatal();
 if(fmp.run==LVS_EXIT)fmp.entries++;
 uint64_t local_reason;
 if(*bsp_outcome==LVS_EXIT&&fmp.run==LVS_EXIT&&
    lvb_read(&backend,0x4402,&local_reason)&&!(local_reason>>16)&&!(fmp.run_reason>>16)){
  const struct lvn_context *remote=&firmware_ap.native;
  const uint64_t begin=native.entry_tsc>remote->entry_tsc?native.entry_tsc:remote->entry_tsc;
  const uint64_t end=native.exit_tsc<remote->exit_tsc?native.exit_tsc:remote->exit_tsc;
  if(end>begin&&end-begin>8192&&end-begin-8192>fmp.tsc_uncertainty){
   if(nested){
    fmp.nested_pairs++;
    fmp.overlap_ticks+=end-begin;
   }else fmp.l1_pairs++;
  }
 }
 return fmp.run;
}

#ifdef SYLE_M2_NESTED
static bool fmp_vmcs_batch(struct nested_vmcs_batch *batch,bool write){
 return fmp.selected&&fmp_call(write?FMP_WRITE_BATCH:FMP_READ_BATCH,(uintptr_t)batch,0);
}
static bool fmp_nested_invept(bool ap,uint64_t eptp){
 if(!fmp_idle())return false;
 bool ok=ap?fmp_call(FMP_INVEPT,eptp,0):ops.invept(ops.context,eptp);
 if(ok)fmp.nested_flushes[ap]++;
 return ok;
}
static enum lvs_result fmp_nested_run(struct nested_l1_context *local,bool pair){
 const bool on_ap=local==&nested_contexts[1];
 struct nested_l1_context *peer=&nested_contexts[on_ap?0:1];
 if(!pair||!peer->entry.suspended||peer->entry.prefetched)
  return on_ap?FW_AP_RUN():lvs_run(&state,&state_ops);
 uint64_t ticks;
 if(!nested_preempt_ticks(peer,&ticks))fatal();
 const uint64_t vmcs01=*peer->l1_page,vmcs02=peer->cpu.page;
 bool switched=on_ap?lvb_switch_current(&backend,vmcs01,vmcs02):
                     fmp_call(FMP_SWITCH,vmcs01,vmcs02);
 if(!switched)fatal();
 bool ready=on_ap?ops.vmwrite(ops.context,0x482e,ticks):
                  fmp_call(FMP_WRITE,0x482e,ticks);
 if(!ready)fatal();
 const uint32_t resume=peer->l1_native->resume;
 peer->l1_native->resume=peer->cpu.launched;
 enum lvs_result outcome;
 if(on_ap)outcome=fmp_pair_run(&peer->entry.outcome,true);
 else peer->entry.outcome=fmp_pair_run(&outcome,true);
 peer->l1_native->resume=resume;
 peer->entry.prefetched=true;
 switched=on_ap?lvb_switch_current(&backend,vmcs02,vmcs01):
                fmp_call(FMP_SWITCH,vmcs02,vmcs01);
 if(!switched)fatal();
 return outcome;
}
#endif
