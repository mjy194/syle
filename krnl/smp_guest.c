#include "smp_guest.h"
#include "vmx_guest.h"
#include "vmx_memory.h"
#include <stddef.h>
_Static_assert(offsetof(struct smp_guest_frame,guest_cr2)==120,"assembly guest CR2");
_Static_assert(offsetof(struct smp_guest_frame,host_cr2)==128,"assembly host CR2");
_Static_assert(sizeof(struct smp_guest_frame)==136,"assembly frame");
struct segment { uint32_t limit,ar; uint64_t base; };
static uint64_t msr(uint32_t n)
{ uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return a|((uint64_t)d<<32); }
static uint32_t cpu_id(void)
{uint32_t a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));return b>>24;}
static bool put(uint64_t f,uint64_t v)
{unsigned char fail;__asm__ volatile("vmwrite %2,%1; setna %0":"=qm"(fail):"r"(f),"r"(v):"cc","memory");return !fail;}
static bool get(uint64_t f,uint64_t *v)
{unsigned char fail;__asm__ volatile("vmread %2,%1; setna %0":"=qm"(fail),"=r"(*v):"r"(f):"cc","memory");return !fail;}
static bool segment(const struct smp_guest_table *g,uint16_t sel,struct segment *s)
{
    *s=(struct segment){.ar=1u<<16};
    if (!sel) return true;
    unsigned off=sel&~7u;
    if ((sel&7) || off+7>g->limit) return false;
    uint64_t d=*(const uint64_t *)(uintptr_t)(g->base+off);
    s->base=((d>>16)&0xffffff)|((d>>32)&0xff000000);
    s->limit=(d&0xffff)|((d>>32)&0xf0000);
    s->ar=((d>>40)&0xff)|((d>>40)&0xf000);
    if(s->ar&0x8000)s->limit=(s->limit<<12)|0xfff;
    if(!(s->ar&0x10)){
        if(off+15>g->limit)return false;
        s->base|=*(const uint64_t *)(uintptr_t)(g->base+off+8)<<32;
    }
    return true;
}
static bool snapshot(struct smp_guest_host *h,struct segment seg[8])
{
    __asm__ volatile("sgdt %0":"=m"(h->gdtr));
    __asm__ volatile("sidt %0":"=m"(h->idtr));
    __asm__ volatile("mov %%cr0,%0":"=r"(h->cr0));
    __asm__ volatile("mov %%cr3,%0":"=r"(h->cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(h->cr4));
    __asm__ volatile("mov %%cr2,%0":"=r"(h->cr2));
    __asm__ volatile("mov %%es,%0":"=r"(h->selectors[0]));
    __asm__ volatile("mov %%cs,%0":"=r"(h->selectors[1]));
    __asm__ volatile("mov %%ss,%0":"=r"(h->selectors[2]));
    __asm__ volatile("mov %%ds,%0":"=r"(h->selectors[3]));
    __asm__ volatile("mov %%fs,%0":"=r"(h->selectors[4]));
    __asm__ volatile("mov %%gs,%0":"=r"(h->selectors[5]));
    __asm__ volatile("sldt %0":"=r"(h->selectors[6]));
    __asm__ volatile("str %0":"=r"(h->selectors[7]));
    if(h->selectors[6] || !h->selectors[1] || !h->selectors[7])return false;
    for(unsigned i=0;i<8;i++)if(!segment(&h->gdtr,h->selectors[i],&seg[i]))return false;
    h->fs_base=msr(0xc0000100);h->gs_base=msr(0xc0000101);
    h->pat=msr(0x277);h->efer=msr(0xc0000080);
    h->sysenter_cs=msr(0x174);h->sysenter_esp=msr(0x175);h->sysenter_eip=msr(0x176);
    return true;
}
static bool restore_check(const struct smp_guest_host *h)
{
    /* VM exit loads bases but gives GDTR/IDTR architectural maximum limits. */
    __asm__ volatile("lgdt %0; lidt %1"::"m"(h->gdtr),"m"(h->idtr):"memory");
    struct smp_guest_table gdtr,idtr;
    uint64_t cr0,cr2,cr3,cr4,flags;
    __asm__ volatile("sgdt %0":"=m"(gdtr));__asm__ volatile("sidt %0":"=m"(idtr));
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));__asm__ volatile("mov %%cr2,%0":"=r"(cr2));
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));__asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    __asm__ volatile("pushfq; popq %0":"=r"(flags));
    return !(flags&(1u<<9)) && cr0==h->cr0 && cr2==h->cr2 && cr3==h->cr3 && cr4==h->cr4 &&
        gdtr.base==h->gdtr.base && gdtr.limit==h->gdtr.limit && idtr.base==h->idtr.base && idtr.limit==h->idtr.limit &&
        msr(0xc0000100)==h->fs_base && msr(0xc0000101)==h->gs_base &&
        msr(0x277)==h->pat && msr(0xc0000080)==h->efer &&
        msr(0x174)==h->sysenter_cs && msr(0x175)==h->sysenter_esp && msr(0x176)==h->sysenter_eip;
}
#define W(f,v) do {if(!put(f,v))goto clear;}while(0)
static bool local_start(struct smp_guest_context *ctx,bool retain)
{
    if(!ctx)return false;
    ctx->status=SMP_GUEST_BAD_CONTEXT;
    if(ctx->vmcs_live || !ctx->vmcs_hpa || (ctx->vmcs_hpa&4095) || !ctx->vmcs_view ||
        ((uintptr_t)ctx->vmcs_view&4095) || (ctx->eptp&4095)!=0x1e ||
        (ctx->eptp>>48) || !(ctx->eptp&~4095ull) ||
        (ctx->stack_top!=0xa000 && ctx->stack_top!=0xb000) ||
        ctx->vmcall_gpa<0x6000 || ctx->vmcall_gpa>0x6ffd)return false;
    if(cpu_id()!=ctx->owner_apic_id){ctx->status=SMP_GUEST_WRONG_CPU;return false;}
    ctx->status=SMP_GUEST_BLOCKED;
    uint64_t flags,dr7,current;
    __asm__ volatile("pushfq; popq %0":"=r"(flags));
    if(flags&(1u<<9))return false;
    __asm__ volatile("mov %%dr7,%0":"=r"(dr7));
    if(dr7!=0x400 || msr(0x1d9))return false;
    uint64_t basic=msr(0x480);
    unsigned size=(basic>>32)&0x1fff;
    if(!size || size>4096 || (basic&(1ull<<31)) || ((basic>>50)&15)!=6 ||
        (ctx->vmcs_hpa>>48) || ((basic&(1ull<<48)) && ctx->vmcs_hpa>=(1ull<<32)) ||
        !vmx_memory_region_wb(ctx->vmcs_hpa))return false;
    unsigned char fail;
    __asm__ volatile("vmptrst %1; setna %0":"=qm"(fail),"=m"(current)::"cc","memory");
    if(fail || current!=UINT64_MAX)return false;
    bool true_ctls=(basic>>55)&1;
    uint32_t pin,primary,secondary,entry,exit;
    uint64_t pcaps=msr(true_ctls?0x48e:0x482);
    if(!(pcaps&(1ull<<63)) || !vmx_guest_controls(msr(0x48b),2,2,&secondary))return false;
    uint64_t ecaps=msr(0x48c),need=(1ull<<6)|(1ull<<14)|(1ull<<20)|(1ull<<25);
    if((ecaps&need)!=need)return false;
    uint32_t ew=(1u<<2)|(1u<<9)|(1u<<14)|(1u<<15),xw=(1u<<2)|(1u<<9)|(15u<<18);
    if(ctx->kick_mode)xw|=1u<<15; /* acknowledge actual external vector on VM exit */
    if(!vmx_guest_controls(msr(true_ctls?0x48d:0x481),1,0x1f,&pin) ||
       !vmx_guest_controls(pcaps,1u<<31,0x0401e172u|(1u<<31),&primary) ||
       !vmx_guest_controls(msr(true_ctls?0x490:0x484),ew,0x11ffu|ew,&entry) ||
       !vmx_guest_controls(msr(true_ctls?0x48f:0x483),xw,0x36dffu|xw,&exit))return false;
    struct segment seg[8];
    if(!snapshot(&ctx->host,seg) || !(ctx->host.cr4&(1u<<13)) ||
        (ctx->host.cr4&((1u<<12)|(1u<<23))))return false; /* no LA57/CET guest */
    struct smp_guest_host *h=&ctx->host;
    volatile uint32_t *region=ctx->vmcs_view;
    for(unsigned i=0;i<1024;i++)region[i]=0;
    region[0]=(uint32_t)basic&0x7fffffff;
    ctx->vmcs_live=true;ctx->cleared=false;ctx->host_restored=false;
    ctx->status=SMP_GUEST_CLEAR_FAILED;
    __asm__ volatile("vmclear %1; setna %0":"=qm"(fail):"m"(ctx->vmcs_hpa):"cc","memory");
    if(fail)return false;
    ctx->status=SMP_GUEST_LOAD_FAILED;
    __asm__ volatile("vmptrld %1; setna %0":"=qm"(fail):"m"(ctx->vmcs_hpa):"cc","memory");
    if(fail)goto clear;
    ctx->status=SMP_GUEST_SETUP_FAILED;
    W(0x401e,secondary);W(0x201a,ctx->eptp);
    W(0x4000,pin);W(0x4002,primary);W(0x4004,0xffffffffu);
    W(0x4006,0);W(0x4008,0);W(0x400a,0);W(0x400c,exit);W(0x400e,0);W(0x4010,0);
    W(0x4012,entry);W(0x4014,0);W(0x4016,0);W(0x4018,0);W(0x401a,0);W(0x2010,0);
    W(0x6000,ctx->cache_guard?UINT64_C(0x60000000):0);W(0x6002,0);W(0x6004,h->cr0);W(0x6006,h->cr4);
    W(0x2800,UINT64_MAX);W(0x2802,0);W(0x2804,h->pat);W(0x2806,h->efer);
    W(0x2c00,h->pat);W(0x2c02,h->efer);
    for(unsigned i=0;i<8;i++){
        uint64_t sel=0,ar=1u<<16,base=0,limit=0;
        if(i==1){sel=8;ar=0xa09b;limit=0xffffffff;}
        if(i==0 || i==2 || i==3){sel=16;ar=0xc093;limit=0xffffffff;}
        if(i==7){sel=24;ar=0x8b;base=0x5100;limit=103;}
        W(0x800+2*i,sel);W(0x4800+2*i,limit);W(0x4814+2*i,ar);W(0x6806+2*i,base);
    }
    W(0x4810,39);W(0x4812,0);W(0x6816,0x5000);W(0x6818,0x5800);
    W(0x4824,0);W(0x4826,0);W(0x6822,0);
    W(0x6800,h->cr0);W(0x6802,0x1000);W(0x6804,h->cr4);W(0x681a,0x400);
    W(0x681c,ctx->stack_top);W(0x681e,0x6000);W(0x6820,2);
    W(0x482a,0);W(0x6824,0);W(0x6826,0);
    for(unsigned i=0;i<6;i++)W(0xc00+2*i,h->selectors[i]);
    W(0xc0c,h->selectors[7]);W(0x6c00,h->cr0);W(0x6c02,h->cr3);W(0x6c04,h->cr4);
    W(0x6c06,h->fs_base);W(0x6c08,h->gs_base);W(0x6c0a,seg[7].base);
    W(0x6c0c,h->gdtr.base);W(0x6c0e,h->idtr.base);
    W(0x4c00,h->sysenter_cs);W(0x6c10,h->sysenter_esp);W(0x6c12,h->sysenter_eip);
    struct {uint64_t eptp,reserved;} inv={ctx->eptp,0};
    uint64_t kind=1;
    ctx->status=SMP_GUEST_FLUSH_FAILED;
    __asm__ volatile("invept %1,%2; setna %0":"=qm"(fail):"m"(inv),"r"(kind):"cc","memory");
    if(fail)goto clear;
    ctx->frame=(struct smp_guest_frame){.r8=ctx->owner_token,.r10=ctx->kick_mode?1:0};
    ctx->status=SMP_GUEST_ENTER_FAILED;
    ctx->enter_result=(unsigned)smp_guest_enter(&ctx->frame,0);
    ctx->host_restored=restore_check(h);
    if(!ctx->host_restored){ctx->status=SMP_GUEST_HOST_MISMATCH;goto clear;}
    if(ctx->enter_result){(void)get(0x4400,&ctx->instruction_error);goto clear;}
    ctx->status=SMP_GUEST_EXIT_MISMATCH;
    if(!get(0x4402,&ctx->exit_reason) || !get(0x681e,&ctx->exit_rip) ||
       !get(0x440c,&ctx->exit_length) || !get(0x681c,&ctx->exit_rsp))goto clear;
    if(ctx->kick_mode) {
        if(!get(0x4404,&ctx->exit_info)||!get(0x6820,&ctx->exit_flags)||!smp_guest_kick_valid(ctx))goto clear;
        ctx->kicked=true;
    } else if(!smp_guest_exit_valid(ctx))goto clear;
    if(cpu_id()!=ctx->owner_apic_id)goto clear;
    ctx->status=SMP_GUEST_OK;
    if(retain){ctx->launched=true;return true;}
clear:
    __asm__ volatile("vmclear %1; setna %0":"=qm"(fail):"m"(ctx->vmcs_hpa):"cc","memory");
    if(fail){ctx->status=SMP_GUEST_CLEAR_FAILED;return false;}
    ctx->vmcs_live=false;ctx->cleared=true;
    return ctx->status==SMP_GUEST_OK;
}

/* Retained VMCS remains active on this owner CPU only. No hidden invalidation
 * or CLEAR occurs between warm and resume: shootdown is explicit. */
static bool current_owner(struct smp_guest_context *c) {
    uint64_t current,flags; unsigned char fail;
    if(!c||!c->vmcs_live||cpu_id()!=c->owner_apic_id)return false;
    __asm__ volatile("pushfq; popq %0":"=r"(flags));
    if(flags&(1u<<9))return false;
    __asm__ volatile("vmptrst %1; setna %0":"=qm"(fail),"=m"(current)::"cc","memory");
    return !fail&&current==c->vmcs_hpa;
}
bool smp_guest_local_run(struct smp_guest_context *c){return local_start(c,false);}
bool smp_guest_local_warm(struct smp_guest_context *c){return local_start(c,true);}
bool smp_guest_local_invalidate(struct smp_guest_context *c) {
    if(!current_owner(c))return false;
    struct {uint64_t eptp,reserved;} inv={c->eptp,0};
    unsigned char fail; uint64_t kind=1;
    __asm__ volatile("invept %1,%2; setna %0":"=qm"(fail):"m"(inv),"r"(kind):"cc","memory");
    if(fail){c->status=SMP_GUEST_FLUSH_FAILED;return false;}
    return true;
}
bool smp_guest_local_clear(struct smp_guest_context *c) {
    if(!current_owner(c))return false;
    unsigned char fail;
    __asm__ volatile("vmclear %1; setna %0":"=qm"(fail):"m"(c->vmcs_hpa):"cc","memory");
    if(fail){c->status=SMP_GUEST_CLEAR_FAILED;return false;}
    c->vmcs_live=false;c->cleared=true;c->launched=false;return true;
}
bool smp_guest_local_resume(struct smp_guest_context *c,uint64_t expected) {
    if(!current_owner(c)||!c->launched||c->status!=SMP_GUEST_OK||
       !(c->kicked?smp_guest_kick_valid(c):smp_guest_resume_valid(c)))return false;
    /* Prior exit must be our completed VMCALL, not an arbitrary fault. */
    if(!c->kicked&&!put(0x681e,c->exit_rip+3))return false;
    c->expected_data=expected;
    c->status=SMP_GUEST_ENTER_FAILED;
    c->enter_result=(unsigned)smp_guest_enter(&c->frame,1);
    c->host_restored=restore_check(&c->host);
    if(!c->host_restored){c->status=SMP_GUEST_HOST_MISMATCH;return false;}
    if(c->enter_result){(void)get(0x4400,&c->instruction_error);return false;}
    c->status=SMP_GUEST_EXIT_MISMATCH;
    if(!get(0x4402,&c->exit_reason)||!get(0x681e,&c->exit_rip)||
       !get(0x440c,&c->exit_length)||!get(0x681c,&c->exit_rsp)||
       !smp_guest_exit_valid(c)||cpu_id()!=c->owner_apic_id)return false;
    c->kicked=false;c->status=SMP_GUEST_OK;return true;
}
