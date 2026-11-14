#include "firmware_vcpu_host.h"
#include "firmware_cpu_mode.h"
#include "firmware_cpu_profile.h"
#include "firmware_pci_probe.h"
#include "linux_pic_mask.h"
#include "linux_disabled_apic.h"
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
 return fcp_supported(&cpu);
}
static _Alignas(16) unsigned char fallback_stack[4096];
extern const unsigned char firmware_vars_start[],firmware_vars_end[];
static uint64_t saved_cr0,saved_cr3,saved_cr4;
static bool changed;
static __attribute__((noreturn)) void fatal(void){panic("OVMF.OBSERVE unsafe host/ownership");}
static __attribute__((noreturn)) void fallback(void){fatal();}
static uint64_t alloc(void*x){(void)x;return pmm_alloc_page();}
static bool release(void*x,uint64_t p){(void)x;return pmm_free_page(p)!=0;}
static void*map(void*x,uint64_t p){(void)x;return paging_phys_to_virt(p);}
static bool wb(void*x,uint64_t p){(void)x;return paging_page_rw_wb(p)&&vmx_memory_region_wb(p);}
static const struct sg_memory_ops memory_ops={0,alloc,release,map,wb};
static bool rd(uint32_t f,uint64_t*v){unsigned char bad;__asm__ volatile("vmread %2,%1;setna %0":"=qm"(bad),"=r"(*v):"r"((uint64_t)f):"cc","memory");return !bad;}
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
 if(!rd(0x681e,&rip)||!rd(0x440c,&len)||!rd(0x6400,&q)||!rd(0x4408,&vector)||vector||!rd(0x0802,&cs)||(cs&3))return false;
 if(reason==52)return true;
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
  if(!fcp_query(&cpu,(uint32_t)native.gpr[0],(uint32_t)native.gpr[2],&r))return false;
  native.gpr[0]=r.a;native.gpr[1]=r.b;native.gpr[2]=r.c;native.gpr[3]=r.d;
  log_text("OVMF.OBSERVE CPUID minimal startup profile\n");
 }else if(reason==28){
  unsigned cr=q&15,access=(q>>4)&3,reg=(q>>8)&15;
  if((q&~UINT64_C(0xf3f))||(cr!=0&&cr!=3&&cr!=4)||access>1)return false;
  if(access==0){uint64_t v;struct fcm_state n;if(!reg_read(reg,&v)||!fcm_write(&mode,mem.layout.ram.size,cr==0?FCM_CR0:(cr==3?FCM_CR3:FCM_CR4),v,&n)||!commit_mode(&n))return false;}
  else if(!reg_write(reg,cr==0?mode.cr0:(cr==3?mode.cr3:mode.cr4)))return false;
 }else if(reason==31||reason==32){
  if((uint32_t)native.gpr[2]==0x1b){
   struct fcp_result r;if(!fcp_query(&cpu,1,0,&r))return false;
   /* Only the disabled-controller read contract; commits RIP exactly once. */
   return lda_read(LCP_BASELINE,r.c,r.d,native.gpr,rip,(unsigned)(cs&3),(uint32_t)reason,(unsigned)len,0,pic_write);
  }
  if((uint32_t)native.gpr[2]!=0xc0000080)return false;
  if(reason==31){native.gpr[0]=(uint32_t)mode.efer;native.gpr[3]=(uint32_t)(mode.efer>>32);}
  else {struct fcm_state n;uint64_t v=(uint32_t)native.gpr[0]|((uint64_t)(uint32_t)native.gpr[3]<<32);if(!fcp_efer_allowed(v)||!fcm_write(&mode,mem.layout.ram.size,FCM_EFER,v,&n)||!commit_mode(&n))return false;}
 }else return false;
 return wr(0x681e,rip+len);
}
bool firmware_probe_run(void){
 bool ok=false;uint64_t reason=0;
 log_text("OVMF.OBSERVE begin fixed OVMF, diagnostic stop is not boot success\n");
 if(!lvn_bind(&native,&state_ops)){log_text("OVMF.OBSERVE fail=native_bind\n");return false;}
 if(!lvs_init(&state)){log_text("OVMF.OBSERVE fail=state_init\n");return false;}
 if(!lvb_bind(&backend,&memory_ops,&ops)){log_text("OVMF.OBSERVE fail=backend_bind\n");return false;}
 if(!cpu_init()){log_text("OVMF.OBSERVE fail=cpu_profile_host\n");return false;}
 pci.address=0;lpic_init(&pic);
 log_text("OVMF.OBSERVE bindings ready\n");
 if(!fwm_prepare(&mem,&memory_ops,firmware_probe_start,(size_t)(firmware_probe_end-firmware_probe_start),firmware_vars_start,(size_t)(firmware_vars_end-firmware_vars_start),ops.physical_bits)){log_text("OVMF.OBSERVE fail=memory_prepare\n");if(!fwm_release(&mem))fatal();return false;}
 if(!enable())goto cleanup;
 if(!fwh_open(&host,&ops,&mem,(uint64_t)(uintptr_t)(fallback_stack+sizeof fallback_stack),(uint64_t)(uintptr_t)fallback))goto cleanup;
 if(!ops.invept(ops.context,mem.eptp))goto cleanup;
 for(unsigned i=0;i<15;i++)native.gpr[i]=host.plan.gpr[i];
 mode=(struct fcm_state){.cr0=0x60000010};
 for(unsigned i=0;i<64;i++){
  uint64_t flags;if(!rd(0x6820,&flags)||!lpic_entry_ok(&pic,flags))break;
  if(!wr(0x482e,UINT64_C(1)<<(28-host.caps.timer_shift)))break;
  enum lvs_result result=lvs_run(&state,&state_ops);if(!safe())fatal();
  if(result!=LVS_EXIT){log_text("OVMF.OBSERVE state_result=");log_hex(result);log_text("\n");break;}
  if(!rd(0x4402,&reason))break;
  uint64_t rip=0,q=0;rd(0x681e,&rip);rd(0x6400,&q);
  log_text("OVMF.OBSERVE exit reason=");log_hex(reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text(" cr0=");log_hex(mode.cr0);log_text(" cr4=");log_hex(mode.cr4);log_text(" efer=");log_hex(mode.efer);log_text(" rax=");log_hex(native.gpr[0]);log_text(" rcx=");log_hex(native.gpr[2]);if(reason==48){uint64_t gpa=0;rd(0x2400,&gpa);log_text(" gpa=");log_hex(gpa);}log_text("\n");
  if(reason>UINT16_MAX||!dispatch(reason))break;
  native.resume=1;
 }
 /* Observation only: never report boot success, even after the exit budget. */
 if(!ok){uint64_t rip=0,err=0,q=0;rd(0x681e,&rip);rd(0x4400,&err);rd(0x6400,&q);log_text("OVMF.OBSERVE failed reason=");log_hex(reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text(" vmerr=");log_hex(err);log_text("\n");}
cleanup:
 if(!ok){log_text("OVMF.OBSERVE open_stage=");log_hex(host.open_stage);log_text("\n");}
 if(!lvh_close(&host))fatal();
 if(changed){if(!safe())fatal();__asm__ volatile("mov %0,%%cr4"::"r"(saved_cr4):"memory");changed=false;}
 if(!fwm_release(&mem))fatal();
 log_text("OVMF.OBSERVE STOPPED cleanup, boot not completed\n");return false;
}
