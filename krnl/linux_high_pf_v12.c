/* v5 links only the exact consumed-event guard revision. */
/* Reconstructed opt-in bounded CR/PF pinned probe. Trusted fixed inputs, not Linux boot. */
#include "linux_guest_memory.h"
#include "linux_vcpu_cr.h"
#include "linux_pf_guard.h"
#include "linux_gs_commit.h"
#include "linux_cpu_commit.h"
#include "linux_efer_commit.h"
#include "linux_pt_diag.h"
#include "linux_pt_permissions.h"
#include "linux_high_pf.h"
#include "linux_vcpu_backend.h"
#include "linux_vcpu_native.h"
#include "memory.h"
#include "paging.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
extern const unsigned char linux_kernel_start[],linux_kernel_end[],linux_initrd_start[],linux_initrd_end[];
static struct linux_guest_memory memory;
static struct lvh_context host;
static struct lvb_context backend;
static struct lvn_context native;
static struct lvs_state state;
static struct lvh_ops hop;
static struct lvs_ops sop;
static bool attempted;
static uint64_t alloc(void*x){(void)x;return pmm_alloc_page();}
static bool release(void*x,uint64_t p){(void)x;return pmm_free_page(p)!=0;}
static void*map(void*x,uint64_t p){(void)x;return paging_phys_to_virt(p);}
static bool wb(void*x,uint64_t p){(void)x;return paging_page_rw_wb(p)&&vmx_memory_region_wb(p);}
static const struct sg_memory_ops memops={0,alloc,release,map,wb};
static _Alignas(16) unsigned char emergency_stack[4096];
__attribute__((noreturn)) static void unexpected_exit(void){panic("LINUX.UNEXPECTED_EXIT_TARGET ownership_retained");}
static bool read_field(uint64_t field,uint64_t*v){
 unsigned char bad;__asm__ volatile("vmread %2,%1;setna %0":"=qm"(bad),"=r"(*v):"r"(field):"cc","memory");return !bad;
}
static uint64_t ticks(void){uint32_t a,d;__asm__ volatile("lfence;rdtsc":"=a"(a),"=d"(d)::"memory");return ((uint64_t)d<<32)|a;}
static void number(const char*s,uint64_t n){log_text(s);log_hex(n);}
static bool snapshot(struct lvc_state*s){
 for(unsigned i=0;i<15;i++)s->gpr[i]=native.gpr[i];
 return read_field(0x681c,&s->rsp)&&read_field(0x681e,&s->rip)&&read_field(0x6800,&s->cr0)&&read_field(0x6802,&s->cr3)&&read_field(0x6804,&s->cr4)&&read_field(0x6004,&s->cr0_shadow)&&read_field(0x6006,&s->cr4_shadow);
}
static bool apply(const struct lvc_change*c){
 const struct lvc_state*n=&c->next;
#define PUT(f,v) do{if(!hop.vmwrite(hop.context,f,v))return false;}while(0)
 if(c->changed&LVC_CR0){PUT(0x6800,n->cr0);PUT(0x6004,n->cr0_shadow);}
 if(c->changed&LVC_CR3){PUT(0x6802,n->cr3);}
 if(c->changed&LVC_CR4){PUT(0x6804,n->cr4);PUT(0x6006,n->cr4_shadow);}
 if(c->changed&LVC_RSP){PUT(0x681c,n->rsp);}
 /* No VPID; invalidate EPT-derived translations before committing RIP. */
 if(!hop.invept(hop.context,memory.eptp))return false;
 PUT(0x681e,n->rip);
 for(unsigned i=0;i<15;i++)native.gpr[i]=n->gpr[i];
 return true;
#undef PUT
}
static bool guest_read64(void*unused,uint64_t gpa,uint64_t*out){
 (void)unused;if(!memory.prepared||!out||gpa>LGM_RAM_SIZE-8||(gpa&7))return false;
 unsigned page=(unsigned)(gpa>>12);uint64_t pa=memory.pages[page];
 if(!pa||(pa&4095))return false;
 const unsigned char*p=memory.owner.map(memory.owner.context,pa);
 if(!p)return false;
 uint64_t v=0;
 for(unsigned i=0;i<8;i++)v|=(uint64_t)p[(gpa&4095)+i]<<(8*i);
 *out=v;return true;
}
static void walk_log(uint64_t cr3,uint64_t va,bool dump){
 struct lpt_trace t;lpt_walk(cr3,va,LGM_RAM_SIZE,0,guest_read64,&t);
 number("LINUX.PT.va=",va);number("PT.structural_status=",t.status);
 for(unsigned i=0;i<t.count;i++){number("PT.level=",i);number("PT.entry_gpa=",t.address[i]);number("PT.entry=",t.entry[i]);}
 if(t.status==1){number("PT.gpa=",t.gpa);if(dump){uint64_t v;if(guest_read64(0,t.gpa,&v))number("PT.data64=",v);}}
}
static void high_pf_diag(uint64_t rip,uint64_t fault){
 uint64_t cr3,idtr,limit;
 const uint32_t fields[]={0x6800,0x6804,0x2806,0x4824,0x681c};
 const char*names[]={"PF.CR0=","PF.CR4=","PF.EFER=","PF.blocking=","PF.RSP="};
 for(unsigned i=0;i<5;i++){uint64_t v;if(read_field(fields[i],&v))number(names[i],v);else log_text("LINUX.PT.FIELD_UNAVAILABLE\n");}
 number("LINUX.PF.RSI=",native.gpr[4]);number("LINUX.PF.RDI=",native.gpr[5]);number("LINUX.PF.RCX=",native.gpr[2]);
 if(!read_field(0x6802,&cr3)||!read_field(0x6818,&idtr)||!read_field(0x4812,&limit)){log_text("LINUX.PT.READ_FAILED\n");return;}
 walk_log(cr3,fault,false);walk_log(cr3,rip,false);walk_log(cr3,native.gpr[5],false);
 uint64_t rsp;if(read_field(0x681c,&rsp))walk_log(cr3,rsp,false);
 if(limit>=14*16+15&&idtr<=UINT64_MAX-14*16-8){
  uint64_t low,high;struct lpt_trace a,b;
  walk_log(cr3,idtr+14*16,true);walk_log(cr3,idtr+14*16+8,true);
  lpt_walk(cr3,idtr+14*16,LGM_RAM_SIZE,0,guest_read64,&a);lpt_walk(cr3,idtr+14*16+8,LGM_RAM_SIZE,0,guest_read64,&b);
  if(a.status==1&&b.status==1&&guest_read64(0,a.gpa,&low)&&guest_read64(0,b.gpa,&high)){
   uint64_t handler=(low&0xffff)|((low>>32)&0xffff0000ULL)|((high&0xffffffffULL)<<32);
   number("IDT14.handler=",handler);number("IDT14.selector=",(low>>16)&0xffff);
   number("IDT14.IST=",(low>>32)&7);number("IDT14.attributes=",(low>>40)&255);
   number("IDT14.fixed_gate=",((low>>16)&0xffff)==0x10&&((low>>32)&255)==0&&((low>>40)&255)==0x8e&&(high>>32)==0);
   walk_log(cr3,handler,false);
   log_text("IDT14.TSS_STACK_NOT_VALIDATED\n");
  }else log_text("IDT14.UNAVAILABLE\n");
 }
}

static void permission_snapshot(uint64_t rip){
 uint64_t cr3,cr0,cr4,efer,rsp,gdtr,limit,cs,ar,ss,ssar;
 if(!read_field(0x6802,&cr3)||!read_field(0x6800,&cr0)||!read_field(0x6804,&cr4)||!read_field(0x2806,&efer)||!read_field(0x681c,&rsp)||!read_field(0x6816,&gdtr)||!read_field(0x4810,&limit)||!read_field(0x0802,&cs)||!read_field(0x4816,&ar)||!read_field(0x0804,&ss)||!read_field(0x4818,&ssar)){log_text("PERM.SNAPSHOT_UNAVAILABLE\n");return;}
 number("PERM.GDTR.base=",gdtr);number("PERM.GDTR.limit=",limit);number("PERM.CS=",cs);number("PERM.CS.ar=",ar);number("PERM.SS=",ss);number("PERM.SS.ar=",ssar);
 bool profile=(cs&3)==0;number("PERM.CPL0=",profile);
 number("PERM.fault_code_X=",profile&&lpp_range(cr3,rip,2,2,cr0,cr4,efer,LGM_RAM_SIZE,0,guest_read64));
 number("PERM.destination_W=",profile&&lpp_range(cr3,native.gpr[5],4096,1,cr0,cr4,efer,LGM_RAM_SIZE,0,guest_read64));
 number("PERM.stack_below_4K_W=",profile&&rsp>=4096&&lpp_range(cr3,rsp-4096,4096,1,cr0,cr4,efer,LGM_RAM_SIZE,0,guest_read64));
 const uint64_t code[]={0xffffffff816cf3deULL,0xffffffff816cf480ULL,0xffffffff816cf550ULL,0xffffffff816cf6c0ULL,0xffffffff81200c7eULL};
 const uint64_t sizes[]={7,58,368,65,29};
 for(unsigned i=0;i<5;i++){number("PERM.handler_part=",code[i]);number("PERM.handler_X=",profile&&lpp_range(cr3,code[i],sizes[i],2,cr0,cr4,efer,LGM_RAM_SIZE,0,guest_read64));}
 uint64_t off=cs&~7ULL;
 if(!(cs&4)&&off<=limit&&limit-off>=7&&gdtr<=UINT64_MAX-off&&lpp_range(cr3,gdtr+off,8,0,cr0,cr4,efer,LGM_RAM_SIZE,0,guest_read64)){
  struct lpt_trace tr;uint64_t desc;lpt_walk(cr3,gdtr+off,LGM_RAM_SIZE,0,guest_read64,&tr);
  if(tr.status==1&&guest_read64(0,tr.gpa,&desc)){
   number("PERM.CS.descriptor=",desc);
   number("PERM.CS.long_kernel_code=",cs==0x10&&(desc&(1ULL<<47))&&(desc&(1ULL<<44))&&(desc&(1ULL<<43))&&!(desc&(3ULL<<45))&&(desc&(1ULL<<53))&&!(desc&(1ULL<<54)));
  }else log_text("PERM.CS_DESCRIPTOR_UNAVAILABLE\n");
 }else log_text("PERM.CS_DESCRIPTOR_UNAVAILABLE\n");
 log_text("PERM.DIAGNOSTIC_ONLY_NO_INJECTION\n");
}

static bool virtual_u64(uint64_t cr3,uint64_t va,uint64_t*out){
 struct lpt_trace t;lpt_walk(cr3,va,LGM_RAM_SIZE,0,guest_read64,&t);
 return t.status==1&&guest_read64(0,t.gpa,out);
}
static bool high_snapshot(uint64_t reason,uint64_t rip,uint64_t fault,uint64_t event,uint64_t vector,struct lhp_snapshot*s){
 *s=(struct lhp_snapshot){0};
 s->v[H_REASON]=reason;s->v[H_RIP]=rip;s->v[H_ADDR]=fault;s->v[H_EVENT]=event;s->v[H_VECTOR]=vector;
 s->v[H_RSI]=native.gpr[4];s->v[H_RDI]=native.gpr[5];s->v[H_RCX]=native.gpr[2];
 const uint32_t fields[]={0x4404,0x4406,0x6802,0x4826,0x4824,0x6800,0x6804,0x2806,0x681c,0x0802,0x4816,0x0804,0x4818,0x6816,0x4810,0x6818,0x4812};
 const unsigned slots[]={H_INFO,H_ERROR,H_CR3,H_ACTIVITY,H_BLOCKING,H_CR0,H_CR4,H_EFER,H_RSP,H_CS,H_CSAR,H_SS,H_SSAR,H_GDTR,H_GDTLIMIT,H_IDTR,H_IDTLIMIT};
 for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++)if(!read_field(fields[i],&s->v[slots[i]]))return false;
 uint64_t cr3=s->v[H_CR3],cr0=s->v[H_CR0],cr4=s->v[H_CR4],efer=s->v[H_EFER];
 /* Fixed addresses are checked again against the complete exact tuple. */
 if(!virtual_u64(cr3,0xffffffff81611010ULL,&s->v[H_CSDESC])||!virtual_u64(cr3,0xffffffff8174a0e0ULL,&s->v[H_GATELO])||!virtual_u64(cr3,0xffffffff8174a0e8ULL,&s->v[H_GATEHI])||!guest_read64(0,0x16f5888,&s->v[H_ROOTENTRY]))return false;
 /* Faulting REP MOVSL crosses an aligned qword boundary at ...f50f. */
 uint64_t a,b;
 if(!virtual_u64(cr3,0xffffffff816cf508ULL,&a)||!virtual_u64(cr3,0xffffffff816cf510ULL,&b))return false;
 s->v[H_BYTES]=(a>>56)|((b&255)<<8);
 s->permissions=true;
 const uint64_t starts[]={0xffffffff8174a0e0ULL,0xffffffff81611010ULL,0xffffffff816cf50fULL,0xffffffff8174d760ULL,0xffffffff81602f38ULL,0xffffffff816cf3deULL,0xffffffff816cf480ULL,0xffffffff816cf550ULL,0xffffffff816cf6c0ULL,0xffffffff81200c7eULL,0xffffffff81736000ULL,0xffffffff816f5000ULL,0xffffffff816f6000ULL,0xffffffff81706000ULL,0xffffffff81716000ULL,0xffffffff81726000ULL,0xffffffff81634010ULL};
 const uint64_t sizes[]={16,8,2,4096,4096,7,58,368,65,29,8,4096,65536,65536,65536,65536,16};
 const unsigned access[]={0,0,2,1,1,2,2,2,2,2,1,1,1,1,1,1,0};
 for(unsigned i=0;i<sizeof(starts)/sizeof(starts[0]);i++)if(!lpp_range(cr3,starts[i],sizes[i],access[i],cr0,cr4,efer,LGM_RAM_SIZE,0,guest_read64)){s->permissions=false;number("HIGH_PF.PERMISSION_FAILED range=",i);number("start=",starts[i]);}
 /* Avoid allocator exhaustion/reset branch: normal path needs at most 2 tables. */
 uint64_t counter,base,flags;
 if(!virtual_u64(cr3,0xffffffff81736000ULL,&counter)||!virtual_u64(cr3,0xffffffff81634010ULL,&base)||!virtual_u64(cr3,0xffffffff81634018ULL,&flags))return false;
 number("HIGH_PF.counter_pair=",counter);number("HIGH_PF.physical_base=",base);number("HIGH_PF.pmd_flags=",flags);
 if((counter>>32)>62||base!=0||flags!=0xe3)s->permissions=false;
 for(unsigned i=0;i<H_COUNT;i++){number("HIGH_PF.snapshot_slot=",i);number("value=",s->v[i]);}
 number("HIGH_PF.permissions=",s->permissions);
 return true;
}

static void diagnostics(uint64_t reason){
 uint64_t v;
 if(reason==0){
  const uint32_t fields[]={0x4404,0x4406,0x6818,0x4812,0x6802};
  const char*names[]={"EXCEPTION.info=","error=","IDTR.base=","IDTR.limit=","CR3="};
  for(unsigned i=0;i<5;i++)if(read_field(fields[i],&v))number(names[i],v);
  number("savedCR2=",native.cr2);
 }
 if(reason==31||reason==32){
  number("MSR.ECX=",native.gpr[2]);number("MSR.lowEDX_EAX=",((native.gpr[3]&0xffffffff)<<32)|(native.gpr[0]&0xffffffff));
 }
}

static struct lvp_cpu_features cpu_view;
static void cpu_read(uint32_t leaf,uint32_t*a,uint32_t*b,uint32_t*c,uint32_t*d){
 __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}
static bool cpu_baseline(void){
 uint32_t max,b,c,d,ext,a,x,y,z;
 cpu_read(0,&max,&b,&c,&d);cpu_read(0x80000000,&ext,&x,&y,&z);
 if(max<1||ext<0x80000008)return false;
 cpu_read(1,&a,&x,&y,&cpu_view.basic_edx);
 cpu_read(0x80000001,&a,&x,&y,&cpu_view.extended_edx);
 cpu_read(0x80000008,&a,&x,&y,&z);cpu_view.physical_bits=a&255;
 number("LINUX.CPU_HOST basic_edx=",cpu_view.basic_edx);number("extended_edx=",cpu_view.extended_edx);number("physical_bits=",cpu_view.physical_bits);
 return lcpu_host_valid(max,b,c,d,ext,&cpu_view);
}

bool linux_recovery_pf_probe(void){
 if(attempted)return false;
 attempted=true;
 if(!cpu_baseline()){log_text("LINUX.BLOCKED CPU_BASELINE\n");return false;}
 log_text("LINUX.RECOVERED_PF_PROBE max4_unique_pf max64_entries max_tsc_2pow32 not_boot_success\n");
 uint64_t cr4,cr0,cr3,flags;
 __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
 __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
 __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
 __asm__ volatile("pushfq;pop %0":"=r"(flags));
 if((flags&0x600)||(cr4&(1u<<13))||paging_ap_start_attempted())return false;
 uint64_t initial_pages=pmm_get_stats().allocated_pages;
 bool changed=false,ok=false;
 if(!lvn_bind(&native,&sop)||!lvs_init(&state)||!lvb_bind(&backend,&memops,&hop)){
  log_text("LINUX.BLOCKED BIND\n");return false;
 }
 uint64_t feature,lo,hi,vmxcr4=cr4|(1u<<13);
 if(!lvn_read_msr(&native,0x3a,&feature)||(feature&5)!=5||
    !lvn_read_msr(&native,0x488,&lo)||!lvn_read_msr(&native,0x489,&hi)||
    (vmxcr4&lo)!=lo||(vmxcr4&~hi)){log_text("LINUX.BLOCKED VMXE_PREFLIGHT\n");return false;}
 if(!lgm_prepare(&memory,&memops,linux_kernel_start,(size_t)(linux_kernel_end-linux_kernel_start),
 linux_initrd_start,(size_t)(linux_initrd_end-linux_initrd_start),
 "console=ttyS0,115200 earlyprintk=serial,ttyS0,115200 nokaslr rdinit=/init")){
  log_text("LINUX.BLOCKED MEMORY\n");goto cleanup;
 }
 __asm__ volatile("mov %0,%%cr4"::"r"(vmxcr4):"memory");changed=true;
 if(!lvh_open(&host,&hop,&memory,(uintptr_t)(emergency_stack+sizeof emergency_stack),(uintptr_t)unexpected_exit)){
  log_text("LINUX.BLOCKED HOST_OPEN\n");goto cleanup;
 }
 if(!hop.invept(hop.context,memory.eptp)){log_text("LINUX.BLOCKED INITIAL_INVEPT\n");goto cleanup;}
 for(unsigned i=0;i<15;i++)native.gpr[i]=host.plan.gpr[i];
 native.resume=0;native.cr2=0;
 const struct lvc_limits limits={host.caps.cr0_fixed0,host.caps.cr0_fixed1,host.caps.cr4_fixed0,host.caps.cr4_fixed1,LGM_RAM_SIZE};
 struct lpfg_state pf={0};
 enum lhp_state high=LHP_UNUSED;
 uint64_t began=ticks();
 for(unsigned index=0;index<64;index++){
  if(ticks()-began>=UINT64_C(0x100000000)){log_text("LINUX.STOP TSC_BUDGET\n");break;}
  if(!hop.vmwrite(hop.context,0x482e,UINT64_C(1)<<(28-host.caps.timer_shift))){log_text("LINUX.STOP TIMER_WRITE\n");break;}
  enum lvs_result result=lvs_run(&state,&sop);
  if(result==LVS_HOST_DIRTY||state.host_dirty)panic("LINUX.HOST_STATE_DIRTY ownership_retained");
  if(result!=LVS_EXIT){number("LINUX.STOP STATE_RESULT=",result);break;}
  uint64_t reason,rip,qualification,vectoring,length,cs,event;
  if(!read_field(0x4402,&reason)||!read_field(0x681e,&rip)||!read_field(0x6400,&qualification)||!read_field(0x4408,&vectoring)||!read_field(0x440c,&length)||!read_field(0x0802,&cs)||!read_field(0x4016,&event)){log_text("LINUX.STOP EXIT_READ\n");break;}
  number("LINUX.CR_EXIT index=",index);number("reason=",reason);number("rip=",rip);number("qualification=",qualification);
  /* v4 diagnostic-only observations: never used to alter the v3 policy. */
  number("LINUX.PF_DIAG event.raw=",event);
  number("LINUX.PF_DIAG vectoring.raw=",vectoring);
  uint64_t diagnostic_activity=0;
  if(read_field(0x4826,&diagnostic_activity))number("LINUX.PF_DIAG activity.raw=",diagnostic_activity);
  else log_text("LINUX.PF_DIAG activity.read_failed\n");
  number("LINUX.PF_DIAG guard.count=",pf.count);
  number("LINUX.PF_DIAG guard.seen=",pf.seen);
  number("LINUX.PF_DIAG guard.pending=",pf.pending);
  number("LINUX.PF_DIAG guard.closed=",pf.closed);
  diagnostics(reason);
  if(reason==0){high_pf_diag(rip,qualification);permission_snapshot(rip);}
  if(reason&(1ull<<31)){log_text("LINUX.STOP ENTRY_FAILURE\n");break;}
  if(cs&3){log_text("LINUX.STOP CPL\n");break;}
  if(vectoring&(1ull<<31)){number("LINUX.STOP VECTORING=",vectoring);break;}
  if(high==LHP_PENDING){
   enum lhp_state consumed;
   if(pf.pending||!lhp_consume(high,reason,vectoring,event,&consumed)||!hop.vmwrite(hop.context,0x4016,0)){
    log_text("LINUX.STOP HIGH_PF_CONSUME no_resume\n");break;
   }
   high=consumed;event=0;log_text("LINUX.HIGH_PF_EVENT_CONSUMED not_handler_completion\n");
  }
  struct lpfg_state observed;
  if(!lpfg_observe(&pf,reason,rip,vectoring,event,&observed)){log_text("LINUX.STOP EVENT_GUARD\n");break;}
  if(pf.pending&&!hop.vmwrite(hop.context,0x4016,0)){log_text("LINUX.STOP CONSUMED_EVENT_CLEAR\n");break;}
  pf=observed;event=0;
  if(ticks()-began>=UINT64_C(0x100000000)){log_text("LINUX.STOP TSC_BUDGET\n");break;}
  if(reason==0&&pf.closed){
   struct lhp_snapshot snap;
   if(index==63||!high_snapshot(reason,rip,qualification,event,vectoring,&snap)||!lhp_accept(high,&pf,&snap)){
    log_text("LINUX.STOP HIGH_PF_EXACT_GUARD\n");break;
   }
   if(!lhp_commit(&high,&native.cr2,&native.resume,qualification,hop.context,hop.vmwrite)){
    log_text("LINUX.STOP HIGH_PF_COMMIT no_resume\n");break;
   }
   log_text("LINUX.HIGH_PF_INJECTION_PREPARED one_only\n");continue;
  }
  if(reason==0){
   uint64_t info,error,blocking,idtr,idtlimit,guestcr3,activity;
   if(index==63||!read_field(0x4404,&info)||!read_field(0x4406,&error)||!read_field(0x4824,&blocking)||
    !read_field(0x6818,&idtr)||!read_field(0x4812,&idtlimit)||!read_field(0x6802,&guestcr3)||!read_field(0x4826,&activity)){
      log_text("LINUX.STOP PF_READ_OR_ENTRY_BUDGET\n");break;
   }
   struct lve_pf_exit input={reason,qualification,vectoring,rip,info,error,event,blocking,idtr,idtlimit};
   struct lve_pf_entry injection;struct lpfg_state next_pf;
   if(!lpfg_inject(&pf,&input,guestcr3,activity,&next_pf,&injection)){log_text("LINUX.STOP PINNED_PF_GUARD\n");break;}
   /* No RIP advance. Write interruption-info last; partial failure stops. */
   if(!hop.vmwrite(hop.context,0x4018,injection.error)||
      !hop.vmwrite(hop.context,0x401a,injection.instruction_length)||
      !hop.vmwrite(hop.context,0x4016,injection.info)){
        log_text("LINUX.STOP PF_COMMIT no_resume\n");break;
   }
   native.cr2=injection.cr2;
   pf=next_pf;native.resume=1;
   number("LINUX.PF_INJECTION_PREPARED=",pf.count);
   continue;
  }
  if(reason==10){
   number("LINUX.CPUID leaf=",(uint32_t)native.gpr[0]);number("subleaf=",(uint32_t)native.gpr[2]);
   if(index==63||length!=2||!lcpu_apply(native.gpr,rip,(unsigned)(cs&3),reason,(unsigned)length,&cpu_view,hop.context,hop.vmwrite)){
    log_text("LINUX.STOP CPUID_POLICY_OR_COMMIT no_resume\n");break;
   }
   number("CPUID.EAX=",native.gpr[0]);number("CPUID.EBX=",native.gpr[1]);number("CPUID.ECX=",native.gpr[2]);number("CPUID.EDX=",native.gpr[3]);
   native.resume=1;log_text("LINUX.CPUID_RESUME_PREPARED\n");continue;
  }
  if((reason==31||reason==32)&&(uint32_t)native.gpr[2]==0xc0000080){
   struct lefer_env env={.eptp=memory.eptp};struct lvp_cpuid_result view;
   if(index==63||length!=2||!lvp_cpuid(&cpu_view,0x80000001,0,&view)||
      !read_field(0x2806,&env.efer)||!read_field(0x6800,&env.cr0)||
      !read_field(0x6804,&env.cr4)||!read_field(0x4012,&env.entry)){
    log_text("LINUX.STOP EFER_READ_OR_BUDGET\n");break;
   }
   env.ext_edx=view.d;number("LINUX.EFER.current=",env.efer);
   if(!lefer_apply(native.gpr,rip,(unsigned)(cs&3),reason,(unsigned)length,&env,hop.context,hop.vmwrite,hop.invept)){
    log_text("LINUX.STOP EFER_POLICY_OR_COMMIT no_resume\n");break;
   }
   native.resume=1;log_text("LINUX.EFER_RESUME_PREPARED\n");continue;
  }
  if(reason==31||reason==32){
   uint64_t gs_base;
   if(index==63||length!=2||!read_field(0x6810,&gs_base)){
    log_text("LINUX.STOP GS_READ_OR_BUDGET\n");break;
   }
   number("LINUX.GS.current=",gs_base);
   if(!lgsc_apply(native.gpr,rip,gs_base,(unsigned)(cs&3),reason,(unsigned)length,hop.context,hop.vmwrite)){
    log_text("LINUX.STOP GS_POLICY_OR_COMMIT no_resume\n");break;
   }
   native.resume=1;log_text("LINUX.GS_RESUME_PREPARED\n");continue;
  }
  if(reason!=28){log_text("LINUX.STOP UNHANDLED_EXIT\n");break;}
  if(index==63){log_text("LINUX.STOP ENTRY_BUDGET\n");break;}
  struct lvc_state current;struct lvc_change change;
  if(!snapshot(&current)){log_text("LINUX.STOP CR_SNAPSHOT\n");break;}
  number("CR0.actual=",current.cr0);number("CR0.shadow=",current.cr0_shadow);
  number("CR4.actual=",current.cr4);number("CR4.shadow=",current.cr4_shadow);
  for(unsigned i=0;i<15;i++){number("gpr.index=",i);number("value=",current.gpr[i]);}
  number("rsp=",current.rsp);
  if(length>15||!lvc_emulate(&current,&limits,qualification,(unsigned)length,&change)){log_text("LINUX.STOP CR_POLICY\n");break;}
  number("next.CR0=",change.next.cr0);number("next.CR3=",change.next.cr3);number("next.CR4=",change.next.cr4);number("next.RIP=",change.next.rip);
  if(!apply(&change)){log_text("LINUX.STOP CR_COMMIT no_resume\n");break;}
  native.resume=1;
  number("LINUX.CR_RESUME_PREPARED=",index+1);
 }

cleanup:
 if(!lvh_close(&host))panic("LINUX.CLOSE_FAILED ownership_retained");
 if(changed)__asm__ volatile("mov %0,%%cr4"::"r"(cr4):"memory");
 uint64_t a,b,c;
 __asm__ volatile("mov %%cr0,%0":"=r"(a));__asm__ volatile("mov %%cr3,%0":"=r"(b));
 __asm__ volatile("mov %%cr4,%0":"=r"(c));
 if(a!=cr0||b!=cr3||c!=cr4)panic("LINUX.HOST_CR_RESTORE_FAILED ownership_retained");
 if(!lgm_release(&memory))panic("LINUX.MEMORY_RELEASE_FAILED");
 if(pmm_get_stats().allocated_pages!=initial_pages)panic("LINUX.PMM_ACCOUNTING_FAILED");
 log_text(ok?"LINUX.FIRST_EXIT_OBSERVED not_boot_success\n":"LINUX.PF_PROBE_NOT_COMPLETED\n");
 return ok;
}
