#include "linux_guest_manager.h"
#include "ring3.h"
#include "memory.h"
#include "paging.h"
#include "managed_host_context.h"
#include "linux_pf_diagnostic.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
extern const unsigned char linux_kernel_start[],linux_kernel_end[],linux_initrd_start[],linux_initrd_end[];
static struct lgs_session session;
static struct lvb_context backend;
static struct {
 uint64_t cr0,cr3,cr4;
 bool saved,changed,installed;
 struct lgm_service_ops service;
} platform;
static _Alignas(16) unsigned char fallback_stack[4096];
static __attribute__((noreturn)) void fatal(void *unused) { (void)unused;panic("MANAGED GUEST HOST STATE UNSAFE; OWNERSHIP RETAINED"); }
static __attribute__((noreturn)) void fallback(void) { fatal(0); }
static uint64_t allocate(void*x){(void)x;return pmm_alloc_page();}
static bool free_page(void*x,uint64_t p){(void)x;return pmm_free_page(p)!=0;}
static void*map(void*x,uint64_t p){(void)x;return paging_phys_to_virt(p);}
static bool wb(void*x,uint64_t p){(void)x;return paging_page_rw_wb(p)&&vmx_memory_region_wb(p);}
static const struct sg_memory_ops memory_ops={0,allocate,free_page,map,wb};
static bool enable(void*x) {
 (void)x;uint64_t flags,feature,lo,hi,cr4;
 __asm__ volatile("pushfq;pop %0":"=r"(flags));
 __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
 if((flags&0x600)||(cr4&(1u<<13))||paging_ap_start_attempted()||platform.changed)return false;
 uint64_t next=cr4|(1u<<13);
 if(!lvn_read_msr(&session.native,0x3a,&feature)||(feature&5)!=5||
    !lvn_read_msr(&session.native,0x488,&lo)||!lvn_read_msr(&session.native,0x489,&hi)||
    (next&lo)!=lo||(next&~hi))return false;
 __asm__ volatile("mov %%cr0,%0":"=r"(platform.cr0));
 __asm__ volatile("mov %%cr3,%0":"=r"(platform.cr3));
 platform.cr4=cr4;platform.saved=true;
 __asm__ volatile("mov %0,%%cr4"::"r"(next):"memory");platform.changed=true;return true;
}
static bool disable(void*x) {
 (void)x;if(session.host.active)return false;
 if(platform.changed) {__asm__ volatile("mov %0,%%cr4"::"r"(platform.cr4):"memory");platform.changed=false;}
 return true;
}
static bool safe(void*x) {
 (void)x;if(!platform.saved)return true;
 uint64_t a,b,c;__asm__ volatile("mov %%cr0,%0":"=r"(a));
 __asm__ volatile("mov %%cr3,%0":"=r"(b));__asm__ volatile("mov %%cr4,%0":"=r"(c));
 return a==platform.cr0&&b==platform.cr3&&c==(platform.cr4|(platform.changed?(1u<<13):0));
}
static bool read_field(void*x,uint32_t f,uint64_t*v) {
 (void)x;unsigned char bad;__asm__ volatile("vmread %2,%1;setna %0":"=qm"(bad),"=r"(*v):"r"((uint64_t)f):"cc","memory");return !bad;
}
static bool dispatch(void*x,struct lgs_session*s,uint32_t reason) {
 (void)x;return platform.service.dispatch&&platform.service.dispatch(platform.service.ctx,s,reason);
}
static bool prepare_guest(void*x,struct lgs_session*s){
 (void)x;return platform.service.prepare_guest(platform.service.ctx,s);
}
#ifdef MG_PROGRESS_DIAG
extern void linux_guest_progress_note(struct lgs_session*);
#endif
static bool before_entry(void*x,struct lgs_session*s) {
 (void)x;bool ok=platform.service.before_entry(platform.service.ctx,s);
 #ifdef MG_PROGRESS_DIAG
 if(ok)linux_guest_progress_note(s);
 #endif
 return ok;
}
static bool after_entry(void*x,struct lgs_session*s,const struct lgs_entry_result*r) {
 (void)x;bool ok=platform.service.after_entry(platform.service.ctx,s,r);
 #ifdef MG_PROGRESS_DIAG
 if(ok)linux_guest_progress_note(s);
 #endif
 return ok;
}
/* int80 uses the Ring3-owned root and a null SS. Temporarily enter the
 * already established supervisor root; never weaken paging WB proof. */
static uint64_t context_root(void*x){(void)x;uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static void context_set_root(void*x,uint64_t v){(void)x;__asm__ volatile("mov %0,%%cr3"::"r"(v):"memory");}
static struct mhc_selectors context_selectors(void*x){
 (void)x;struct mhc_selectors s;
 __asm__ volatile("mov %%ds,%0;mov %%es,%1;mov %%ss,%2":"=r"(s.ds),"=r"(s.es),"=r"(s.ss));return s;
}
static void context_set_selectors(void*x,struct mhc_selectors s){
 (void)x;__asm__ volatile("mov %0,%%ds;mov %1,%%es;mov %2,%%ss"::"r"(s.ds),"r"(s.es),"r"(s.ss):"memory");
}
static const struct mhc_ops context_ops={0,context_root,context_set_root,context_selectors,context_set_selectors};
static struct mhc_saved enter_context(void){
 struct mhc_saved s={0};if(!mhc_enter(&s,&context_ops,paging_root()))fatal(0);return s;
}
static void leave_context(struct mhc_saved*s){if(!mhc_leave(s,&context_ops))fatal(0);}
static int prepare(void*c,uint64_t p){
 struct lgs_session *g=c;
 if(p!=SYLE_GUEST_BOOT_PROFILE||g->prepared||g->enabled||g->failed||!platform.service.reset(platform.service.ctx)){
  log_text("MANAGED.CREATE rejected before session prepare\n");return 1;
 }
 uint16_t incoming_ss;__asm__ volatile("mov %%ss,%0":"=r"(incoming_ss));
 struct mhc_saved s=enter_context();int r=lgs_guest_backend.prepare(c,p);
 if(r){
  log_text("MANAGED.CREATE failed prepare_stage=");log_hex(g->prepare_stage);
  log_text(" open_stage=");log_hex(g->host.open_stage);
  log_text(" incoming_ss=");log_hex(incoming_ss);
  log_text("\n");
 }
 leave_context(&s);return r;
}
static void diagnostic_field(const char *name,uint32_t field){
 uint64_t v;log_text(name);if(read_field(0,field,&v))log_hex(v);else log_text("unavailable");
}
static void pf_diagnostic(struct lgs_session*g){
 struct lgpd_snapshot d;if(!lgpd_collect(g,&d))return;
 log_text("MANAGED.PF valid=");log_hex(d.valid);log_text(" r12=");log_hex(d.r12);log_text("\n");
 for(unsigned i=0;i<LGPD_FIELDS;i++){log_text("PF.field ");log_hex(i);log_text("=");log_hex(d.value[i]);log_text("\n");}
 for(unsigned i=0;i<LGPD_TRACES;i++){
  log_text("PF.trace ");log_hex(i);log_text(" va=");log_hex(d.va[i]);log_text(" status=");log_hex(d.trace[i].status);log_text(" gpa=");log_hex(d.trace[i].gpa);log_text("\n");
  for(unsigned j=0;j<d.trace[i].count&&j<4;j++){log_text("PF.pte address=");log_hex(d.trace[i].address[j]);log_text(" value=");log_hex(d.trace[i].entry[j]);log_text("\n");}
 }
 log_text("PF.data_valid=");log_hex(d.data_valid);log_text(" gate=");log_hex(d.gate[0]);log_hex(d.gate[1]);log_text(" csdesc=");log_hex(d.csdesc);log_text("\n");
 log_text("PF.instruction=");log_hex(d.bytes[0]);log_hex(d.bytes[1]);log_text(" handler=");log_hex(d.handler_bytes[0]);log_hex(d.handler_bytes[1]);log_text("\n");
}
#ifdef MG_PROGRESS_DIAG
extern void linux_guest_progress_probe(struct lgs_session*,bool,bool);
#endif
static int start(void*c){
 struct mhc_saved saved=enter_context();struct lgs_session*g=c;
 #ifdef MG_PROGRESS_DIAG
 linux_guest_progress_probe(g,false,false);
 #endif
 int r=lgs_guest_backend.start(c);
 #ifdef MG_PROGRESS_DIAG
 linux_guest_progress_probe(g,true,r==0);
 #endif
 if(r){
  log_text("MANAGED.SLICE failed stage=");log_hex(g->step_stage);
  log_text(" state_result=");log_hex(g->step_result);
  log_text(" accepted_exits=");log_hex(g->exits);
  if(g->step_stage>=2&&g->host.active&&g->host.ready){
   /* Read-only raw snapshot after safe host restoration. Emulation may have
    * partially updated VMCS/GPRs; these are diagnostics, not validated exits. */
   diagnostic_field(" raw_reason=",0x4402);diagnostic_field(" raw_rip=",0x681e);
   diagnostic_field(" raw_qual=",0x6400);diagnostic_field(" raw_event=",0x4404);
   diagnostic_field(" raw_vectoring=",0x4408);diagnostic_field(" raw_instruction_error=",0x4400);
   diagnostic_field(" raw_exit_length=",0x440c);
   log_text(" raw_rax=");log_hex(g->native.gpr[0]);
   log_text(" raw_rdx=");log_hex(g->native.gpr[3]);
   log_text(" raw_rcx=");log_hex(g->native.gpr[2]);log_text("\n");
   pf_diagnostic(g);
  }
  log_text("\n");
 }
 leave_context(&saved);return r;
}
static int stop(void*c){struct mhc_saved s=enter_context();int r=lgs_guest_backend.stop(c);leave_context(&s);return r;}
static int release(void*c){struct mhc_saved s=enter_context();int r=lgs_guest_backend.release(c);leave_context(&s);return r;}
static int send(void*c,uint8_t b){(void)c;return platform.service.send?platform.service.send(platform.service.ctx,b):-(int)SYLE_GUEST_ENOTSUP;}
static int recv(void*c){(void)c;return platform.service.recv?platform.service.recv(platform.service.ctx):-(int)SYLE_GUEST_ENOTSUP;}
static const struct guest_backend guest_ops={.prepare=prepare,.start=start,.stop=stop,.release=release,.console_send=send,.console_recv=recv};
bool linux_guest_manager_install(const struct lgm_service_ops *service) {
 if(platform.installed||!service||!service->reset||!service->dispatch||
    (!service->before_entry!=!service->after_entry)||!lvn_bind(&session.native,&session.sop)||
    !lvb_bind(&backend,&memory_ops,&session.hop))return false;
 platform.service=*service;
 struct lgs_platform ops={.enable=enable,.disable=disable,.safe=safe,
  .read_field=read_field,.dispatch=dispatch,.fatal=fatal,
  .before_entry=service->before_entry?before_entry:0,
  .after_entry=service->after_entry?after_entry:0,
  .prepare_guest=service->prepare_guest?prepare_guest:0};
 if(!lgs_configure(&session,&session.hop,&session.sop,&ops,
 linux_kernel_start,(size_t)(linux_kernel_end-linux_kernel_start),
 linux_initrd_start,(size_t)(linux_initrd_end-linux_initrd_start),
 "console=ttyS0,115200 earlyprintk=serial,ttyS0,115200 nokaslr rdinit=/init",
 (uintptr_t)(fallback_stack+sizeof fallback_stack),(uintptr_t)fallback))return false;
 if(!ring3_guest_bind(&guest_ops,&session))return false;
 platform.installed=true;return true;
}
