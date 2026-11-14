#include "firmware_vcpu_host.h"
#include "firmware_cpu_mode.h"
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
static _Alignas(16) unsigned char fallback_stack[4096];
static unsigned char vars[4096];
static uint64_t saved_cr0,saved_cr3,saved_cr4;
static bool changed;
static __attribute__((noreturn)) void fatal(void){panic("FIRMWARE.PROBE unsafe host/ownership");}
static __attribute__((noreturn)) void fallback(void){fatal();}
static uint64_t alloc(void*x){(void)x;return pmm_alloc_page();}
static bool release(void*x,uint64_t p){(void)x;return pmm_free_page(p)!=0;}
static void*map(void*x,uint64_t p){(void)x;return paging_phys_to_virt(p);}
static bool wb(void*x,uint64_t p){(void)x;return paging_page_rw_wb(p)&&vmx_memory_region_wb(p);}
static const struct sg_memory_ops memory_ops={0,alloc,release,map,wb};
static bool rd(uint32_t f,uint64_t*v){unsigned char bad;__asm__ volatile("vmread %2,%1;setna %0":"=qm"(bad),"=r"(*v):"r"((uint64_t)f):"cc","memory");return !bad;}
static bool wr(uint32_t f,uint64_t v){return ops.vmwrite(ops.context,f,v);}
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
static bool dispatch(uint64_t reason,unsigned*stage,bool*done){
 uint64_t rip,len,q,vector,cs;
 if(!rd(0x681e,&rip)||!rd(0x440c,&len)||!rd(0x6400,&q)||!rd(0x4408,&vector)||vector||!rd(0x0802,&cs)||(cs&3))return false;
 if(reason==52)return true;
 if(len<1||len>15||rip>UINT64_MAX-len)return false;
 if(reason==28){
  unsigned cr=q&15,access=(q>>4)&3,reg=(q>>8)&15;
  if((q&~UINT64_C(0xf3f))||(cr!=0&&cr!=3&&cr!=4)||access>1)return false;
  if(access==0){uint64_t v;struct fcm_state n;if(!reg_read(reg,&v)||!fcm_write(&mode,mem.layout.ram.size,cr==0?FCM_CR0:(cr==3?FCM_CR3:FCM_CR4),v,&n)||!commit_mode(&n))return false;}
  else if(!reg_write(reg,cr==0?mode.cr0:(cr==3?mode.cr3:mode.cr4)))return false;
 }else if(reason==31||reason==32){
  if((uint32_t)native.gpr[2]!=0xc0000080)return false;
  if(reason==31){native.gpr[0]=(uint32_t)mode.efer;native.gpr[3]=(uint32_t)(mode.efer>>32);}
  else {struct fcm_state n;uint64_t v=(uint32_t)native.gpr[0]|((uint64_t)(uint32_t)native.gpr[3]<<32);if(!fcm_write(&mode,mem.layout.ram.size,FCM_EFER,v,&n)||!commit_mode(&n))return false;}
 }else if(reason==30){
  static const unsigned marker[3]={0x16,0x32,0x64};
  if((q&0xffff)!=0x40||((q>>16)&0xffff)!=0xe9||q>>32||*stage>=3||(native.gpr[0]&255)!=marker[*stage])return false;
  if((*stage==0&&(mode.cr0&1))||(*stage==1&&(!(mode.cr0&1)||(mode.cr0&(1ull<<31))))||(*stage==2&&!mode.ia32e))return false;
  log_text("FIRMWARE.PROBE marker=");log_hex(marker[*stage]);log_text("\n");++*stage;
 }else if(reason==12){uint64_t ar;if(*stage!=3||!mode.ia32e||!rd(0x4816,&ar)||!(ar&0x2000))return false;*done=true;return true;
 }else return false;
 return wr(0x681e,rip+len);
}
bool firmware_probe_run(void){
 bool ok=false,done=false;unsigned stage=0;uint64_t reason=0;
 log_text("FIRMWARE.PROBE begin tiny reset, not OVMF\n");
 if(!lvn_bind(&native,&state_ops)){log_text("FIRMWARE.PROBE fail=native_bind\n");return false;}
 if(!lvs_init(&state)){log_text("FIRMWARE.PROBE fail=state_init\n");return false;}
 if(!lvb_bind(&backend,&memory_ops,&ops)){log_text("FIRMWARE.PROBE fail=backend_bind\n");return false;}
 log_text("FIRMWARE.PROBE bindings ready\n");
 for(unsigned i=0;i<sizeof vars;i++)vars[i]=0xff;
 if(!fwm_prepare(&mem,&memory_ops,firmware_probe_start,(size_t)(firmware_probe_end-firmware_probe_start),vars,sizeof vars,ops.physical_bits)){log_text("FIRMWARE.PROBE fail=memory_prepare\n");if(!fwm_release(&mem))fatal();return false;}
 if(!enable())goto cleanup;
 if(!fwh_open(&host,&ops,&mem,(uint64_t)(uintptr_t)(fallback_stack+sizeof fallback_stack),(uint64_t)(uintptr_t)fallback))goto cleanup;
 if(!ops.invept(ops.context,mem.eptp))goto cleanup;
 for(unsigned i=0;i<15;i++)native.gpr[i]=host.plan.gpr[i];
 mode=(struct fcm_state){.cr0=0x60000010};
 for(unsigned i=0;i<4096&&!done;i++){
  if(!wr(0x482e,UINT64_C(1)<<(28-host.caps.timer_shift)))break;
  enum lvs_result result=lvs_run(&state,&state_ops);if(!safe())fatal();
  if(result!=LVS_EXIT||!rd(0x4402,&reason)||reason>UINT16_MAX||!dispatch(reason,&stage,&done))break;
  native.resume=1;
 }
 ok=done;
 if(!ok){uint64_t rip=0,err=0,q=0;rd(0x681e,&rip);rd(0x4400,&err);rd(0x6400,&q);log_text("FIRMWARE.PROBE failed reason=");log_hex(reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text(" vmerr=");log_hex(err);log_text("\n");}
cleanup:
 if(!ok){log_text("FIRMWARE.PROBE open_stage=");log_hex(host.open_stage);log_text("\n");}
 if(!lvh_close(&host))fatal();
 if(changed){if(!safe())fatal();__asm__ volatile("mov %0,%%cr4"::"r"(saved_cr4):"memory");changed=false;}
 if(!fwm_release(&mem))fatal();
 log_text(ok?"FIRMWARE.PROBE PASS reset16->protected32->long64 cleanup\n":"FIRMWARE.PROBE NOT_COMPLETED\n");return ok;
}
