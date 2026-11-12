#include "smp_vmx.h"
#include "vmx_memory.h"
#define VMXE (UINT64_C(1)<<13)
#define NE (UINT64_C(1)<<5)
enum smp_vmx_status smp_vmx_policy(const struct vmx_caps *c,
    uint64_t hpa, unsigned bits, uint64_t *newcr0)
{
    if (!c || !newcr0) return SMP_VMX_BAD_CONTEXT;
    if ((c->feature_control&5)!=5) return SMP_VMX_BLOCKED_FEATURE_CONTROL;
    unsigned size=(unsigned)((c->basic>>32)&0x1fff);
    if (!size || size>4096 || (c->basic&(UINT64_C(1)<<31)) ||
        ((c->basic>>50)&15)!=6) return SMP_VMX_BLOCKED_BASIC;
    if (bits<32 || bits>52 || !hpa || (hpa&4095) ||
        hpa>=(UINT64_C(1)<<bits) ||
        ((c->basic&(UINT64_C(1)<<48)) && hpa>=(UINT64_C(1)<<32)))
        return SMP_VMX_BLOCKED_ADDRESS;
    uint64_t cr0=c->cr0 | (c->cr0_fixed0&NE);
    if ((cr0&c->cr0_fixed0)!=c->cr0_fixed0 || (cr0&~c->cr0_fixed1))
        return SMP_VMX_BLOCKED_CR0;
    uint64_t cr4=c->cr4|VMXE;
    if ((c->cr4&VMXE) || (cr4&c->cr4_fixed0)!=c->cr4_fixed0 ||
        (cr4&~c->cr4_fixed1)) return SMP_VMX_BLOCKED_CR4;
    *newcr0=cr0;
    return SMP_VMX_OK;
}
#ifndef SMP_VMX_POLICY_ONLY
static void cpuid(uint32_t leaf, uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d)
{ __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0)); }
static uint64_t msr(uint32_t n)
{ uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return a|((uint64_t)d<<32); }
static uint32_t local_id(void)
{ uint32_t a,b,c,d;cpuid(1,&a,&b,&c,&d);return b>>24; }
static enum smp_vmx_status status(struct smp_vmx_context *ctx,enum smp_vmx_status s)
{ if(ctx)ctx->status=s;return s; }
static bool interrupts_disabled(void)
{ uint64_t flags;__asm__ volatile("pushfq; popq %0":"=r"(flags)::"memory");return !(flags&(1u<<9)); }
static bool restore(struct smp_vmx_context *ctx)
{
    __asm__ volatile("mov %0,%%cr4"::"r"(ctx->saved_cr4):"memory");
    __asm__ volatile("mov %0,%%cr0"::"r"(ctx->saved_cr0):"memory");
    uint64_t cr0,cr4;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    return cr0==ctx->saved_cr0 && cr4==ctx->saved_cr4;
}
enum smp_vmx_status smp_vmx_local_enter(struct smp_vmx_context *ctx)
{
    if (!ctx || ctx->phase!=SMP_VMX_READY || ctx->root_live || !ctx->region_view ||
        ((uintptr_t)ctx->region_view&4095)) return status(ctx,SMP_VMX_BAD_CONTEXT);
    if (!interrupts_disabled()) return status(ctx,SMP_VMX_BLOCKED_IF);
    uint32_t a,b,c,d;
    cpuid(1,&a,&b,&c,&d);
    ctx->owner_apic_id=b>>24;
    ctx->phase=SMP_VMX_FAILED;
    /* This branch must precede every VMX MSR and VMX instruction. */
    if (!(c&(1u<<5))) return status(ctx,SMP_VMX_BLOCKED_NO_VMX);
    struct vmx_caps caps={0};
    caps.feature_control=msr(0x3a);
    if ((caps.feature_control&5)!=5) return status(ctx,SMP_VMX_BLOCKED_FEATURE_CONTROL);
    caps.basic=msr(0x480);
    caps.cr0_fixed0=msr(0x486);caps.cr0_fixed1=msr(0x487);
    caps.cr4_fixed0=msr(0x488);caps.cr4_fixed1=msr(0x489);
    __asm__ volatile("mov %%cr0,%0":"=r"(caps.cr0));
    __asm__ volatile("mov %%cr4,%0":"=r"(caps.cr4));
    cpuid(0x80000000,&a,&b,&c,&d);
    unsigned bits=36;
    if (a>=0x80000008) { cpuid(0x80000008,&a,&b,&c,&d);bits=a&255; }
    uint64_t cr0;
    enum smp_vmx_status s=smp_vmx_policy(&caps,ctx->region_hpa,bits,&cr0);
    if (s!=SMP_VMX_OK) return status(ctx,s);
    if (!vmx_memory_region_wb(ctx->region_hpa)) return status(ctx,SMP_VMX_BLOCKED_WB);
    volatile uint32_t *region=ctx->region_view;
    for(unsigned i=0;i<1024;i++)region[i]=0;
    region[0]=(uint32_t)caps.basic&0x7fffffff;
    ctx->saved_cr0=caps.cr0;ctx->saved_cr4=caps.cr4;
    uint64_t cr4=caps.cr4|VMXE;
    __asm__ volatile("mov %0,%%cr0"::"r"(cr0):"memory");
    __asm__ volatile("mov %0,%%cr4"::"r"(cr4):"memory");
    unsigned char cf,zf;
    __asm__ volatile("vmxon %2; setc %0; setz %1":"=qm"(cf),"=qm"(zf):"m"(ctx->region_hpa):"cc","memory");
    if(cf||zf){
        bool restored=restore(ctx);
        return status(ctx,restored?SMP_VMX_ON_FAILED:SMP_VMX_RESTORE_FAILED);
    }
    ctx->root_live=true;ctx->phase=SMP_VMX_ENTERED;
    return status(ctx,SMP_VMX_OK);
}
enum smp_vmx_status smp_vmx_local_check(struct smp_vmx_context *ctx)
{
    if(!ctx || !ctx->root_live)return status(ctx,SMP_VMX_BAD_CONTEXT);
    if(!interrupts_disabled())return status(ctx,SMP_VMX_BLOCKED_IF);
    if(local_id()!=ctx->owner_apic_id)return status(ctx,SMP_VMX_WRONG_CPU);
    uint64_t current=0;
    unsigned char cf,zf;
    __asm__ volatile("vmptrst %2; setc %0; setz %1":"=qm"(cf),"=qm"(zf),"=m"(current)::"cc","memory");
    ctx->last_vmcs=current;
    if(cf||zf)return status(ctx,SMP_VMX_PTR_FAILED);
    if(current!=UINT64_MAX)return status(ctx,SMP_VMX_CURRENT_VMCS);
    ctx->phase=SMP_VMX_CHECKED;return status(ctx,SMP_VMX_OK);
}
enum smp_vmx_status smp_vmx_local_leave(struct smp_vmx_context *ctx)
{
    if(!ctx || !ctx->root_live)return status(ctx,SMP_VMX_BAD_CONTEXT);
    if(!interrupts_disabled())return status(ctx,SMP_VMX_BLOCKED_IF);
    if(local_id()!=ctx->owner_apic_id)return status(ctx,SMP_VMX_WRONG_CPU);
    unsigned char cf,zf;
    __asm__ volatile("vmxoff; setc %0; setz %1":"=qm"(cf),"=qm"(zf)::"cc","memory");
    if(cf||zf)return status(ctx,SMP_VMX_OFF_FAILED);
    ctx->root_live=false;
    if(!restore(ctx)){ctx->phase=SMP_VMX_FAILED;return status(ctx,SMP_VMX_RESTORE_FAILED);}
    ctx->phase=SMP_VMX_EXITED;
    return status(ctx,SMP_VMX_OK);
}
#endif
