/* Reconstructed opt-in first-exit only. Trusted fixed inputs, not Linux boot. */
#include "linux_guest_memory.h"
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
bool linux_recovery_first_exit(void){
 if(attempted)return false;
 attempted=true;
 log_text("LINUX.RECOVERED_FIRST_EXIT max1_entry not_boot_success\n");
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
 enum lvs_result result=lvs_run(&state,&sop); /* Exactly one invocation. */
 if(result==LVS_HOST_DIRTY||state.host_dirty)panic("LINUX.HOST_STATE_DIRTY ownership_retained");
 if(result!=LVS_EXIT){log_text("LINUX.BLOCKED STATE_RESULT=");log_hex(result);goto cleanup;}
 uint64_t reason,rip,qualification;
 if(!read_field(0x4402,&reason)||!read_field(0x681e,&rip)||!read_field(0x6400,&qualification)){
  log_text("LINUX.BLOCKED EXIT_READ\n");goto cleanup;
 }
 log_text("LINUX.FIRST_EXIT reason=");log_hex(reason);
 log_text("rip=");log_hex(rip);log_text("qualification=");log_hex(qualification);
 if(reason&(1ull<<31)){log_text("LINUX.VMENTRY_FAILURE\n");goto cleanup;}
 ok=true;
cleanup:
 if(!lvh_close(&host))panic("LINUX.CLOSE_FAILED ownership_retained");
 if(changed)__asm__ volatile("mov %0,%%cr4"::"r"(cr4):"memory");
 uint64_t a,b,c;
 __asm__ volatile("mov %%cr0,%0":"=r"(a));__asm__ volatile("mov %%cr3,%0":"=r"(b));
 __asm__ volatile("mov %%cr4,%0":"=r"(c));
 if(a!=cr0||b!=cr3||c!=cr4)panic("LINUX.HOST_CR_RESTORE_FAILED ownership_retained");
 if(!lgm_release(&memory))panic("LINUX.MEMORY_RELEASE_FAILED");
 if(pmm_get_stats().allocated_pages!=initial_pages)panic("LINUX.PMM_ACCOUNTING_FAILED");
 log_text(ok?"LINUX.FIRST_EXIT_OBSERVED not_boot_success\n":"LINUX.FIRST_EXIT_NOT_COMPLETED\n");
 return ok;
}
