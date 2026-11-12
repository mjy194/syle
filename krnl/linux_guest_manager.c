#include "linux_guest_manager.h"
#include "ring3.h"
#include "memory.h"
#include "paging.h"
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
static bool before_entry(void*x,struct lgs_session*s) {
 (void)x;return platform.service.before_entry(platform.service.ctx,s);
}
static bool after_entry(void*x,struct lgs_session*s,const struct lgs_entry_result*r) {
 (void)x;return platform.service.after_entry(platform.service.ctx,s,r);
}
/* int80 retains user DS/ES. Set valid kernel selectors for VMCS host capture,
 * then restore the exact caller values before returning through IRET. */
struct segments { uint16_t ds,es; };
static struct segments enter_segments(void) {
 struct segments s;__asm__ volatile("mov %%ds,%0;mov %%es,%1":"=r"(s.ds),"=r"(s.es));
 uint16_t k=0x10;__asm__ volatile("mov %0,%%ds;mov %0,%%es"::"r"(k):"memory");return s;
}
static void leave_segments(struct segments s) {__asm__ volatile("mov %0,%%ds;mov %1,%%es"::"r"(s.ds),"r"(s.es):"memory");}
static int prepare(void*c,uint64_t p){
 struct lgs_session *g=c;
 if(p!=SYLE_GUEST_BOOT_PROFILE||g->prepared||g->enabled||g->failed||!platform.service.reset(platform.service.ctx))return 1;
 struct segments s=enter_segments();int r=lgs_guest_backend.prepare(c,p);leave_segments(s);return r;
}
static int start(void*c){struct segments s=enter_segments();int r=lgs_guest_backend.start(c);leave_segments(s);return r;}
static int stop(void*c){struct segments s=enter_segments();int r=lgs_guest_backend.stop(c);leave_segments(s);return r;}
static int release(void*c){struct segments s=enter_segments();int r=lgs_guest_backend.release(c);leave_segments(s);return r;}
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
  .after_entry=service->after_entry?after_entry:0};
 if(!lgs_configure(&session,&session.hop,&session.sop,&ops,
 linux_kernel_start,(size_t)(linux_kernel_end-linux_kernel_start),
 linux_initrd_start,(size_t)(linux_initrd_end-linux_initrd_start),
 "console=ttyS0,115200 earlyprintk=serial,ttyS0,115200 nokaslr rdinit=/init",
 (uintptr_t)(fallback_stack+sizeof fallback_stack),(uintptr_t)fallback))return false;
 if(!ring3_guest_bind(&guest_ops,&session))return false;
 platform.installed=true;return true;
}
