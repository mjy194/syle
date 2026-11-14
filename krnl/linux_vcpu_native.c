#include "linux_vcpu_native.h"
#include <stddef.h>
_Static_assert(offsetof(struct lvn_context,cr2)==120,"cr2 offset");
_Static_assert(offsetof(struct lvn_context,host_cr2)==128,"host cr2 offset");
_Static_assert(offsetof(struct lvn_context,resume)==136,"resume offset");
_Static_assert(offsetof(struct lvn_context,entry_status)==140,"status offset");
_Static_assert(offsetof(struct lvn_context,host_dr6)==152,"dr6 offset");
_Static_assert(offsetof(struct lvn_context,host_gdtr)==160,"gdtr offset");
_Static_assert(offsetof(struct lvn_context,host_idtr)==170,"idtr offset");
volatile uint64_t lvn_armed_rip;
static uint32_t mxcsr_mask=0xffbf;
extern const uint8_t lvn_exception_hook_present __attribute__((weak));
extern char lvn_rd_fault[],lvn_wr_fault[],lvn_fxsave_fault[],lvn_fxload_fault[],lvn_false[];
extern char lvn_rsp_fault[],lvn_rip_fault[],lvn_early_fail[];
extern char lvn_launch_fault[],lvn_resume_fault[],lvn_entry_fail[];
extern bool lvn_read_raw(void *,uint32_t,uint64_t *),lvn_write_raw(void *,uint32_t,uint64_t);
extern bool lvn_save_raw(void *,uint8_t *),lvn_load_raw(void *,const uint8_t *),lvn_enter_raw(void *);
bool lvn_fault_fixup(unsigned vector,uint64_t *rip){
 if(!rip||(vector!=6&&vector!=13)||!lvn_armed_rip||*rip!=lvn_armed_rip)return false;
 uint64_t next=0;
 if(*rip==(uintptr_t)lvn_rd_fault||*rip==(uintptr_t)lvn_wr_fault||
    *rip==(uintptr_t)lvn_fxsave_fault||*rip==(uintptr_t)lvn_fxload_fault)next=(uintptr_t)lvn_false;
 else if(*rip==(uintptr_t)lvn_rsp_fault||*rip==(uintptr_t)lvn_rip_fault)next=(uintptr_t)lvn_early_fail;
 else if(*rip==(uintptr_t)lvn_launch_fault||*rip==(uintptr_t)lvn_resume_fault)next=(uintptr_t)lvn_entry_fail;
 if(!next)return false;
 lvn_armed_rip=0;*rip=next;return true;
}
static bool allowed(void){
 uint16_t cs;uint64_t flags;
 __asm__ volatile("mov %%cs,%0":"=r"(cs));if(cs&3)return false;
 __asm__ volatile("pushfq;pop %0":"=r"(flags));
 return !(flags&512)&&!lvn_armed_rip&& &lvn_exception_hook_present&&lvn_exception_hook_present==1;
}
static bool listed(uint32_t n){
 for(unsigned i=0;i<LVS_MSR_COUNT;i++)if(n==lvs_msr_ids[i])return true;
 return false;
}
static bool readable(uint32_t n){
 if(listed(n))return true;
 switch(n){
 case 0x3a:case 0x480:case 0x481:case 0x482:case 0x483:case 0x484:
 case 0x485:case 0x486:case 0x487:case 0x488:case 0x489:case 0x48b:
 case 0x48c:case 0x48d:case 0x48e:case 0x48f:case 0x490:
 case 0x1d9:case 0x277:case 0x174:case 0x175:case 0x176:
 case 0xc0000080:case 0xc0000100:case 0xc0000101:return true;
 default:return false;
 }
}
bool lvn_read_msr(void *ctx,uint32_t n,uint64_t *v){
 return v&&readable(n)&&allowed()&&lvn_read_raw(ctx,n,v);
}
#ifdef MG_HOST_CLOCK
/* Exact read-only Hyper-V reference counter. Caller verifies Hv#1 and feature
 * advertisement; reuse the armed #GP/#UD fixup, never a naked RDMSR. */
bool lvn_read_clock_reference(uint64_t*v){
 return v&&allowed()&&lvn_read_raw(0,0x40000020,v);
}
#endif
bool lvn_write_msr(void *ctx,uint32_t n,uint64_t v){
 struct lvn_context*c=ctx;
 return c&&c->enabled&&listed(n)&&allowed()&&lvn_write_raw(ctx,n,v);
}
static bool save(void*ctx,uint8_t*v){
 struct lvn_context*c=ctx;
 return c&&c->enabled&&v&&!((uintptr_t)v&15)&&allowed()&&lvn_save_raw(ctx,v);
}
static bool load(void*ctx,const uint8_t*v){
 struct lvn_context*c=ctx;
 if(!c||!c->enabled||!v||((uintptr_t)v&15)||!allowed())return false;
 uint32_t mx=(uint32_t)v[24]|((uint32_t)v[25]<<8)|((uint32_t)v[26]<<16)|((uint32_t)v[27]<<24);
 return !(mx&~mxcsr_mask)&&lvn_load_raw(ctx,v);
}
bool lvn_enter(void*ctx){
 struct lvn_context*c=ctx;
 return c&&c->enabled&&c->resume<=1&&allowed()&&lvn_enter_raw(ctx);
}
static void cpuid(uint32_t leaf,uint32_t*a,uint32_t*b,uint32_t*c,uint32_t*d){
 __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}
bool lvn_bind(struct lvn_context*c,struct lvs_ops*o){
 if(!c||!o||c->enabled||!allowed())return false;
 uint32_t a,b,x,d;cpuid(0,&a,&b,&x,&d);
 if(a<1||b!=0x756e6547||d!=0x49656e69||x!=0x6c65746e)return false;
 cpuid(1,&a,&b,&x,&d);
 if(!(x&(1u<<5))||(d&((1u<<24)|(1u<<25)|(1u<<26)))!=((1u<<24)|(1u<<25)|(1u<<26)))return false;
 cpuid(0x80000000,&a,&b,&x,&d);if(a<0x80000001)return false;
 cpuid(0x80000001,&a,&b,&x,&d);
 if((d&((1u<<11)|(1u<<27)|(1u<<29)))!=((1u<<11)|(1u<<27)|(1u<<29)))return false;
 uint64_t cr0,cr4,dr7;
 __asm__ volatile("mov %%cr0,%0":"=r"(cr0));__asm__ volatile("mov %%cr4,%0":"=r"(cr4));
 __asm__ volatile("mov %%dr7,%0":"=r"(dr7));
 if(dr7!=0x400||(cr0&12)||!(cr4&(1u<<9))||
    (cr4&((1u<<12)|(1u<<18)|(1u<<22)|(1u<<23))))return false;
 uint64_t value;if(!lvn_read_msr(c,0x1d9,&value)||value)return false;
 for(unsigned i=0;i<LVS_MSR_COUNT;i++)if(!lvn_read_msr(c,lvs_msr_ids[i],&value))return false;
 _Alignas(16) uint8_t fx[512]={0};if(!lvn_save_raw(c,fx))return false;
 uint32_t mask=(uint32_t)fx[28]|((uint32_t)fx[29]<<8)|((uint32_t)fx[30]<<16)|((uint32_t)fx[31]<<24);
 mxcsr_mask=mask?mask:0xffbf;
 c->enabled=true;*o=(struct lvs_ops){c,save,load,lvn_read_msr,lvn_write_msr,lvn_enter};return true;
}
