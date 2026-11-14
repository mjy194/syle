#include "firmware_vcpu_host.h"
#include "firmware_cpu_mode.h"
#include "firmware_cpu_profile.h"
#include "firmware_pci_probe.h"
#include "linux_pic_mask.h"
#include "firmware_active_cpu.h"
#include "firmware_apic_transaction.h"
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
static struct fpi_state pci;
static struct lpic_state pic;
static struct fat_state active_apic;
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
 return fca_supported(&cpu);
}
static _Alignas(16) unsigned char fallback_stack[4096];
extern const unsigned char firmware_vars_start[],firmware_vars_end[];
static uint64_t saved_cr0,saved_cr3,saved_cr4;
static bool changed;
static __attribute__((noreturn)) void fatal(void){panic("OVMF.APIC_GUARD unsafe host/ownership");}
static __attribute__((noreturn)) void fallback(void){fatal();}
static uint64_t alloc(void*x){(void)x;return pmm_alloc_page();}
static bool release(void*x,uint64_t p){(void)x;return pmm_free_page(p)!=0;}
static void*map(void*x,uint64_t p){(void)x;return paging_phys_to_virt(p);}
static bool wb(void*x,uint64_t p){(void)x;return paging_page_rw_wb(p)&&vmx_memory_region_wb(p);}
static const struct sg_memory_ops memory_ops={0,alloc,release,map,wb};
static bool rd(uint32_t f,uint64_t*v){unsigned char bad;__asm__ volatile("vmread %2,%1;setna %0":"=qm"(bad),"=r"(*v):"r"((uint64_t)f):"cc","memory");return !bad;}
static bool traced_rd(uint32_t f,uint64_t*v){
 if(rd(f,v))return true;
 log_text("OVMF.APIC_GUARD vmread_failed field=");log_hex(f);log_text("\n");return false;
}
static bool wr(uint32_t f,uint64_t v){return ops.vmwrite(ops.context,f,v);}
static bool pic_write(void*x,uint32_t f,uint64_t v){(void)x;return wr(f,v);}
static bool safe(void){uint64_t a,b,c;__asm__ volatile("mov %%cr0,%0":"=r"(a));__asm__ volatile("mov %%cr3,%0":"=r"(b));__asm__ volatile("mov %%cr4,%0":"=r"(c));return !state.host_dirty&&a==saved_cr0&&b==saved_cr3&&c==(saved_cr4|(changed?0x2000:0));}
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
static bool commit_mode(const struct fcm_state*n){
 uint64_t entry,hw0=n->cr0|(host.caps.cr0_fixed0&~UINT64_C(0x80000001)),hw4=n->cr4|0x2000;
 if((hw0&~host.caps.cr0_fixed1)||(hw4&host.caps.cr4_fixed0)!=host.caps.cr4_fixed0||(hw4&~host.caps.cr4_fixed1)||!rd(0x4012,&entry))return false;
 entry=(entry&~UINT64_C(0x200))|(n->ia32e?0x200:0);
 /* Any partial failure is terminal; never re-enter a mixed-mode VMCS. */
 if(!wr(0x6800,hw0)||!wr(0x6004,n->cr0)||!wr(0x6802,n->cr3)||!wr(0x6804,hw4)||!wr(0x6006,n->cr4)||!wr(0x2806,n->efer)||!wr(0x4012,entry))return false;
 mode=*n;return true;
}
static bool dispatch(uint64_t reason){
 uint64_t rip,len,q,vector,cs;
 if(!traced_rd(0x681e,&rip)||!traced_rd(0x440c,&len)||!traced_rd(0x6400,&q)||!traced_rd(0x4408,&vector))return false;
 if(vector){log_text("OVMF.APIC_GUARD reject=vectoring value=");log_hex(vector);log_text("\n");return false;}
 if(!traced_rd(0x0802,&cs))return false;
 if(cs&3){log_text("OVMF.APIC_GUARD reject=cpl cs=");log_hex(cs);log_text("\n");return false;}
 uint64_t flags,blocking,pending_debug;
 if(!traced_rd(0x6820,&flags)||!traced_rd(0x4824,&blocking)||!traced_rd(0x6822,&pending_debug))return false;
 if(reason==48){log_text("OVMF.APIC_GUARD ept_gate flags=");log_hex(flags);log_text(" blocking=");log_hex(blocking);log_text(" pending_debug=");log_hex(pending_debug);log_text(" cs=");log_hex(cs);log_text("\n");}
 if((flags&(UINT64_C(1)<<8))||((flags&(UINT64_C(1)<<16))&&reason!=48)||blocking||pending_debug){log_text("OVMF.APIC_GUARD reject=instruction_state\n");return false;}
 if(reason==52)return true;
 if(reason==48){
  struct fmm_memory m={.memory=&mem,.advanced_ept_info=!!(host.caps.ept&(UINT64_C(1)<<22))};struct fat_exit e={.rip=rip,.qualification=q,.reason=48,.vectoring=(uint32_t)vector};
  uint64_t ar,selector,event;struct fcp_result cpu_result;
  if(!traced_rd(0x6800,&m.cr0)||!traced_rd(0x6802,&m.cr3)||!traced_rd(0x6804,&m.cr4)||!traced_rd(0x2806,&m.efer)||
   !traced_rd(0x6808,&m.cs_base)||!traced_rd(0x4816,&ar)||!traced_rd(0x0802,&selector)||!traced_rd(0x681c,&e.rsp)||
   !traced_rd(0x680c,&e.ds_base)||!traced_rd(0x6806,&e.es_base)||!traced_rd(0x680a,&e.ss_base)||
   !traced_rd(0x640a,&e.linear)||!traced_rd(0x2400,&e.physical)||!traced_rd(0x6820,&e.rflags)||
   !traced_rd(0x4824,&e.blocking)||!traced_rd(0x6822,&e.pending_debug)||!traced_rd(0x681a,&e.dr7)||!traced_rd(0x4016,&event))return false;
  if(!fca_query(&cpu,1,0,&cpu_result)){log_text("OVMF.APIC_GUARD reject=cpu_query\n");return false;}
  log_text("OVMF.APIC_GUARD ept_snapshot_ready cr0=");log_hex(m.cr0);log_text(" cr3=");log_hex(m.cr3);log_text(" cr4=");log_hex(m.cr4);log_text(" cs_ar=");log_hex(ar);log_text(" ept_caps=");log_hex(host.caps.ept);log_text("\n");
  m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)selector;e.entry_event=(uint32_t)event;
  struct fat_trace trace;
  enum fat_result result=fat_mmio_traced(&active_apic,native.gpr,&m,&e,cpu_result.c,cpu_result.d,true,0,pic_write,&trace);
  log_text("OVMF.APIC_GUARD decode stage=");log_hex(trace.stage);log_text(" width=");log_hex(trace.width);log_text(" reg=");log_hex(trace.reg);log_text(" write=");log_hex(trace.write);log_text(" value=");log_hex(trace.store_value);log_text(" lvt=");log_hex(trace.lvt);log_text(" initial=");log_hex(trace.initial);log_text(" armed=");log_hex(trace.armed);log_text("\n");
  log_text("OVMF.APIC_GUARD mmio_result=");log_hex((uint64_t)(int64_t)result);log_text(" cs_ar=");log_hex(ar);log_text(" ept_caps=");log_hex(host.caps.ept);log_text(" gla=");log_hex(e.linear);log_text("\n");
  return result==FAT_COMMITTED;
 }
 if(len<1||len>15||rip>UINT64_MAX-len)return false;
 if(reason==30){
  unsigned port=(unsigned)((q>>16)&0xffff);
  if(port==0x20||port==0x21||port==0xa0||port==0xa1){
   uint64_t flags;if(!rd(0x6820,&flags))return false;
   /* lpic commits RIP itself; do not advance it a second time below. */
   return lpic_io_flags(&pic,native.gpr,rip,(unsigned)(cs&3),reason,q,(unsigned)len,flags,0,pic_write);
  }
  if(!fpi_exit(&pci,q,&native.gpr[0]))return false;
 }else if(reason==10){
  if(len!=2)return false;
  struct fcp_result r;
  if(!fca_query(&cpu,(uint32_t)native.gpr[0],(uint32_t)native.gpr[2],&r))return false;
  native.gpr[0]=r.a;native.gpr[1]=r.b;native.gpr[2]=r.c;native.gpr[3]=r.d;
  log_text("OVMF.APIC_GUARD CPUID minimal startup profile\n");
 }else if(reason==28){
  unsigned cr=q&15,access=(q>>4)&3,reg=(q>>8)&15;
  if((q&~UINT64_C(0xf3f))||(cr!=0&&cr!=3&&cr!=4)||access>1)return false;
  if(access==0){uint64_t v;struct fcm_state n;if(!reg_read(reg,&v)||!fcm_write(&mode,mem.layout.ram.size,cr==0?FCM_CR0:(cr==3?FCM_CR3:FCM_CR4),v,&n)||!commit_mode(&n))return false;}
  else if(!reg_write(reg,cr==0?mode.cr0:(cr==3?mode.cr3:mode.cr4)))return false;
 }else if(reason==31||reason==32){
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
  else {struct fcm_state n;uint64_t v=(uint32_t)native.gpr[0]|((uint64_t)(uint32_t)native.gpr[3]<<32);if(!fcp_efer_allowed(v)||!fcm_write(&mode,mem.layout.ram.size,FCM_EFER,v,&n)||!commit_mode(&n))return false;}
 }else return false;
 return wr(0x681e,rip+len);
}
bool firmware_probe_run(void){
 bool ok=false;uint64_t reason=0;
 log_text("OVMF.APIC_GUARD begin active register-only OVMF probe, NO timer/IRQ support\n");
 if(!lvn_bind(&native,&state_ops)){log_text("OVMF.APIC_GUARD fail=native_bind\n");return false;}
 if(!lvs_init(&state)){log_text("OVMF.APIC_GUARD fail=state_init\n");return false;}
 if(!lvb_bind(&backend,&memory_ops,&ops)){log_text("OVMF.APIC_GUARD fail=backend_bind\n");return false;}
 if(!cpu_init()){log_text("OVMF.APIC_GUARD fail=cpu_profile_host\n");return false;}
 pci.address=0;lpic_init(&pic);
 if(!fag_init(&active_apic.device))return false;
 log_text("OVMF.APIC_GUARD bindings ready\n");
 if(!fwm_prepare(&mem,&memory_ops,firmware_probe_start,(size_t)(firmware_probe_end-firmware_probe_start),firmware_vars_start,(size_t)(firmware_vars_end-firmware_vars_start),ops.physical_bits)){log_text("OVMF.APIC_GUARD fail=memory_prepare\n");if(!fwm_release(&mem))fatal();return false;}
 if(!enable())goto cleanup;
 if(!fwh_open(&host,&ops,&mem,(uint64_t)(uintptr_t)(fallback_stack+sizeof fallback_stack),(uint64_t)(uintptr_t)fallback))goto cleanup;
 if(!ops.invept(ops.context,mem.eptp))goto cleanup;
 for(unsigned i=0;i<15;i++)native.gpr[i]=host.plan.gpr[i];
 mode=(struct fcm_state){.cr0=0x60000010};
 for(unsigned i=0;i<64;i++){
  if(active_apic.failed||!fag_valid(&active_apic.device))break;
  uint64_t flags;if(!rd(0x6820,&flags)||!lpic_entry_ok(&pic,flags))break;
  if(!wr(0x482e,UINT64_C(1)<<(28-host.caps.timer_shift)))break;
  enum lvs_result result=lvs_run(&state,&state_ops);if(!safe())fatal();
  if(result!=LVS_EXIT){log_text("OVMF.APIC_GUARD state_result=");log_hex(result);log_text("\n");break;}
  if(!rd(0x4402,&reason))break;
  uint64_t rip=0,q=0;rd(0x681e,&rip);rd(0x6400,&q);
  log_text("OVMF.APIC_GUARD exit reason=");log_hex(reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text(" cr0=");log_hex(mode.cr0);log_text(" cr4=");log_hex(mode.cr4);log_text(" efer=");log_hex(mode.efer);log_text(" rax=");log_hex(native.gpr[0]);log_text(" rcx=");log_hex(native.gpr[2]);if(reason==48){uint64_t gpa=0;rd(0x2400,&gpa);log_text(" gpa=");log_hex(gpa);}log_text("\n");
  if(reason>UINT16_MAX||!dispatch(reason))break;
  native.resume=1;
 }
 /* Observation only: never report boot success, even after the exit budget. */
 if(!ok){uint64_t rip=0,err=0,q=0;rd(0x681e,&rip);rd(0x4400,&err);rd(0x6400,&q);log_text("OVMF.APIC_GUARD failed reason=");log_hex(reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text(" vmerr=");log_hex(err);log_text("\n");}
cleanup:
 if(!ok){log_text("OVMF.APIC_GUARD open_stage=");log_hex(host.open_stage);log_text("\n");}
 if(!lvh_close(&host))fatal();
 if(changed){if(!safe())fatal();__asm__ volatile("mov %0,%%cr4"::"r"(saved_cr4):"memory");changed=false;}
 if(!fwm_release(&mem))fatal();
 log_text("OVMF.APIC_GUARD STOPPED cleanup, boot not completed\n");return false;
}
