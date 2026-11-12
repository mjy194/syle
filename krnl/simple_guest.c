/* Opt-in, trusted integer-only ELF prototype. Not a hostile-code sandbox. */
#include "simple_guest.h"
#include "guest_uart.h"
#include "paging.h"
#include "memory.h"
#include "vmx_memory.h"
#include "ept.h"
#include "../efi/src/exceptions.h"
struct table { uint16_t limit; uint64_t base; } __attribute__((packed));
struct segment { uint16_t selector; uint32_t limit, ar; uint64_t base; };
struct vmx_guest_registers vmx_guest_regs;
uint64_t vmx_host_cr2,vmx_guest_cr2;
extern int vmx_guest_enter(unsigned resume);
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
static const void *source;
static size_t source_bytes;
static struct sg_memory memory;
static bool bound,started,cleared,success;
static uint64_t allocated_before;
static uint64_t allocate(void *c) {(void)c;return pmm_alloc_page();}
static bool release(void *c,uint64_t p) {(void)c;return pmm_free_page(p)!=0;}
static void *map(void *c,uint64_t p) {(void)c;return paging_phys_to_virt(p);}
static bool writeback(void *c,uint64_t p) {(void)c;return paging_page_rw_wb(p)&&vmx_memory_region_wb(p);}
static const struct sg_memory_ops ops={0,allocate,release,map,writeback};
bool simple_guest_bind(const void *p,size_t n) {
    struct sg_image image;
    if(bound || started || !sg_image_inspect(p,n,&image)) return false;
    source=p;source_bytes=n;bound=true;return true;
}
bool simple_guest_succeeded(void) {return success;}
static bool invalidate(void) {
    struct {uint64_t eptp,reserved;} d __attribute__((aligned(16)))={memory.eptp,0};
    unsigned char failed;
    __asm__ volatile("invept %1,%2; setna %0":"=qm"(failed):"m"(d),"r"(UINT64_C(1)):"cc","memory");
    return !failed;
}
/* vmx.c calls this ONLY after successful VMXOFF, before freeing its regions. */
void ept_guest_release(void) {
    if(!started) return;
    if(!cleared) panic("SIMPLE_GUEST ACTIVE OWNERSHIP RETAINED");
    if(!sg_memory_release(&memory,&ops)) panic("SIMPLE_GUEST FREE FAILED");
    if(pmm_get_stats().allocated_pages!=allocated_before) panic("SIMPLE_GUEST PMM ACCOUNTING FAILED");
    started=false;
}
#define W(f,v) do {if(!put((f),(v))) goto clear;} while(0)
bool vmx_guest_run(uint64_t physical,uint64_t basic) {
    uint64_t flags,cr0,cr3,cr4,dr7,dr6;
    unsigned char fail;
    if(!bound || started) return false;
    success=false;
    __asm__ volatile("pushfq;pop %0":"=r"(flags));
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    __asm__ volatile("mov %%dr7,%0":"=r"(dr7));
    __asm__ volatile("mov %%dr6,%0":"=r"(dr6));
    /* Reject unimplemented host translation/security/debug restoration modes. */
    if((flags&(1u<<9)) || (cr4&((1u<<12)|(1u<<17)|(1u<<23))) ||
       dr7!=0x400 || msr(0x1d9)!=0 || paging_ap_start_attempted()) return false;
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000000u),"c"(0));
    if(a<0x80000001u) return false;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000001u),"c"(0));
    if(!(d&(1u<<20))) return false; /* guest page tables use NX */
    const uint64_t guest_cr0=UINT64_C(0x8001003f),guest_cr4=(1u<<5)|(1u<<13);
    if((guest_cr0&msr(0x486))!=msr(0x486) || (guest_cr0&~msr(0x487)) ||
       (guest_cr4&msr(0x488))!=msr(0x488) || (guest_cr4&~msr(0x489))) return false;
    /* IA32_VMX_MISC[4:0] specifies the preemption-timer/TSC rate.
     * Cap the rate before rounding so one entry cannot exceed the fixed
     * 2^28 TSC-tick budget. This is not a wall-clock duration promise. */
    unsigned timer_shift=(unsigned)(msr(0x485)&31);
    if(timer_shift>20) {
        log_text("SIMPLE_GUEST BLOCKED TIMER_RATE\n");return false;
    }
    uint64_t timer_quantum=UINT64_C(1)<<timer_shift;
    uint32_t entry_ticks=(uint32_t)(((uint64_t)SG_ENTRY_TICKS+timer_quantum-1)/timer_quantum);
    if(!entry_ticks || (uint64_t)entry_ticks*timer_quantum>SG_ENTRY_TICKS) {
        log_text("SIMPLE_GUEST BLOCKED TIMER_BUDGET\n");return false;
    }
    bool true_ctls=(basic>>55)&1;
    uint32_t pin,primary,secondary,entry,exit;
    uint32_t primary_want=(1u<<7)|(1u<<9)|(1u<<10)|(1u<<11)|(1u<<12)|
        (1u<<15)|(1u<<16)|(1u<<19)|(1u<<20)|(1u<<23)|(1u<<24)|
        (1u<<29)|(1u<<30)|(1u<<31);
    uint32_t entry_want=(1u<<2)|(1u<<9)|(1u<<14)|(1u<<15);
    uint32_t exit_want=(1u<<2)|(1u<<9)|(15u<<18);
    if(!(msr(true_ctls?0x48e:0x482)&(UINT64_C(1)<<63)) ||
       !vmx_guest_controls(msr(0x48b),2,2,&secondary) || !ept_caps_supported(msr(0x48c)) ||
       !vmx_guest_controls(msr(true_ctls?0x48d:0x481),0x49,0x5f,&pin) ||
       !vmx_guest_controls(msr(true_ctls?0x48e:0x482),primary_want,primary_want|0x0401e172u,&primary) ||
       !vmx_guest_controls(msr(true_ctls?0x48f:0x483),exit_want,exit_want|0x36dffu,&exit) ||
       !vmx_guest_controls(msr(true_ctls?0x490:0x484),entry_want,entry_want|0x11ffu,&entry)) {
        log_text("SIMPLE_GUEST BLOCKED CONTROLS\n");return false;
    }
    /* No MSR or I/O bitmaps, TPR shadow, or secondary features besides EPT. */
    if(primary&((1u<<21)|(1u<<25)|(1u<<28))) return false;
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
    for(unsigned i=0;i<8;i++) if((sel[i]&7)||!segment(&gdtr,sel[i],&seg[i])) return false;
    if(sel[6] || !sel[1] || !sel[7]) return false;
    seg[4].base=msr(0xc0000100);seg[5].base=msr(0xc0000101);
    volatile uint32_t *region=paging_phys_to_virt(physical);
    if(!region) return false;
    for(unsigned i=0;i<1024;i++) region[i]=0;
    region[0]=(uint32_t)basic&0x7fffffff;
    __asm__ volatile("vmclear %1; setna %0":"=qm"(fail):"m"(physical):"cc","memory");
    if(fail) panic("SIMPLE_GUEST INITIAL VMCLEAR FAILED; STATE RETAINED");
    __asm__ volatile("vmptrld %1; setna %0":"=qm"(fail):"m"(physical):"cc","memory");
    if(fail) return false;
    allocated_before=pmm_get_stats().allocated_pages;started=true;cleared=false;
    if(!sg_memory_prepare(&memory,&ops,source,source_bytes)) goto clear;
    W(0x401e,secondary);W(0x201a,memory.eptp);
    W(0x4000,pin);W(0x4002,primary);W(0x4004,UINT32_MAX);
    W(0x4006,0);W(0x4008,0);W(0x400a,0);
    W(0x400c,exit);W(0x400e,0);W(0x4010,0);
    W(0x4012,entry);W(0x4014,0);W(0x4016,0);W(0x4018,0);W(0x401a,0);
    W(0x2010,0);
    W(0x6000,UINT64_MAX);W(0x6002,UINT64_MAX);W(0x6004,guest_cr0);W(0x6006,guest_cr4);
    W(0x2800,UINT64_MAX);W(0x2802,0);
    W(0x2804,msr(0x277));W(0x2806,0xd00);
    W(0x2c00,msr(0x277));W(0x2c02,msr(0xc0000080));
    for(unsigned i=0;i<8;i++) {
        uint64_t selector=0,ar=1u<<16,base=0,limit=0;
        if(i==1) {selector=8;ar=0xa09b;limit=UINT32_MAX;}
        if(i==0||i==2||i==3) {selector=16;ar=0xc093;limit=UINT32_MAX;}
        if(i==7) {selector=24;ar=0x8b;base=SG_TSS;limit=103;}
        W(0x800+2*i,selector);W(0x4800+2*i,limit);W(0x4814+2*i,ar);W(0x6806+2*i,base);
    }
    W(0x4810,39);W(0x4812,0);W(0x6816,SG_GDT);W(0x6818,SG_IDT);
    W(0x4824,0);W(0x4826,0);W(0x6822,0);
    W(0x6800,guest_cr0);W(0x6802,SG_CR3);W(0x6804,guest_cr4);W(0x681a,0x400);
    W(0x681c,SG_STACK_TOP);W(0x681e,memory.image.entry);W(0x6820,2);
    W(0x482a,0);W(0x6824,0);W(0x6826,0);
    for(unsigned i=0;i<6;i++) W(0xc00+2*i,sel[i]);
    W(0xc0c,sel[7]);W(0x6c00,cr0);W(0x6c02,cr3);W(0x6c04,cr4);
    W(0x6c06,seg[4].base);W(0x6c08,seg[5].base);W(0x6c0a,seg[7].base);
    W(0x6c0c,gdtr.base);W(0x6c0e,idtr.base);
    W(0x4c00,(uint32_t)msr(0x174));W(0x6c10,msr(0x175));W(0x6c12,msr(0x176));
    vmx_guest_regs=(struct vmx_guest_registers){.rdi=SG_BOOT_INFO};vmx_guest_cr2=0;
    if(!invalidate()) panic("SIMPLE_GUEST PREENTRY INVEPT FAILED; PAGES RETAINED");
    struct sg_run_state state={0};
    struct guest_uart uart;guest_uart_init(&uart);
    log_text("SIMPLE_GUEST BEGIN trusted_integer_only bounded_entries\n");
    for(unsigned attempt=0;attempt<SG_EXIT_LIMIT;attempt++) {
        /* Reload the rate-adjusted hardware count on every entry. */
        W(0x482e,entry_ticks);
        int result=vmx_guest_enter(attempt!=0);
        __asm__ volatile("mov %0,%%dr6"::"r"(dr6):"memory");
        __asm__ volatile("lgdt %0;lidt %1"::"m"(gdtr),"m"(idtr):"memory");
        if(result!=1) break;
        uint64_t reason,rip,length,next=0,vectoring;
        uint8_t character=0;
        if(!get(0x4402,&reason)||!get(0x681e,&rip)||!get(0x440c,&length)||
           !get(0x4408,&vectoring)||(vectoring&(1u<<31))) break;
        enum sg_action action;
        if(reason==30) {
            uint64_t qualification;
            if(!get(0x6400,&qualification)) break;
            action=sg_dispatch_io(&state,&memory.image,&uart,qualification,rip,length,
                                  &vmx_guest_regs,&next,&character);
        } else {
            action=sg_dispatch(&state,&memory.image,reason,rip,length,
                               &vmx_guest_regs,&next,&character);
        }
        if(action==SG_REJECT) {log_text("SIMPLE_GUEST REJECT reason=");log_hex(reason);break;}
        if(action==SG_FINISH) {success=!state.failed&&state.exit_code==0;break;}
        W(0x681e,next);
        if(action==SG_PUTCHAR) {char text[2]={(char)character,0};log_text(text);}
    }
clear:
    __asm__ volatile("vmclear %1; setna %0":"=qm"(fail):"m"(physical):"cc","memory");
    if(fail) panic("SIMPLE_GUEST VMCLEAR FAILED; PAGES RETAINED");
    if(memory.eptp && !invalidate()) panic("SIMPLE_GUEST FINAL INVEPT FAILED; PAGES RETAINED");
    cleared=true;
    log_text(success?"SIMPLE_GUEST COMPLETED status=0\n":"SIMPLE_GUEST FAILED\n");
    return success;
}
