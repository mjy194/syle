#ifdef VMX_NESTED_TEST
#include "nested_guest.h"
#endif
#ifdef VMX_IOAPIC_TEST
#include "device_guest.h"
#endif
#include "vmx_guest.h"
#ifdef VMX_NESTED_VCPU_TEST
#include "nested_vcpu.h"
#define ept (vcpu->fixture)
#define vmx_guest_regs (vcpu->frame.regs)
#define vmx_guest_cr2 (vcpu->frame.guest_cr2)
#define vmx_host_cr2 (vcpu->frame.host_cr2)
#endif
#include "paging.h"
#ifdef VMX_APIC_TEST
#include "lapic.h"
#ifdef VMX_TIMER_TEST
#include "lapic_timer.h"
static uint64_t timer_now(void) {
    uint32_t lo,hi;
    __asm__ volatile("lfence;rdtsc":"=a"(lo),"=d"(hi)::"memory");
    return ((uint64_t)hi<<32)|lo;
}
#endif
#ifdef VMX_COLLIDE_DELIVER_TEST
extern const unsigned char ept_collision_ud_ack[];
#endif
#ifdef VMX_MOVSS_TEST
extern const unsigned char ept_movss_fault[];
#endif
#include "vcpu.h"
extern const unsigned char ept_apic_halt[],ept_apic_ack[],ept_apic_first_isr[],ept_apic_return[];
#endif
#if defined(VMX_PF_TEST) || defined(VMX_NESTED_PF_INJECT_TEST)
#ifndef VMX_NESTED_VCPU_TEST
uint64_t vmx_host_cr2,vmx_guest_cr2;
#endif
extern const unsigned char ept_pf_fault[],ept_pf_ack[];
#endif
#ifdef VMX_EVENT_TEST
#include "vcpu.h"
#ifdef VMX_GP_TEST
extern const unsigned char ept_gp_fault[],ept_gp_ack[],ept_gp_return[];
#endif
#ifdef VMX_WAKE_TEST
extern const unsigned char ept_irq_ack[],ept_final_halt[];
#endif
extern const unsigned char ept_ud_fault[],ept_ud_ack[],ept_event_halt[];
#endif
#ifdef VMX_EXIT_TEST
#include "vmexit.h"
extern const unsigned char ept_cpuid0[],ept_cpuid1[],ept_cpuid2[],ept_cpuid3[],ept_halt[];
#endif
#ifdef VMX_EPT_TEST
#include "ept.h"
#ifndef VMX_NESTED_VCPU_TEST
static struct ept_fixture ept;
void ept_guest_release(void) { ept_fixture_destroy(&ept); }
#endif
#endif
#include "../efi/src/exceptions.h"

struct table { uint16_t limit; uint64_t base; } __attribute__((packed));
struct segment { uint16_t selector; uint32_t limit, ar; uint64_t base; };
#ifndef VMX_NESTED_VCPU_TEST
static unsigned char guest_stack[16384] __attribute__((aligned(16)));
struct vmx_guest_registers vmx_guest_regs;
#endif
_Static_assert(sizeof(struct vmx_guest_registers) == 15 * 8, "assembly GPR layout");
extern int vmx_guest_enter(unsigned resume);
extern void vmx_guest_payload(void);
extern void vmx_guest_vmcall0(void), vmx_guest_vmcall1(void), vmx_guest_vmcall2(void);
static uint64_t msr(uint32_t n) {
    uint32_t lo,hi;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(n));
    return ((uint64_t)hi<<32)|lo;
}
static bool put(uint64_t f,uint64_t v) {
    unsigned char fail;
    __asm__ volatile("vmwrite %2,%1; setna %0":"=qm"(fail):"r"(f),"r"(v):"cc","memory");
    return !fail;
}
static bool get(uint64_t f,uint64_t *v) {
    unsigned char fail;
    __asm__ volatile("vmread %2,%1; setna %0":"=qm"(fail),"=r"(*v):"r"(f):"cc","memory");
    return !fail;
}
static bool segment(const struct table *g,uint16_t selector,struct segment *s) {
    *s=(struct segment){.selector=selector,.ar=1u<<16};
    if (!(selector&~3u)) return true;
    unsigned off=selector&~7u;
    if ((selector&4) || off+7>g->limit) return false;
    uint64_t d=*(const uint64_t *)(uintptr_t)(g->base+off);
    s->base=((d>>16)&0xffffff)|((d>>32)&0xff000000);
    s->limit=(d&0xffff)|((d>>32)&0xf0000);
    s->ar=((d>>40)&0xff)|((d>>40)&0xf000);
    if (s->ar&0x8000) s->limit=(s->limit<<12)|0xfff;
    if (!(s->ar&0x10)) {
        if (off+15>g->limit) return false;
        s->base|=(*(const uint64_t *)(uintptr_t)(g->base+off+8))<<32;
    }
    return true;
}
#define W(f,v) do { if (!put((f),(v))) goto setup_failed; } while(0)
#ifdef VMX_NESTED_VCPU_TEST
#ifdef VMX_NESTED_PARALLEL_TEST
#include "nested_parallel.h"
#define log_text(s) nested_parallel_trace_text(vcpu->parallel,(s))
#define log_hex(v) nested_parallel_trace_hex(vcpu->parallel,(v))
#define panic(s) nested_parallel_fault_stop(vcpu->parallel,1,(s))
#endif
bool vmx_guest_run_context(struct nested_vcpu *vcpu,uint64_t physical,uint64_t basic) {
#else
bool vmx_guest_run(uint64_t physical,uint64_t basic) {
#endif
    unsigned char fail;
    uint64_t flags,cr0,cr3,cr4,dr7;
    __asm__ volatile("pushfq; pop %0":"=r"(flags));
    if (flags&(1u<<9)) { log_text("VMX.GUEST_BLOCKED interrupts enabled\n");return false; }
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    __asm__ volatile("mov %%dr7,%0":"=r"(dr7));
#ifdef VMX_STACKPART_TEST
    if(cr4&(1u<<23)) { log_text("VCPU.STACKPART_BLOCKED CET_ACTIVE\n");return false; }
#endif
    /* Avoid silently changing preexisting debug state on VM exit. */
    if (dr7!=0x400 || msr(0x1d9)!=0) { log_text("VMX.GUEST_BLOCKED active debug state\n");return false; }
    uint32_t pin,primary,entry,exit;
    uint32_t primary_want=0;
#ifdef VMX_EPT_TEST
    uint32_t secondary;
    /* EPT_VPID_CAP read only when EPT is advertised in secondary controls. */
    if (!(msr((basic&(1ULL<<55)) ? 0x48e : 0x482) >> 63)) {
        log_text("EPT.BLOCKED NO_SECONDARY_CONTROLS\n");return false;
    }
    if (!vmx_guest_controls(msr(0x48b),2,2,&secondary) ||
        !ept_caps_supported(msr(0x48c))) {
        log_text("EPT.BLOCKED CAPABILITIES\n");return false;
    }
#if defined(VMX_NESTED_EPT_AD_TEST) || defined(VMX_NESTED_OWNER_AD_TEST) || defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
    if(!(msr(0x48c)&(UINT64_C(1)<<21))){log_text("NESTED.AD_BLOCKED NO_PHYSICAL_AD\n");return false;}
#endif
    primary_want=1u<<31;
#endif
    bool true_ctls=(basic>>55)&1;
    /* Legacy must-one/reserved defaults plus only implemented controls.
     * PAT/EFER fields are written only after capability support is required. */
    uint32_t entry_want=(1u<<2)|(1u<<9)|(1u<<14)|(1u<<15);
    uint32_t exit_want=(1u<<2)|(1u<<9)|(15u<<18);
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    exit_want|=1u<<15; /* L0 owns/acknowledges physical external IRQs. */
#endif
    uint32_t primary_supported=0x0401e172u|(1u<<7)|(1u<<9)|(1u<<10)|
        (1u<<11)|(1u<<12)|(1u<<15)|(1u<<16)|(1u<<19)|(1u<<20)|
        (1u<<23)|(1u<<24)|(1u<<29)|(1u<<30);
#if defined(VMX_EXIT_TEST) || defined(VMX_EVENT_TEST) || defined(VMX_APIC_TEST)
    primary_want |= 1u<<7; /* HLT exiting; CPUID exits unconditionally in VMX. */
#endif
    primary_supported |= primary_want;
#ifdef VMX_TIMER_TEST
    if(!lapic_timer_caps(msr(true_ctls?0x48d:0x481),msr(true_ctls?0x48f:0x483))) {
        log_text("APIC.TIMER_BLOCKED CONTROLS_NOT_TOGGLEABLE\n");return false;
    }
    uint32_t ca,cb,cc,cd;
    __asm__ volatile("cpuid":"=a"(ca),"=b"(cb),"=c"(cc),"=d"(cd):"a"(0x80000000u),"c"(0));
    if(ca<0x80000007u) {log_text("APIC.TIMER_BLOCKED NO_INVARIANT_TSC_LEAF\n");return false;}
    __asm__ volatile("cpuid":"=a"(ca),"=b"(cb),"=c"(cc),"=d"(cd):"a"(0x80000007u),"c"(0));
    if(!(cd&(1u<<8))) {log_text("APIC.TIMER_BLOCKED NO_INVARIANT_TSC\n");return false;}
    unsigned timer_shift=msr(0x485)&31;
#endif
    uint32_t pin_want=0;
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    pin_want=1;
#endif
#if defined(VMX_WAKE_TEST) || defined(VMX_APIC_TEST)
    if (!vcpu_halt_activity_supported(msr(0x485))) {
        log_text("VCPU.WAKE_BLOCKED NO_HLT_ACTIVITY_SUPPORT\n");
        return false;
    }
    pin_want=1; /* Physical IRQs exit, never route to the minimal guest IDT. */
#endif
    if (!vmx_guest_controls(msr(true_ctls?0x48d:0x481),pin_want,0x1fu,&pin) ||
        !vmx_guest_controls(msr(true_ctls?0x48e:0x482),primary_want,primary_supported,&primary) ||
        !vmx_guest_controls(msr(true_ctls?0x48f:0x483),exit_want,
                            0x36dffu|exit_want,&exit) ||
        !vmx_guest_controls(msr(true_ctls?0x490:0x484),entry_want,
                            0x11ffu|entry_want,&entry)) {
        log_text("VMX.GUEST_BLOCKED unsupported controls\n");return false;
    }
#ifdef VMX_DEMAND_TEST
    if(primary&(1u<<9)) {
        log_text("VCPU.DEMAND_BLOCKED INVLPG_EXITING_FORCED\n");
        return false;
    }
#endif
#if defined(VMX_WINDOW_TEST) || defined(VMX_IOAPIC_TEST)
    if(!lapic_window_caps(msr(true_ctls?0x48e:0x482))) {
        log_text("APIC.WINDOW_BLOCKED CONTROL_NOT_TOGGLEABLE\n");return false;
    }
#endif
#ifdef VMX_APIC_TEST
    if(primary&(1u<<28)) {
        log_text("APIC.BLOCKED MSR_BITMAPS_MUST_BE_OFF\n");return false;
    }
#endif
    struct table gdtr,idtr;
    __asm__ volatile("sgdt %0":"=m"(gdtr));
    __asm__ volatile("sidt %0":"=m"(idtr));
    uint16_t sel[8];
    __asm__ volatile("mov %%es,%0":"=r"(sel[0]));
    __asm__ volatile("mov %%cs,%0":"=r"(sel[1]));
    __asm__ volatile("mov %%ss,%0":"=r"(sel[2]));
    __asm__ volatile("mov %%ds,%0":"=r"(sel[3]));
    __asm__ volatile("mov %%fs,%0":"=r"(sel[4]));
    __asm__ volatile("mov %%gs,%0":"=r"(sel[5]));
    __asm__ volatile("sldt %0":"=r"(sel[6]));
    __asm__ volatile("str %0":"=r"(sel[7]));
    struct segment seg[8];
    for (unsigned i = 0; i < 8; ++i) {
        if (!segment(&gdtr, sel[i], &seg[i])) {
            log_text("VMX.GUEST_BAD_SEGMENT index=");
            log_hex(i);
            log_text("selector=");
            log_hex(sel[i]);
            return false;
        }
    }
    for (unsigned i = 0; i < 8; ++i) {
        if (sel[i] & 7) {
            log_text("VMX.GUEST_BAD_SELECTOR index=");
            log_hex(i);
            log_text("selector=");
            log_hex(sel[i]);
            return false;
        }
    }
    if (sel[6] || !sel[1] || !sel[7]) {
        log_text("VMX.GUEST_BAD_LDT_CS_TR\n");
        return false;
    }
    seg[4].base=msr(0xc0000100);seg[5].base=msr(0xc0000101);
    volatile uint32_t *region=paging_phys_to_virt(physical);
    if (!region) return false;
    for (unsigned i=0;i<1024;i++) region[i]=0;
    region[0]=(uint32_t)basic&0x7fffffff;
    __asm__ volatile("vmclear %1; setna %0":"=qm"(fail):"m"(physical):"cc","memory");
    if (fail) { log_text("VMX.VMCLEAR_FAILED\n");return false; }
#ifdef VMX_NESTED_VCPU_TEST
    uint32_t ca,cb,cc,cd;__asm__ volatile("cpuid":"=a"(ca),"=b"(cb),"=c"(cc),"=d"(cd):"a"(1),"c"(0));
    #ifdef VMX_NESTED_PARALLEL_TEST
    if(!vcpu->parallel||vcpu->owner_apic!=(cb>>24)||vcpu->vmcs01!=physical||vcpu->busy||vcpu->poison)return false;
#else
    if(!nested_vcpu_init(vcpu,cb>>24,physical))return false;
#endif
#endif
    __asm__ volatile("vmptrld %1; setna %0":"=qm"(fail):"m"(physical):"cc","memory");
    if (fail) { log_text("VMX.VMPTRLD_FAILED\n");return false; }
    bool success=false;
#ifdef VMX_EPT_TEST
    #ifdef VMX_NESTED_PARALLEL_TEST
    if(!ept.started||!ept.eptp||!ept_fixture_flush(&ept)){log_text("EPT.PREPARED_FLUSH_FAILED\n");goto clear;}
#else
    if (!ept_fixture_init(&ept)) {log_text("EPT.ALLOCATION_FAILED\n");goto clear;}
#endif
#endif
#ifdef VMX_EPT_TEST
    W(0x401e,secondary);W(0x201a,ept.eptp);
#endif
    W(0x4000,pin); W(0x4002,primary); W(0x4004,0xffffffffu);
    W(0x4006,0);W(0x4008,0);W(0x400a,0); /* PF mask/match, CR3 targets */
    W(0x400c,exit);W(0x400e,0);W(0x4010,0);
    W(0x4012,entry);W(0x4014,0);W(0x4016,0);W(0x4018,0);W(0x401a,0);
    W(0x2010,0); /* TSC offset, including forced legacy control */
    #if defined(VMX_NESTED_UC_PAT_TEST) || defined(VMX_NESTED_WT_TEST) || defined(VMX_NESTED_OWNER_TYPED_TEST) || defined(VMX_NESTED_ATOMIC_UC_TEST)
    W(0x6000,UINT64_C(0x60000000));
#else
    W(0x6000,0);
#endif
    W(0x6002,0);W(0x6004,cr0);W(0x6006,cr4);
    W(0x2800,UINT64_MAX);W(0x2802,0); /* link pointer and DEBUGCTL */
    W(0x2804,msr(0x277));W(0x2806,msr(0xc0000080));
    W(0x2c00,msr(0x277));W(0x2c02,msr(0xc0000080));
    for (unsigned i=0;i<8;i++) {
        W(0x800+2*i,seg[i].selector);W(0x4800+2*i,seg[i].limit);
        W(0x4814+2*i,seg[i].ar);W(0x6806+2*i,seg[i].base);
    }
    W(0x4810,gdtr.limit);W(0x4812,idtr.limit);
    W(0x6816,gdtr.base);W(0x6818,idtr.base);
    W(0x4824,0);W(0x4826,0);W(0x6822,0); /* interruptibility, activity, pending DB */
    W(0x6800,cr0);W(0x6802,cr3);W(0x6804,cr4);W(0x681a,0x400);
#ifdef VMX_NESTED_VCPU_TEST
    W(0x681c,EPT_STACK_TOP);W(0x681e,EPT_CODE_GPA);W(0x6820,2);
#else
    W(0x681c,(uintptr_t)(guest_stack+sizeof guest_stack));
    W(0x681e,(uintptr_t)vmx_guest_payload);W(0x6820,2);
#endif
    W(0x482a,(uint32_t)msr(0x174));W(0x6824,msr(0x175));W(0x6826,msr(0x176));
    for (unsigned i=0;i<6;i++) W(0xc00+2*i,sel[i]);
    W(0xc0c,sel[7]);
    W(0x6c00,cr0);W(0x6c02,cr3);W(0x6c04,cr4);
    W(0x6c06,seg[4].base);W(0x6c08,seg[5].base);W(0x6c0a,seg[7].base);
    W(0x6c0c,gdtr.base);W(0x6c0e,idtr.base);
    W(0x4c00,(uint32_t)msr(0x174));W(0x6c10,msr(0x175));W(0x6c12,msr(0x176));
#ifdef VMX_EPT_TEST
    /* All guest architectural addresses refer to private guest GPA/GVA pages. */
    for (unsigned i=0;i<8;i++) {
        uint64_t selector=0, ar=1u<<16, base=0, limit=0;
        if(i==1) {selector=8;ar=0xa09b;limit=0xffffffff;}
        if(i==0 || i==2 || i==3) {selector=16;ar=0xc093;limit=0xffffffff;}
        if(i==7) {selector=24;ar=0x8b;base=0x5100;limit=103;}
        W(0x800+2*i,selector);W(0x4800+2*i,limit);
        W(0x4814+2*i,ar);W(0x6806+2*i,base);
    }
    W(0x4810,39);W(0x4812,0);W(0x6816,0x5000);W(0x6818,0x5800);
    #ifdef VMX_EVENT_TEST
    #ifdef VMX_PF_TEST
    W(0x4812,15*16-1);
#elif defined(VMX_GP_TEST)
    W(0x4812,14*16-1);
#elif defined(VMX_WAKE_TEST)
    W(0x4812,0x41*16-1);
#else
    W(0x4812,7*16-1);
#endif /* vector 6 gate within private guest IDT */
#endif
#ifdef VMX_APIC_TEST
    W(0x4812,0x41*16-1);
#endif
    #ifdef VMX_NESTED_DOMAIN_TEST
    W(0x6802,0x1000+vcpu->parallel->cpu_index*0x20000);
#else
    W(0x6802,0x1000);
#endif
    W(0x681c,EPT_STACK_TOP);W(0x681e,EPT_CODE_GPA);
    W(0x482a,0);W(0x6824,0);W(0x6826,0);
    vmx_guest_regs=(struct vmx_guest_registers){0};
#ifdef VMX_NESTED_TEST
#ifdef VMX_NESTED_VCPU_TEST
    success=nested_guest_run_context(vcpu,physical,basic,&ept);
#else
    success=nested_guest_run(physical,basic,&ept);
#endif
    goto clear;
#endif
    /* Exactly five expected exits: normal VMCALL, absent/read, RO/write,
     * NX/execute, final VMCALL. Remediations retry the SAME instruction. */
#ifdef VMX_APIC_TEST
    struct lapic apic;lapic_init(&apic);
#ifdef VMX_IOAPIC_TEST
    if(!device_guest_init()) goto clear;
#endif
    bool injected=false,accepted=false;
#ifdef VMX_TIMER_TEST
    struct lapic_timer timer;
    #ifdef VMX_PERIODIC_TEST
    lapic_timer_init_mode(&timer,timer_now(),true);
    unsigned periodic_acks=0,real_halt_wakes=0;
#else
    lapic_timer_init(&timer,timer_now());
#endif
    bool timer_halted=false,timer_halt_entered=false;
    unsigned masked_expiries=0,timer_irqs=0,timer_exits=0;
    log_text("APIC.TIMER_VMX_TSC_SHIFT=");log_hex(timer_shift);
    log_text("APIC.TIMER_VIRTUAL_BASE_TSC_TICKS=0x400\n");
#endif
#ifdef VMX_COLLIDE_DELIVER_TEST
    bool ud_pending=false,ud_handled=false;
#endif
#ifdef VMX_WINDOW_TEST
    bool window_armed=false;
#endif
    uint8_t selected=0;
    const unsigned max_exits=96;
#elif defined(VMX_EVENT_TEST)
    struct vcpu cpu={.state=VCPU_RUNNABLE};
#ifdef VMX_VECTORING_TEST
    bool vectoring_retried=false;
    uint64_t vectoring_flags=0;
#endif
#ifdef VMX_PF_TEST
    uint64_t host_cr2_baseline;
    __asm__ volatile("mov %%cr2,%0":"=r"(host_cr2_baseline));
    vmx_guest_cr2=0; /* guest's independent initial architectural state */
#endif
    #if defined(VMX_VECTORING_TEST)
    const unsigned max_exits=10;
#elif defined(VMX_WAKE_TEST) || defined(VMX_GP_TEST) || defined(VMX_PF_TEST)
    const unsigned max_exits=9;
#else
    const unsigned max_exits=7;
#endif
#elif defined(VMX_EXIT_TEST)
    const unsigned char *exit_labels[5]={ept_cpuid0,ept_cpuid1,ept_cpuid2,ept_cpuid3,ept_halt};
    const unsigned max_exits=9;
#else
    const unsigned max_exits=5;
#endif
    for(unsigned attempt=0;attempt<max_exits;attempt++) {
        unsigned step=attempt;
#ifdef VMX_VECTORING_TEST
        if(attempt>7) step=attempt-1; /* one extra exit during gate delivery */
#endif
#ifdef VMX_PF_TEST
        /* Known host sentinel proves restoration, then restore the caller's
         * original value immediately, before any C error/cleanup path. */
        __asm__ volatile("mov %0,%%cr2"::"r"(UINT64_C(0x12345000)):"memory");
#endif
#ifdef VMX_TIMER_TEST
        uint32_t remaining=0;
        if(!lapic_timer_reload(&timer,timer_now(),timer_shift,&remaining)) break;
        if(timer.armed) {
            if(timer_halted && !timer_halt_entered && !remaining) {
                log_text("APIC.TIMER_NOT_EXERCISED deadline_before_halted_entry\n");break;
            }
            if(!put(0x482e,remaining)||!put(0x4000,pin|(1u<<6))||
               !put(0x400c,exit|(1u<<22))) break;
        } else {
            /* Paired control writes: no entry between inconsistent settings. */
            if(!put(0x400c,exit)||!put(0x4000,pin)) break;
        }
#endif
        int result=vmx_guest_enter(step!=0);
#ifdef VMX_PF_TEST
        uint64_t host_cr2_now;
        __asm__ volatile("mov %%cr2,%0":"=r"(host_cr2_now));
        __asm__ volatile("mov %0,%%cr2"::"r"(host_cr2_baseline):"memory");
        if(host_cr2_now!=0x12345000) panic("HOST CR2 RESTORE FAILED");
#endif
        __asm__ volatile("lgdt %0; lidt %1"::"m"(gdtr),"m"(idtr):"memory");
        if(result!=1) {
            uint64_t error=0;log_text("EPT.ENTRY_FAILED\n");
            if(get(0x4400,&error)) {log_text("EPT.INSTRUCTION_ERROR=");log_hex(error);}
            break;
        }
        uint64_t reason,rip,rsp,length;
        if(!get(0x4402,&reason)||!get(0x681e,&rip)||!get(0x681c,&rsp)) break;
        log_text("EPT.STEP=");log_hex(step);log_text("EPT.EXIT_REASON=");log_hex(reason);
        log_text("EPT.RIP=");log_hex(rip);
#ifdef VMX_APIC_TEST
        if(step>=4) {
#ifdef VMX_IOAPIC_TEST
            int action=device_guest_exit(&ept,primary,reason,rip,rsp);
            if(action<0) break;
            if(action>0) {success=ept_fixture_data_ok(&ept);break;}
            continue;
#endif
            /* Confirm delivery before ANY timer poll/early-exit path. In
             * particular a preemption exit cannot prove whether delivery
             * completed: reentering with entry-valid could inject twice. */
            if(injected && !accepted) {
                uint64_t vectoring;
                uintptr_t first=EPT_CODE_GPA+(uintptr_t)ept_apic_first_isr-(uintptr_t)ept_payload_start;
                if(!get(0x4408,&vectoring)||
                   !lapic_irq_ack_valid(reason,rip,first,vectoring)) {
                    log_text("APIC.INJECTION_UNCONFIRMED_EXIT_REFUSED\n");break;
                }
                if(!lapic_accept(&apic,selected)||!put(0x4016,0)) break;
                accepted=true;
                log_text("APIC.ACCEPT_COMMITTED IRR_TO_ISR\n");
            }
#ifdef VMX_TIMER_TEST
            if(reason==52) {
                uint64_t saved;
                if(!timer.armed||!get(0x482e,&saved)||saved) break;
                timer_exits++;
                if(timer_halted) timer_halt_entered=true;
                log_text("APIC.TIMER_PREEMPTION_EXIT saved=0\n");
            }
            uint8_t expiry_vector=0;
            uint64_t expiry_deadline=timer.deadline;
            uint64_t expiry_now=timer_now();
            enum lapic_timer_event event=lapic_timer_poll(&timer,expiry_now,&expiry_vector);
            if(event==LAPIC_TIMER_INVALID) break;
            if(event==LAPIC_TIMER_MASKED) {
                masked_expiries++;
                log_text("APIC.TIMER_MASKED_EXPIRED_NO_IRR\n");
            }
#ifdef VMX_PERIODIC_TEST
            if(event==LAPIC_TIMER_IRQ || event==LAPIC_TIMER_MASKED) {
                log_text("APIC.PERIODIC_ELAPSED_PERIODS=");log_hex(timer.last_expirations);
                log_text("APIC.PERIODIC_NEXT_PHASE_TSC=");log_hex(timer.deadline);
            }
            if(event==LAPIC_TIMER_IRQ &&
               (!timer_halted || injected || reason!=52)) {
                if(!lapic_write(&apic,0x83f,expiry_vector)) break;
                log_text("APIC.PERIODIC_PENDING_COALESCED\n");
            } else
#endif
            if(event==LAPIC_TIMER_IRQ) {
                uint64_t f,b,a,p,v;
                uintptr_t expected=EPT_CODE_GPA+(uintptr_t)ept_apic_return-(uintptr_t)ept_payload_start;
                if(reason!=52||!timer_halted||!timer_halt_entered||injected||
                   rip!=expected||rsp!=EPT_STACK_TOP||expiry_vector!=0x40) {
                    log_text("APIC.TIMER_NOT_EXERCISED expired_before_halt_timer_exit\n");break;
                }
                if(!get(0x6820,&f)||!(f&(1u<<9))||!get(0x4824,&b)||b||
                   !get(0x4826,&a)||a>1||!get(0x4016,&p)||(p&(1u<<31))||
                   !get(0x4408,&v)||(v&(1u<<31))) break;
                /* Reuse internal IRR enqueue operation, not a guest self-IPI. */
                if(!lapic_write(&apic,0x83f,expiry_vector)||!lapic_select(&apic,&selected)||
                   selected!=0x40||!put(0x4018,0)||!put(0x401a,0)||
                   !put(0x4016,0x80000000u|selected)) break;
                timer_irqs++;injected=true;
#ifdef VMX_PERIODIC_TEST
                real_halt_wakes++;
                timer_halted=false;
#endif
                log_text("APIC.TIMER_DEADLINE_TSC=");log_hex(expiry_deadline);
                log_text("APIC.TIMER_EXPIRY_OBSERVED_TSC=");log_hex(expiry_now);
                log_text("APIC.TIMER_DEADLINE_REACHED_IRQ_FROM_HALTED_GUEST\n");
                continue;
            }
            if(reason==52) {
                if(timer_halted && !put(0x4826,1)) break; /* preserve virtual HLT if early */
                continue; /* not an instruction; never advance RIP */
            }
#endif
#ifdef VMX_COLLIDE_DELIVER_TEST
            if(ud_pending) {
                uint64_t b,f,irr,isr,a,v;
                uintptr_t ack=EPT_CODE_GPA+(uintptr_t)ept_collision_ud_ack-(uintptr_t)ept_payload_start;
                if(reason!=18||rip!=ack||rsp!=EPT_STACK_TOP-48||
                   vmx_guest_regs.rax!=0xac06||vmx_guest_regs.r13!=1||
                   !get(0x440c,&length)||length!=3||!get(0x4824,&b)||b||
                   !get(0x6820,&f)||(f&(1u<<9))||!get(0x4826,&a)||a||
                   !get(0x4408,&v)||(v&(1u<<31))||
                   !lapic_read(&apic,0x822,&irr)||irr!=1||
                   !lapic_read(&apic,0x812,&isr)||isr) break;
                /* Entry vectoring, NOT a monitor write, consumed MOVSS shadow.
                 * Gate has IF0; IRQ is still pending and not in service. */
                if(!put(0x4016,0)||!put(0x4002,primary|4)||!put(0x681e,rip+3)) break;
                ud_pending=false;ud_handled=true;window_armed=true;
                log_text("APIC.COLLISION_UD_HANDLER_ACK shadow=0 IF=0 IRR_retained=1\n");
                continue;
            }
#endif
#ifdef VMX_MOVSS_TEST
            if(!injected && !window_armed) {
                uint64_t b,p,v;
#ifndef VMX_COLLISION_TEST
                uint64_t f,a;
#endif
                uintptr_t expected=EPT_CODE_GPA+(uintptr_t)ept_movss_fault-(uintptr_t)ept_payload_start;
#ifdef VMX_COLLISION_TEST
                if(reason==0) {
                    uint64_t info,irr;
                    if(rip!=expected||rsp!=EPT_STACK_TOP||!get(0x4404,&info)||
                       info!=0x80000306||!get(0x4824,&b)||!get(0x4016,&p)||
                       !get(0x4408,&v)||(p&(1u<<31))||(v&(1u<<31))||
                       !lapic_read(&apic,0x822,&irr)||irr!=1||lapic_ppr(&apic)!=0) break;
                    log_text("APIC.COLLISION_OBSERVED_UD_BLOCKING=");log_hex(b);
#ifdef VMX_COLLIDE_DELIVER_TEST
                    uint64_t f,a,debug;
                    if(!get(0x6820,&f)||!get(0x4826,&a)||!get(0x6822,&debug)||debug||
                       !lapic_collision_ud_ready(&apic,reason,info,b,f,a,p,v,ud_handled)) break;
                    if(!put(0x4018,0)||!put(0x401a,0)||!put(0x4016,0x80000306)) break;
                    ud_pending=true;
                    log_text("APIC.COLLISION_INJECT_UD_FIRST preserve_shadow=2 preserve_fault_RIP=1\n");
                    continue;
#else
                    log_text("APIC.COLLISION_SAFE_REJECT irq_pending_retained=1 no_injection=1\n");
                    success=ept_fixture_data_ok(&ept);break; /* expected refusal, not guest completion */
#endif
                }
#else
                if(reason==48) {
                    uint64_t q,gpa;
                    if(!get(0x6400,&q)||!get(0x2400,&gpa)||!get(0x4824,&b)||
                       !get(0x6820,&f)||!get(0x4016,&p)||!get(0x4408,&v)||!get(0x4826,&a)||
                       !(f&(1u<<9))||a||(p&(1u<<31))||(v&(1u<<31))||
                       !ept_movss_fault_valid(reason,q,gpa,rip,expected,b)||
                       !lapic_select(&apic,&selected)||selected!=0x40) break;
                    if(!ept_fixture_movss_remediate(&ept)||!put(0x4002,primary|4)) break;
                    window_armed=true;
                    log_text("APIC.MOVSS_BLOCKING_OBSERVED=2 IRQ_NOT_INJECTED\n");
                    log_text("APIC.MOVSS_EPT_RETRY_SAME_RIP_WINDOW_ARMED\n");
                    continue; /* CPU executes original store, clears shadow naturally */
                }
#endif
            }
#endif
#ifdef VMX_WINDOW_TEST
            if(reason==7) {
                uint64_t f,b,a,p,v;
                uintptr_t expected=EPT_CODE_GPA+(uintptr_t)ept_apic_return-(uintptr_t)ept_payload_start;
                if(!window_armed||injected||rip!=expected||rsp!=EPT_STACK_TOP||
                   vmx_guest_regs.r14!=0x51||!get(0x6820,&f)||!get(0x4824,&b)||
                   !get(0x4826,&a)||!get(0x4016,&p)||!get(0x4408,&v)||
                   !lapic_window_ready(&apic,reason,f,b,a,p,v,&selected)||selected!=0x40) break;
                /* Window exit is not an instruction: no length/RIP advance. */
                if(!put(0x4002,primary)) break; /* MUST clear before injection */
                window_armed=false;
                if(!put(0x4018,0)||!put(0x401a,0)||
                   !put(0x4016,0x80000000u|selected)) break;
                injected=true;
                log_text("APIC.WINDOW_READY_SHADOW_CLEAR_INJECT_SAME_RIP\n");
                continue;
            }
#endif
            uintptr_t end=EPT_CODE_GPA+(uintptr_t)ept_payload_end-(uintptr_t)ept_payload_start;
            if(!get(0x440c,&length)||rip<EPT_CODE_GPA||rip>=end||length>end-rip) break;
            if(reason==10 && length==2) {
                /* Experimental single-vCPU CPUID ABI, not a complete OS CPU. */
                if((uint32_t)vmx_guest_regs.rax!=1) break;
                vmx_guest_regs.rax=0;vmx_guest_regs.rbx=0;
                vmx_guest_regs.rcx=(1u<<31)|(1u<<21);
                vmx_guest_regs.rdx=(1u<<6)|(1u<<9);
            } else if((reason==31||reason==32)&&length==2) {
                uint32_t index=(uint32_t)vmx_guest_regs.rcx;
                uint64_t value=((uint64_t)(uint32_t)vmx_guest_regs.rdx<<32)|
                               (uint32_t)vmx_guest_regs.rax;
                if(reason==31) {
#ifdef VMX_TIMER_TEST
                    bool timer_reg=index==0x832||index==0x838||index==0x839||index==0x83e;
                    if(timer_reg) {
                        if(!lapic_timer_read(&timer,index,timer_now(),&value)) break;
                    } else
#endif
                    if(!lapic_read(&apic,index,&value)) break;
                    vmx_guest_regs.rax=(uint32_t)value;
                    vmx_guest_regs.rdx=(uint32_t)(value>>32);
                } else {
#ifdef VMX_TIMER_TEST
                    bool timer_reg=index==0x832||index==0x838||index==0x839||index==0x83e;
                    if(timer_reg) {
                        if(!lapic_timer_write(&timer,index,value,timer_now())) break;
                    } else
#endif
                    if(!lapic_write(&apic,index,value)) break;
                    if(index==0x80b) log_text("APIC.GUEST_EOI\n");
                    if(index==0x83f) {
                        uint8_t blocked;
                        if(lapic_select(&apic,&blocked)) break;
                        log_text("APIC.TPR_MASKED_PENDING_RETAINED\n");
                    }
                }
#ifndef VMX_WINDOW_TEST
            } else if(reason==12 && length==1 && !injected) {
                uint64_t f,b,a,p,v;
                uintptr_t expected=EPT_CODE_GPA+(uintptr_t)ept_apic_halt-(uintptr_t)ept_payload_start;
#ifdef VMX_TIMER_TEST
                if(rip!=expected||rsp!=EPT_STACK_TOP||!timer.armed||timer_halted||
                   !get(0x6820,&f)||!(f&(1u<<9))||!get(0x4824,&b)||b||
                   !get(0x4826,&a)||a||!get(0x4016,&p)||(p&(1u<<31))||
                   !get(0x4408,&v)||(v&(1u<<31))||!put(0x681e,rip+1)||!put(0x4826,1)) break;
#ifdef VMX_PERIODIC_TEST
                if(lapic_select(&apic,&selected)) {
                    if(selected!=0x40||!put(0x4018,0)||!put(0x401a,0)||
                       !put(0x4016,0x80000000u|selected)) break;
                    timer_irqs++;injected=true;
                    log_text("APIC.PERIODIC_RETAINED_PENDING_WAKE\n");continue;
                }
                timer_halt_entered=false;
#endif
                timer_halted=true;
                log_text("APIC.TIMER_GUEST_HALTED_AWAIT_REAL_EXPIRY\n");continue;
#else
                if(rip!=expected||rsp!=EPT_STACK_TOP||!get(0x6820,&f)||
                   !get(0x4824,&b)||!get(0x4826,&a)||!get(0x4016,&p)||!get(0x4408,&v)||
                   !(f&(1u<<9))||b||a||(p&(1u<<31))||(v&(1u<<31))||
                   !lapic_select(&apic,&selected)||selected!=0x40) break;
                if(!put(0x681e,rip+1)||!put(0x4826,1)||!put(0x4018,0)||
                   !put(0x401a,0)||!put(0x4016,0x80000000u|selected)) break;
                injected=true; /* IRR stays pending until successful reentry. */
                log_text("APIC.HALT_PENDING_INJECTION vector=0x40\n");continue;
#endif
#endif
            } else if(reason==18 && length==3 && accepted) {
                uintptr_t expected=EPT_CODE_GPA+(uintptr_t)ept_apic_ack-(uintptr_t)ept_payload_start;
                uint64_t isr,irr,a,f;
#ifdef VMX_PERIODIC_TEST
                uint64_t expected_count=periodic_acks+1;
#else
                uint64_t expected_count=1;
#endif
                if(rip!=expected||rsp!=EPT_STACK_TOP||vmx_guest_regs.rax!=0xac41||
                   vmx_guest_regs.r15!=expected_count||!lapic_read(&apic,0x812,&isr)||
                   !lapic_read(&apic,0x822,&irr)||isr||
#ifdef VMX_PERIODIC_TEST
                   irr>1||
#else
                   irr||
#endif
                   !get(0x4826,&a)||a||!get(0x6820,&f)||!(f&(1u<<9))) break;
#ifdef VMX_WINDOW_TEST
                if(window_armed||vmx_guest_regs.r14!=0x52) break;
#endif
#if defined(VMX_MOVSS_TEST) && !defined(VMX_COLLIDE_DELIVER_TEST)
                if(!ept_fixture_movss_ok(&ept)) break;
#endif
#ifdef VMX_COLLIDE_DELIVER_TEST
                if(!ud_handled||ud_pending||vmx_guest_regs.r13!=1) break;
#endif
#ifdef VMX_PERIODIC_TEST
                periodic_acks++;
                log_text("APIC.PERIODIC_IRQ_ACK_COUNT=");log_hex(periodic_acks);
                if(periodic_acks<3) {
                    if(!timer.armed||!put(0x681e,rip+length)) break;
                    injected=false;accepted=false;timer_halted=false;
                    continue;
                }
                if(!masked_expiries||timer_irqs!=3||timer_exits<3||timer.armed) break;
                if(real_halt_wakes!=3) {
                    log_text("APIC.PERIODIC_NOT_EXERCISED three_real_halt_expiries_required\n");break;
                }
#elif defined(VMX_TIMER_TEST)
                if(masked_expiries!=1||timer_irqs!=1||!timer_exits||!timer_halt_entered||timer.armed) break;
#endif
                success=ept_fixture_data_ok(&ept);break;
            } else break;
            if(!put(0x681e,rip+length)) break;
#if defined(VMX_WINDOW_TEST) && !defined(VMX_MOVSS_TEST)
            if(!injected) {
                uint64_t pending,activity;
                if(!get(0x4016,&pending)||!get(0x4826,&activity)) break;
                if(lapic_window_arm(&apic,window_armed,injected,pending,activity)) {
                    uint64_t f;
                    if(!get(0x6820,&f)||(f&(1u<<9))) break; /* this test arms under CLI */
                    if(!put(0x4002,primary|4)) break;
                    window_armed=true;
                    log_text("APIC.WINDOW_ARMED_IF0_PENDING_RETAINED\n");
                }
            }
#endif
            continue;
        }
#endif
#ifdef VMX_VECTORING_TEST
        if(attempt==7) {
            uint64_t q,gpa,info,error,flags,blocking,activity;
            uint32_t injection=0,entry_error=0;
            uintptr_t expected=EPT_CODE_GPA+(uintptr_t)ept_gp_fault-(uintptr_t)ept_payload_start;
            /* This exit is EPT, not a newly intercepted #GP. Recover the
             * interrupted event ONLY from IDT-vectoring fields. */
            #ifdef VMX_STACKPART_TEST
            uint64_t expected_rsp=0xd020,cs,ss;
#else
            uint64_t expected_rsp=EPT_STACK_TOP;
#endif
            if(vectoring_retried||rsp!=expected_rsp||
               !get(0x6400,&q)||!get(0x2400,&gpa)||!get(0x4408,&info)||
               (info&~UINT64_C(0x1000))!=UINT64_C(0x80000b0d)||
               !get(0x440a,&error)||!get(0x6820,&flags)||flags!=vectoring_flags||
               !get(0x4824,&blocking)||blocking||!get(0x4826,&activity)||activity) break;
#ifdef VMX_STACKPART_TEST
            if(reason!=48||(q&0x3f)!=10||gpa<0xcff0||gpa>0xcfff||
               error!=0x28||rip!=expected||!get(0x802,&cs)||cs!=8||!get(0x804,&ss)||ss!=16) break;
            int partial=ept_stackpart_observe(&ept,vectoring_flags);
            if(partial<=0) {
                log_text(partial==0?"VCPU.STACKPART_NOT_EXERCISED no_partial_write\n":
                                   "VCPU.STACKPART_INVALID_MEMORY_STATE\n");
                break;
            }
            log_text("VCPU.STACKPART_OBSERVED_UPPER_SLOTS=");log_hex(partial);
            injection=0x80000b0d;entry_error=0x28;
#else
            if(!ept_vectoring_gp_valid(reason,q,gpa,info,error,rip,expected,
                                      &injection,&entry_error)) break;
#endif
            log_text("VCPU.IDT_VECTORING_EPT_GP info=");log_hex(info);
            log_text("VCPU.IDT_VECTORING_ERROR=");log_hex(error);
            log_text("VCPU.IDT_VECTORING_GPA=");log_hex(gpa);
#ifdef VMX_STACKPART_TEST
            if(!ept_stackpart_protect(&ept,false)) break;
#else
            if(!ept_fixture_idt_access(&ept,true)) break;
#endif
            if(!put(0x4018,entry_error)||
               !put(0x401a,0)||!put(0x4016,injection)) break;
            vectoring_retried=true;
            log_text("VCPU.IDT_VECTORING_REINJECT_SAME_RIP error=0x28\n");
            continue;
        }
#endif
#ifdef VMX_EVENT_TEST
        if(step>=4) {
#ifdef VMX_STACKPART_TEST
            if(rsp!=(step>=6?0xd020:EPT_STACK_TOP)) break;
#else
            if(rsp!=EPT_STACK_TOP) break;
#endif
            #ifdef VMX_GP_TEST
            if(step==6) {
                uint64_t info,error,vectoring,pending,blocking,activity;
                uint32_t injection=0,entry_error=0;
                uintptr_t expected=EPT_CODE_GPA+(uintptr_t)ept_gp_fault-(uintptr_t)ept_payload_start;
                if(!get(0x4404,&info)||!get(0x4406,&error)||!get(0x4408,&vectoring)||
                   !get(0x4016,&pending)||!get(0x4824,&blocking)||!get(0x4826,&activity)||
                   !vcpu_inject_gp(&cpu,reason,info,error,vectoring,pending,blocking,activity,
                                  rip,expected,&injection,&entry_error)) break;
#ifdef VMX_VECTORING_TEST
                if(!get(0x6820,&vectoring_flags)) break;
#ifdef VMX_STACKPART_TEST
                if(ept_stackpart_observe(&ept,vectoring_flags)!=0 ||
                   !ept_stackpart_protect(&ept,true)) break;
                log_text("VCPU.STACKPART_FRESH_PAGES_LOWER_WRITE_REVOKED\n");
#else
                if(!ept_fixture_idt_access(&ept,false)) break;
                log_text("VCPU.IDT_ACCESS_REVOKED_BEFORE_GP_INJECTION\n");
#endif
#endif
                if(!put(0x4018,entry_error)||!put(0x401a,0)||!put(0x4016,injection)) break;
                log_text("VCPU.GP_VALIDATED_INJECT_SAME_RIP error=0x28\n");
                continue;
            }
#endif
            #ifdef VMX_PF_TEST
            if(step==6) {
                uint64_t info,error,gla,vectoring,pending,blocking,activity;
                uint32_t injection=0,entry_error=0;
                uintptr_t expected=EPT_CODE_GPA+(uintptr_t)ept_pf_fault-(uintptr_t)ept_payload_start;
                if(!get(0x6400,&gla)||!get(0x4404,&info)||!get(0x4406,&error)||!get(0x4408,&vectoring)||
                   !get(0x4016,&pending)||!get(0x4824,&blocking)||!get(0x4826,&activity)||
                   !vcpu_inject_pf(&cpu,reason,info,error,gla,vectoring,pending,blocking,activity,
                                  rip,expected,&injection,&entry_error)) break;
                vmx_guest_cr2=gla; /* intercepted #PF does not update CR2 automatically */
                if(!put(0x4018,entry_error)||!put(0x401a,0)||!put(0x4016,injection)) break;
                log_text("VCPU.PF_VALIDATED_INJECT_SAME_RIP error=0 cr2=0xc000\n");
                continue;
            }
#endif
            if(step==4) {
                uint64_t info,vectoring,pending,blocking,activity;
                uint32_t injection=0;
                uintptr_t expected=EPT_CODE_GPA+(uintptr_t)ept_ud_fault-(uintptr_t)ept_payload_start;
                if(!get(0x4404,&info)||!get(0x4408,&vectoring)||
                   !get(0x4016,&pending)||!get(0x4824,&blocking)||!get(0x4826,&activity)||
                   !vcpu_inject_ud(&cpu,reason,info,vectoring,pending,blocking,activity,
                                  rip,expected,&injection)) break;
                /* Never advance a fault RIP; entry length is not a #UD length.
                 * No error code is delivered for #UD. */
                if(!put(0x4018,0)||!put(0x401a,0)||!put(0x4016,injection)) break;
                log_text("VCPU.UD_VALIDATED_INJECT_SAME_RIP\n");
                continue;
            }
            /* Successful reentry and subsequent exit consumed the injected
             * event. Explicitly clear valid before any further VMRESUME. */
            if(!put(0x4016,0)||!put(0x4018,0)) break;
            const unsigned char *label=step==5?ept_ud_ack:ept_event_halt;
#ifdef VMX_PF_TEST
            if(step==7) label=ept_pf_ack;
#endif
#ifdef VMX_GP_TEST
            if(step==7) label=ept_gp_ack;
#endif
#ifdef VMX_WAKE_TEST
            if(step==7) label=ept_irq_ack;
            if(step==8) label=ept_final_halt;
#endif
            uintptr_t expected=EPT_CODE_GPA+(uintptr_t)label-
                (uintptr_t)ept_payload_start;
            if(rip!=expected || !get(0x440c,&length)) break;
            if(step==5) {
                if(reason!=18 || length!=3 || vmx_guest_regs.rax!=0xac06 ||
                   vmx_guest_regs.r12!=0xd006 || vmx_guest_regs.r13!=1 ||
                   !vcpu_ack_ud(&cpu) || !put(0x681e,rip+length)) break;
                log_text("VCPU.GUEST_UD_HANDLER_IRETQ_ACK\n");
                continue;
            }
            #ifdef VMX_GP_TEST
            if(step==7) {
                if(reason!=18 || length!=3 || vmx_guest_regs.rax!=0xac0d ||
                   vmx_guest_regs.r15!=1 || !vcpu_ack_gp(&cpu) ||
                   !put(0x681e,rip+length)) break;
#ifdef VMX_STACKPART_TEST
                uintptr_t returned=EPT_CODE_GPA+(uintptr_t)ept_gp_return-(uintptr_t)ept_payload_start;
                if(!ept_stackpart_complete(&ept,vectoring_flags,returned)) break;
                log_text("VCPU.STACKPART_FRAME_CANARIES_VERIFIED\n");
#endif
                log_text("VCPU.GP_HANDLER_IRETQ_ACK error_frame_removed count=1\n");
                continue;
            }
#endif
            #ifdef VMX_PF_TEST
            if(step==7) {
                if(reason!=18 || length!=3 || vmx_guest_regs.rax!=0xac0e ||
                   vmx_guest_regs.r15!=1 || vmx_guest_cr2!=0xc000 || !vcpu_ack_pf(&cpu) ||
                   !put(0x681e,rip+length)) break;
                #ifdef VMX_DEMAND_TEST
                if(!ept_fixture_demand_ok(&ept)) break;
                log_text("VCPU.DEMAND_RETRY_ACK read=0x42 fault_count=1 guest_pte=present_rw_supervisor\n");
#endif
                log_text("VCPU.PF_HANDLER_IRETQ_ACK error_frame_removed count=1 host_cr2_preserved\n");
                continue;
            }
#endif
#ifdef VMX_WAKE_TEST
            if(step==7) {
                uint64_t activity, flags;
                if(reason!=18 || length!=3 || vmx_guest_regs.rax!=0xac40 ||
                   vmx_guest_regs.r14!=1 || !get(0x4826,&activity) ||
                   !get(0x6820,&flags) || !(flags&(1u<<9)) ||
                   !vcpu_ack_irq(&cpu,activity) || !put(0x681e,rip+length)) break;
                log_text("VCPU.IRQ_HANDLER_IRETQ_ACK activity=active count=1\n");
                continue;
            }
            if(step==6) {
                uint64_t flags,blocking,activity,pending,vectoring;
                uint32_t injection=0;
                if(reason!=12 || length!=1 || vmx_guest_regs.rax!=0xc006 ||
                   !get(0x6820,&flags)||!get(0x4824,&blocking)||!get(0x4826,&activity)||
                   !get(0x4016,&pending)||!get(0x4408,&vectoring) || activity!=0 ||
                   !vcpu_halt(&cpu)) break;
                /* HLT exiting intercepts BEFORE execution. Emulate its
                 * next RIP/activity, then deliver a synthetic host event. */
                if(!put(0x681e,rip+1)||!put(0x4826,1)) break;
                if(!vcpu_inject_irq(&cpu,0x40,flags,blocking,1,pending,vectoring,&injection) ||
                   !put(0x4018,0)||!put(0x401a,0)||!put(0x4016,injection)) break;
                log_text("VCPU.HALTED_SYNTHETIC_IRQ_QUEUED vector=0x40\n");
                continue;
            }
            const uint64_t terminal_marker=0xc040;
#else
            const uint64_t terminal_marker=0xc006;
#endif
            if(reason!=12 || length!=1 || vmx_guest_regs.rax!=terminal_marker ||
               !ept_fixture_data_ok(&ept) || !vcpu_halt(&cpu)) break;
            log_text("VCPU.HALTED (no wakeup or reentry)\n");
            success=vcpu_complete(&cpu);
            if(success) log_text("VCPU.COMPLETED\n");
            break;
        }
#endif
#ifdef VMX_EXIT_TEST
        if(step>=4) {
            uintptr_t expected=EPT_CODE_GPA+(uintptr_t)exit_labels[step-4]-(uintptr_t)ept_payload_start;
            uint64_t next=0;
            if(rip!=expected || rsp!=EPT_STACK_TOP || !get(0x440c,&length) ||
               reason!=(step==8?12:10)) break;
            enum vmexit_action action=vmexit_dispatch(reason,rip,length,EPT_CODE_GPA,
                EPT_CODE_GPA+(uintptr_t)ept_payload_end-(uintptr_t)ept_payload_start,
                &vmx_guest_regs,&next);
            if(action==VMEXIT_REJECT) {log_text("VMEXIT.REJECTED\n");break;}
            if(!put(0x681e,next)) break;
            if(action==VMEXIT_HALT) {
                success=step==8 && vmx_guest_regs.rax==0xc0de && ept_fixture_data_ok(&ept);
                if(success) log_text("VMEXIT.HLT_NORMAL_END (no reentry)\n");
                break;
            }
            log_text("VMEXIT.CPUID_HANDLED\n");
            continue;
        }
#endif
        if(step==0 || step==4) {
            uintptr_t expected=EPT_CODE_GPA+(uintptr_t)(step?ept_done_call:ept_normal_call)-(uintptr_t)ept_payload_start;
            if(reason!=18 || rip!=expected || rsp!=EPT_STACK_TOP ||
               !get(0x440c,&length) || length!=3 || vmx_guest_regs.rax!=(step?0xe003:0xe000)) break;
            if(step==4) {
                success=ept_fixture_data_ok(&ept);
                log_text(success?"EPT.GUEST_NORMAL_END_BACKING_VERIFIED\n":"EPT.BACKING_MISMATCH\n");
                break;
            }
            if(!put(0x681e,rip+length)) break;
            log_text("EPT.NORMAL_RW_PASS\n");
        } else {
            uint64_t q,gpa;
            if(!get(0x6400,&q)||!get(0x2400,&gpa)) break;
            unsigned phase=step-1;
            uintptr_t expected=phase==2?0xb000:
                EPT_CODE_GPA+(uintptr_t)(phase?ept_write_fault:ept_read_fault)-(uintptr_t)ept_payload_start;
            log_text("EPT.QUALIFICATION=");log_hex(q);log_text("EPT.GPA=");log_hex(gpa);
            if(!ept_fault_matches(phase,reason,q,gpa,rip,expected) ||
               rsp!=(phase==2?EPT_STACK_TOP-8:EPT_STACK_TOP)) break;
            if(!ept_fixture_remediate(&ept,phase)) {
                log_text("EPT.INVEPT_FAILED\n");break;
            }
            log_text("EPT.VIOLATION_VALIDATED_RETRY_SAME_RIP\n");
        }
    }
#ifdef VMX_APIC_TEST
#ifdef VMX_PERIODIC_TEST
    log_text(success?"APIC.PERIODIC_DIAGNOSTIC_PASS irq_eoi=3 phase_preserved=1\n":"APIC.PERIODIC_DIAGNOSTIC_FAILED\n");
#elif defined(VMX_TIMER_TEST)
    log_text(success?"APIC.TIMER_DIAGNOSTIC_PASS masked=1 oneshot_irq=1 real_halt_expiry=1\n":"APIC.TIMER_DIAGNOSTIC_FAILED\n");
#endif
#ifdef VMX_COLLIDE_DELIVER_TEST
    log_text(success?"APIC.COLLISION_DELIVERY_PASS ud_first=1 irq_after_iret=1\n":"APIC.COLLISION_DELIVERY_FAILED\n");
#elif defined(VMX_COLLISION_TEST)
    log_text(success?"APIC.COLLISION_EXPECTED_REFUSAL_PASS\n":"APIC.COLLISION_DIAGNOSTIC_FAILED\n");
#else
#ifdef VMX_MOVSS_TEST
    log_text(success?"APIC.MOVSS_NATURAL_CLEAR_PASS store=0x55\n":"APIC.MOVSS_DIAGNOSTIC_FAILED\n");
#endif
#ifdef VMX_WINDOW_TEST
    log_text(success?"APIC.WINDOW_DIAGNOSTIC_PASS active_no_hlt=1\n":"APIC.WINDOW_DIAGNOSTIC_FAILED\n");
#endif
    log_text(success?"APIC.DIAGNOSTIC_PASS registers_pending_priority_eoi_wakeup=1\n":"APIC.DIAGNOSTIC_FAILED\n");
#endif
#elif defined(VMX_EVENT_TEST)
    #ifdef VMX_VECTORING_TEST
    success=success && vectoring_retried;
#ifdef VMX_STACKPART_TEST
    log_text(success?"VCPU.STACKPART_DIAGNOSTIC_PASS observed_partial_replay=1\n":
                     "VCPU.STACKPART_DIAGNOSTIC_INCOMPLETE\n");
#endif
    log_text(success?"VCPU.VECTORING_DIAGNOSTIC_PASS gp_reinjected_once=1\n":"VCPU.VECTORING_DIAGNOSTIC_FAILED\n");
#endif
    if(!success) vcpu_fault(&cpu);
#ifdef VMX_DEMAND_TEST
    log_text(success?"VCPU.DEMAND_DIAGNOSTIC_PASS retry_original_load=1\n":"VCPU.DEMAND_DIAGNOSTIC_FAILED\n");
#endif
#ifdef VMX_PF_TEST
    log_text(success?"VCPU.PF_DIAGNOSTIC_PASS error=0 cr2=0xc000 handled=1\n":"VCPU.PF_DIAGNOSTIC_FAILED\n");
#endif
#ifdef VMX_GP_TEST
    log_text(success?"VCPU.GP_DIAGNOSTIC_PASS error=0x28 handled=1\n":"VCPU.GP_DIAGNOSTIC_FAILED\n");
#endif
#ifdef VMX_WAKE_TEST
    log_text(success?"VCPU.WAKE_DIAGNOSTIC_PASS synthetic_irq=1 wake_ack=1\n":"VCPU.WAKE_DIAGNOSTIC_FAILED\n");
#endif
    log_text(success?"VCPU.EVENT_DIAGNOSTIC_PASS ud_injected=1 ud_handled=1 hlt=1\n":"VCPU.FAULTED EVENT_DIAGNOSTIC_FAILED\n");
#elif defined(VMX_EXIT_TEST)
    log_text(success?"VMEXIT.DIAGNOSTIC_PASS cpuid=4 hlt=1 ept_violations=3\n":"VMEXIT.DIAGNOSTIC_FAILED\n");
#else
    log_text(success?"EPT.DIAGNOSTIC_PASS violations=3\n":"EPT.DIAGNOSTIC_FAILED\n");
#endif
#else
    vmx_guest_regs = (struct vmx_guest_registers){0};
    const uintptr_t calls[3] = {(uintptr_t)vmx_guest_vmcall0,
                               (uintptr_t)vmx_guest_vmcall1,
                               (uintptr_t)vmx_guest_vmcall2};
    for (unsigned phase = 0; phase < 3; ++phase) {
        int result = vmx_guest_enter(phase != 0);
        /* VM exit resets table limits. Restore before C logging/dispatch. */
        __asm__ volatile("lgdt %0; lidt %1"::"m"(gdtr),"m"(idtr):"memory");
        if (result != 1) {
            uint64_t error = 0;
            log_text(phase ? "VMX.VMRESUME_FAILED\n" : "VMX.VMLAUNCH_FAILED\n");
            if (get(0x4400, &error)) { log_text("VMX.INSTRUCTION_ERROR=");log_hex(error); }
            break;
        }
        uint64_t reason, rip, length, rsp, rflags;
        if (!get(0x4402,&reason) || !get(0x681e,&rip) || !get(0x440c,&length) ||
            !get(0x681c,&rsp) || !get(0x6820,&rflags)) {
            log_text("VMX.EXIT_READ_FAILED\n");
            break;
        }
        log_text("VMX.EXIT_PHASE="); log_hex(phase);
        log_text("VMX.EXIT_REASON="); log_hex(reason);
        log_text("VMX.GUEST_RIP="); log_hex(rip);
        enum vmx_dispatch_action action = vmx_guest_dispatch(phase, reason, rip,
            calls[phase], length, rsp, (uintptr_t)(guest_stack+sizeof guest_stack),
            rflags, &vmx_guest_regs);
        if (action == VMX_DISPATCH_REJECT) {
            log_text("VMX.EXIT_REJECTED\n");
            break;
        }
        log_text("VMX.GUEST_STATE_PASS\n");
        if (action == VMX_DISPATCH_DONE) {
            success = true;
            log_text("VMX.GUEST_NORMAL_END\n");
            break;
        }
        /* Only an accepted VMCALL is consumed; unexpected exits never advance. */
        if (!put(0x681e, rip + length)) {
            log_text("VMX.RIP_ADVANCE_FAILED\n");
            break;
        }
        log_text("VMX.RESUME_REQUEST\n");
    }
    if (success) {
        log_text("VMX.GUEST_VMCALL_PASS\n");
        log_text("VMX.GUEST_RESUME_LOOP_PASS exits=3 resumes=2\n");
    } else log_text("VMX.GUEST_FAILED (not a successful guest run)\n");
    goto clear;
#endif
    goto clear;
setup_failed:
    log_text("VMX.VMCS_SETUP_FAILED\n");
clear:
    __asm__ volatile("vmclear %1; setna %0":"=qm"(fail):"m"(physical):"cc","memory");
    if (fail) panic("VMCS CLEAR FAILED; VMX STATE RETAINED");
#ifdef VMX_NESTED_VCPU_TEST
    __asm__ volatile("cpuid":"=a"(ca),"=b"(cb),"=c"(cc),"=d"(cd):"a"(1),"c"(0));
    if(vcpu->owner_apic!=(cb>>24)||(!vcpu->cleared01&&!nested_vcpu_clear(vcpu,cb>>24,physical)))panic("CTX CLEAR01 POLICY FAILED");
#endif
#ifdef VMX_EPT_TEST
    if(ept.eptp && !ept_fixture_flush(&ept)) panic("EPT INVALIDATION FAILED; PAGES RETAINED");
#endif
    return success;
}
