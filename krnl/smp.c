#include "smp.h"
#include "smp_acpi.h"
#include "smp_cpu.h"
#ifdef SMP_NESTED_TEST
#include "nested_parallel.h"
#include "vmx_memory.h"
#endif
#ifdef SMP_VMX_TEST
#include "smp_vmx.h"
#include "memory.h"
static struct smp_vmx_context bsp_vmx,ap_vmx;
#ifdef SMP_GUEST_TEST
#include "smp_guest.h"
#include "smp_ept.h"
static struct smp_guest_context bsp_guest,ap_guest;
static struct smp_ept shared_ept;
extern const uint32_t smp_guest_payload_call_offset;
#endif
#endif
#include "paging.h"
#include "../efi/src/exceptions.h"
#include <stddef.h>
#ifdef SMP_MP_TYPE_TEST
#include "cache_mp_transition.h"
#include "cache_ept_alias.h"
#include "cache_page.h"
static struct cache_mp_transition mp_transition;
static struct cache_ept_alias_plan mp_alias;
static struct cache_page_lease mp_host_lease;
static uint64_t ap_mp_epoch;
static enum cache_mp_phase ap_mp_phase;
#endif
#ifdef SMP_MAINTENANCE_TEST
#include "cache_rendezvous.h"
#include "cache_maintenance.h"
static struct cache_rendezvous maintenance;
static struct cache_cpu_snapshot bsp_cache,ap_cache;
static uint64_t ap_cache_epoch;
static enum cache_rendezvous_phase ap_cache_phase;
#endif
#ifdef SMP_CONTROLLER_TEST
#include "shootdown.h"
static struct shootdown controller;
enum remote_op { OP_ON=1,OP_WARM,OP_FLUSH,OP_RESUME,OP_CLEAR,OP_OFF,OP_PARK,OP_MAINTENANCE,OP_MP_TRANSITION };
#endif
struct mailbox {
    _Alignas(64) uint32_t online,request,ack,parked;
    uint32_t expected_id,actual_id;
    uint32_t irq_count,stop;
    uint64_t lapic;
    uint64_t argument,result;
#ifdef SMP_CONTROLLER_TEST
    uint64_t generation,completed_generation;
#ifdef SMP_MAINTENANCE_TEST
    uint64_t completed_phase;
#endif
    uint32_t operation;
#ifdef SMP_KICK_TEST
    uint32_t work,done;
#endif
#endif
    struct smp_cpu_fault fault;
};
#if defined(SMP_MP_TYPE_TEST)
#define STOP_SEQUENCE 24
#define OFF_SEQUENCE 23
#elif defined(SMP_MAINTENANCE_TEST)
#define STOP_SEQUENCE 10
#define OFF_SEQUENCE 9
#elif defined(SMP_KICK_TEST)
#define STOP_SEQUENCE 7
#define OFF_SEQUENCE 6
#elif defined(SMP_CONTROLLER_TEST)
#define STOP_SEQUENCE 11
#define OFF_SEQUENCE 10
#elif defined(SMP_SHOOTDOWN_TEST)
#define STOP_SEQUENCE 6
#define OFF_SEQUENCE 5
#else
#define STOP_SEQUENCE 4
#define OFF_SEQUENCE 3
#endif
static struct mailbox box;
static _Alignas(16) unsigned char ap_stack[32768];
static uint32_t cpu_id(void) {
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    return b>>24;
}
static uint64_t msr(unsigned n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return ((uint64_t)d<<32)|a;}
static uint8_t in8(uint16_t p){uint8_t x;__asm__ volatile("inb %1,%0":"=a"(x):"Nd"(p));return x;}
static void out8(uint16_t p,uint8_t x){__asm__ volatile("outb %0,%1"::"a"(x),"Nd"(p));}
/* PIT channel2 one-shot, 1.193182 MHz. No guessed host TSC frequency.
 * Bounded poll count makes a broken timer a diagnostic failure. */
static bool delay(unsigned ticks) {
    uint8_t old=in8(0x61);out8(0x61,old&~3u);
    out8(0x43,0xb0);out8(0x42,ticks&255);out8(0x42,ticks>>8);
    out8(0x61,(old&~2u)|1u);
    bool done=false;
    for(unsigned i=0;i<10000000;i++) {
        if(in8(0x61)&0x20){done=true;break;}
        __asm__ volatile("pause");
    }
    out8(0x61,old);return done;
}
static bool wait_value(uint32_t *p,uint32_t value) {
    for(unsigned n=0;n<2000;n++) {
        if(__atomic_load_n(p,__ATOMIC_ACQUIRE)==value)return true;
        if(__atomic_load_n(&box.fault.valid,__ATOMIC_ACQUIRE)){log_text("SMP.AP_FAULT_VECTOR=");log_hex(box.fault.vector);log_text("SMP.AP_FAULT_ERROR=");log_hex(box.fault.error);
#ifdef SMP_GUEST_TEST
            log_text("SMP.AP_GUEST_ENTRY_RESULT=");log_hex(ap_guest.enter_result);
            log_text("SMP.AP_GUEST_VM_INSTRUCTION_ERROR=");log_hex(ap_guest.instruction_error);
            log_text("SMP.AP_GUEST_EXIT_REASON=");log_hex(ap_guest.exit_reason);
            log_text("SMP.AP_GUEST_EXIT_RIP=");log_hex(ap_guest.exit_rip);
#endif
            return false;}
        if(!delay(1193))return false;
    }
    return false;
}
static bool read_physical(void *ctx,uint64_t pa,void *dst,size_t n) {
    const struct syle_boot_info *b=ctx;
    if(!n||pa>UINT64_MAX-n)return false;
    for(uint64_t off=0;off<b->memory_map_bytes;off+=b->descriptor_size) {
        const unsigned char *d=(void *)(uintptr_t)(b->memory_map+off);
        uint32_t type;uint64_t base,pages,attr;
        __builtin_memcpy(&type,d,4);__builtin_memcpy(&base,d+8,8);
        __builtin_memcpy(&pages,d+24,8);__builtin_memcpy(&attr,d+32,8);
        if(!((type>=1&&type<=7)||type==9||type==10||type==14)||!(attr&9)||
           (attr&0x2000)||pages>UINT64_MAX/4096||base>UINT64_MAX-pages*4096)continue;
        if(pa>=base&&pa+n<=base+pages*4096) {
            const unsigned char *s=paging_phys_to_virt(pa);
            if(!s||!paging_phys_to_virt(pa+n-1))return false;
            unsigned char *to=dst;for(size_t i=0;i<n;i++)to[i]=s[i];return true;
        }
    }
    return false;
}
static bool low_owned(const struct syle_boot_info *b) {
    uint64_t p=b->ap_trampoline.base;
    if(!p||p>=0x100000||(p&4095)||b->ap_trampoline.size!=4096)return false;
    for(uint64_t off=0;off<b->memory_map_bytes;off+=b->descriptor_size) {
        const unsigned char *d=(void *)(uintptr_t)(b->memory_map+off);
        uint32_t type;uint64_t base,pages,attr;
        __builtin_memcpy(&type,d,4);__builtin_memcpy(&base,d+8,8);
        __builtin_memcpy(&pages,d+24,8);__builtin_memcpy(&attr,d+32,8);
        if(type==2&&(attr&8)&&!(attr&0x21000)&&pages<=UINT64_MAX/4096&&
           base<=UINT64_MAX-pages*4096&&p>=base&&p+4096<=base+pages*4096)return true;
    }
    return false;
}
#ifdef SMP_IPI_TEST
static void ap_ipi(void *context) {
    struct mailbox *m=context;
    uint32_t seq=__atomic_load_n(&m->request,__ATOMIC_ACQUIRE);
    volatile uint32_t *lapic=(void *)(uintptr_t)m->lapic;
    if(seq!=m->irq_count+1||seq>STOP_SEQUENCE||cpu_id()!=m->expected_id||!(lapic[0x120/4]&1)) {
        m->fault.vector=0x40;m->fault.error=seq;
        if(lapic[0x120/4]&1)lapic[0xb0/4]=0;
        __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
        for(;;)__asm__ volatile("cli;hlt");
    }
#ifdef SMP_CONTROLLER_TEST
#ifdef SMP_KICK_TEST
    if(__atomic_load_n(&m->work,__ATOMIC_ACQUIRE)!=__atomic_load_n(&m->done,__ATOMIC_ACQUIRE)) {
        m->fault.vector=0x40;m->fault.error=UINT64_MAX;
        lapic[0xb0/4]=0;__atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
        for(;;)__asm__ volatile("cli;hlt");
    }
    __atomic_store_n(&m->work,seq,__ATOMIC_RELEASE);
#else
    bool ok=false;
    switch(m->operation) {
    case OP_ON:ok=smp_vmx_local_enter(&ap_vmx)==SMP_VMX_OK;break;
    case OP_WARM:ok=smp_guest_local_warm(&ap_guest);break;
    case OP_FLUSH:ok=smp_guest_local_invalidate(&ap_guest);break;
    case OP_RESUME:ok=smp_guest_local_resume(&ap_guest,m->argument);break;
    case OP_CLEAR:ok=smp_guest_local_clear(&ap_guest);break;
    case OP_OFF:ok=smp_vmx_local_leave(&ap_vmx)==SMP_VMX_OK;break;
    case OP_PARK:ok=!ap_vmx.root_live&&!ap_guest.vmcs_live;if(ok)m->stop=1;break;
    default:break;
    }
    if(!ok){m->fault.vector=0x40;m->fault.error=m->operation;
        lapic[0xb0/4]=0;__atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
        for(;;)__asm__ volatile("cli;hlt");}
    m->completed_generation=m->generation;
#endif
#else
#ifdef SMP_VMX_TEST
    enum smp_vmx_status status;
    if(seq==1)status=smp_vmx_local_enter(&ap_vmx);
    else if(seq==2)status=smp_vmx_local_check(&ap_vmx);
    else if(seq==OFF_SEQUENCE)status=smp_vmx_local_leave(&ap_vmx);
    else status=ap_vmx.status;
    if(status!=SMP_VMX_OK) {
        m->fault.vector=0x40;m->fault.error=status;
        lapic[0xb0/4]=0;
        __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
        for(;;)__asm__ volatile("cli;hlt");
    }
#endif
#ifdef SMP_GUEST_TEST
#ifdef SMP_SHOOTDOWN_TEST
    bool guest_ok=true;
    if(seq==2)guest_ok=smp_guest_local_warm(&ap_guest);
    if(seq==3)guest_ok=smp_guest_local_invalidate(&ap_guest);
    if(seq==4)guest_ok=smp_guest_local_resume(&ap_guest,UINT64_C(0x8877665544332211))&&smp_guest_local_clear(&ap_guest);
    if(!guest_ok) {
#else
    if(seq==2&&!smp_guest_local_run(&ap_guest)) {
#endif
        m->fault.vector=0x40;m->fault.error=ap_guest.status;
        lapic[0xb0/4]=0;
        __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
        for(;;)__asm__ volatile("cli;hlt");
    }
#endif
#endif
    m->result=m->argument^UINT64_C(0x53594c4541500000)^m->actual_id;
    m->irq_count=seq;
#ifndef SMP_CONTROLLER_TEST
    if(seq==STOP_SEQUENCE)m->stop=1;
#endif
    lapic[0xb0/4]=0; /* complete work and local EOI before release ACK */
    if(lapic[0x120/4]&1) {
        m->fault.vector=0x40;m->fault.error=UINT64_MAX;
        __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
        for(;;)__asm__ volatile("cli;hlt");
    }
    __atomic_store_n(&m->ack,seq,__ATOMIC_RELEASE);
}
static bool ap_ipi_init(struct mailbox *m) {
    uint64_t base=msr(0x1b);
    if((base&UINT64_C(0xfffff000))!=m->lapic||!(base&(1u<<11))||(base&(1u<<10)))return false;
    volatile uint32_t *a=(void *)(uintptr_t)m->lapic;
    unsigned max=(a[0x30/4]>>16)&255;
    if(max<5||max>6)return false;
    a[0x320/4]=a[0x320/4]|(1u<<16); /* timer */
    a[0x330/4]=a[0x330/4]|(1u<<16); /* thermal */
    a[0x340/4]=a[0x340/4]|(1u<<16); /* performance */
    a[0x350/4]=a[0x350/4]|(1u<<16);a[0x360/4]=a[0x360/4]|(1u<<16);a[0x370/4]=a[0x370/4]|(1u<<16);
    if(max>=6)a[0x2f0/4]=a[0x2f0/4]|(1u<<16);
    a[0x80/4]=0; /* TPR accepts vector40 */
    a[0xf0/4]=(a[0xf0/4]&~UINT32_C(0x1ff))|0x1ff; /* software enable, spurious vectorff */
    smp_cpu_set_ipi(ap_ipi,m);
    return true;
}
#endif
#ifdef SMP_MAINTENANCE_TEST
static bool ap_maintenance(struct mailbox *m){
    enum cache_rendezvous_phase phase=(enum cache_rendezvous_phase)m->argument;
    if(phase==CACHE_RENDEZVOUS_PRECHECK){
        if(ap_cache_epoch||!cache_maintenance_precheck(shared_ept.page[7],&ap_cache))return false;
        ap_cache_epoch=m->generation;
    }else {
        if(ap_cache_epoch!=m->generation)return false;
        if(phase==CACHE_RENDEZVOUS_TLB){
            if(ap_cache_phase!=CACHE_RENDEZVOUS_PRECHECK||!cache_maintenance_tlb(&ap_cache))return false;
        }else if(phase==CACHE_RENDEZVOUS_CACHE){
            if(ap_cache_phase!=CACHE_RENDEZVOUS_TLB||!cache_maintenance_cache(&ap_cache))return false;
        }else return false;
    }
    ap_cache_phase=phase;m->completed_phase=phase;return true;
}
#endif
#ifdef SMP_MP_TYPE_TEST
static bool ap_mp_work(struct mailbox *m){
    enum cache_mp_phase p=(enum cache_mp_phase)m->argument;
    if(p==CACHE_MP_DETACH_EPT){
        if((ap_mp_epoch&&ap_mp_phase!=CACHE_MP_PUBLISH_EPT)||m->generation!=ap_mp_epoch+1+(ap_mp_epoch==0))return false;
    }else if(m->generation!=ap_mp_epoch||p!=ap_mp_phase+1)return false;
    bool ok=false;
    if(p==CACHE_MP_DETACH_EPT||p==CACHE_MP_PUBLISH_EPT)ok=smp_guest_local_invalidate(&ap_guest);
    else if(p==CACHE_MP_BREAK_TLB||p==CACHE_MP_MAKE_TLB)ok=cache_maintenance_tlb(&ap_cache);
    else if(p==CACHE_MP_BREAK_CACHE||p==CACHE_MP_MAKE_CACHE)ok=cache_maintenance_cache(&ap_cache);
    if(!ok)return false;
    ap_mp_epoch=m->generation;ap_mp_phase=p;m->completed_phase=p;return true;
}
#endif
#ifdef SMP_KICK_TEST
static void ap_work(struct mailbox *m,uint32_t work) {
    bool ok=false;
    switch(m->operation) {
    case OP_ON:ok=smp_vmx_local_enter(&ap_vmx)==SMP_VMX_OK;break;
    case OP_WARM:ok=smp_guest_local_warm(&ap_guest);break;
    case OP_FLUSH:ok=smp_guest_local_invalidate(&ap_guest);break;
    case OP_RESUME:ok=smp_guest_local_resume(&ap_guest,m->argument);break;
    case OP_CLEAR:ok=smp_guest_local_clear(&ap_guest);break;
    case OP_OFF:ok=smp_vmx_local_leave(&ap_vmx)==SMP_VMX_OK;break;
    case OP_PARK:ok=!ap_vmx.root_live&&!ap_guest.vmcs_live;if(ok)m->stop=1;break;
#ifdef SMP_MAINTENANCE_TEST
    case OP_MAINTENANCE:ok=ap_maintenance(m);break;
#endif
#ifdef SMP_MP_TYPE_TEST
    case OP_MP_TRANSITION:ok=ap_mp_work(m);break;
#endif
    default:break;
    }
    if(!ok){m->fault.vector=0x40;m->fault.error=m->operation;
        __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
        for(;;)__asm__ volatile("cli;hlt");}
    if(m->operation==OP_WARM) {
        volatile uint32_t *lapic=(void *)(uintptr_t)m->lapic;
        if(!ap_guest.kicked||!(lapic[0x120/4]&2)) {
            m->fault.vector=0x41;m->fault.error=ap_guest.exit_info;
            __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
            for(;;)__asm__ volatile("cli;hlt");
        }
        lapic[0xb0/4]=0;
        if(lapic[0x120/4]&2){m->fault.vector=0x41;
            __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);for(;;)__asm__ volatile("cli;hlt");}
    }
    m->completed_generation=m->generation;
    __atomic_store_n(&m->done,work,__ATOMIC_RELEASE);
}
#endif
#ifdef SMP_NESTED_TEST
#include "smp_nested.inc"
#endif
static __attribute__((noreturn)) void ap_main(void *context) {
    struct mailbox *m=context;
    smp_cpu_init(&m->fault);
    uint64_t pat=msr(0x277),cr0;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    if((pat&255)!=6||((pat>>24)&255)!=0||(cr0&UINT64_C(0x60000000))) {
        m->fault.vector=UINT64_MAX;m->fault.error=pat;
        __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
        for(;;)__asm__ volatile("cli;hlt");
    }
    m->actual_id=cpu_id();
#ifdef SMP_IPI_TEST
    if(!ap_ipi_init(m)) {
        m->fault.vector=UINT64_MAX-1;
        __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
        for(;;)__asm__ volatile("cli;hlt");
    }
#endif
    __atomic_store_n(&m->online,1,__ATOMIC_RELEASE);
#ifdef SMP_NESTED_TEST
    smp_nested_ap(m);
#endif
#ifdef SMP_IPI_TEST
    for(;;) {
        __asm__ volatile("cli":::"memory");
        if(m->stop) {
            __atomic_store_n(&m->parked,1,__ATOMIC_RELEASE);
            for(;;)__asm__ volatile("cli;hlt");
        }
#ifdef SMP_KICK_TEST
        uint32_t work=__atomic_load_n(&m->work,__ATOMIC_ACQUIRE);
        if(work!=__atomic_load_n(&m->done,__ATOMIC_ACQUIRE)){ap_work(m,work);continue;}
#endif
        /* STI interrupt shadow covers HLT; a pending IPI cannot arrive in
         * a check-to-sleep gap and leave this AP sleeping indefinitely. */
        __asm__ volatile("sti;hlt":::"memory");
    }
#else
    uint32_t last=0;
    for(;;) {
        uint32_t seq=__atomic_load_n(&m->request,__ATOMIC_ACQUIRE);
        if(seq==last){__asm__ volatile("pause");continue;}
        if(seq==UINT32_MAX) {
            __atomic_store_n(&m->parked,1,__ATOMIC_RELEASE);
            for(;;)__asm__ volatile("cli;hlt");
        }
        m->result=m->argument^UINT64_C(0x53594c4541500000)^m->actual_id;
        last=seq;__atomic_store_n(&m->ack,seq,__ATOMIC_RELEASE);
    }
#endif
}
static bool icr(volatile uint32_t *lapic,uint32_t target,uint32_t command) {
    unsigned i=0;for(;i<10000000&&(lapic[0x300/4]&(1u<<12));i++)__asm__ volatile("pause");
    if(i==10000000)return false;
    lapic[0x310/4]=target<<24;lapic[0x300/4]=command;
    for(i=0;i<10000000&&(lapic[0x300/4]&(1u<<12));i++)__asm__ volatile("pause");
    return i<10000000;
}
#ifdef SMP_CONTROLLER_TEST
static void controller_require(bool ok) {
    if(!ok){
#ifdef SMP_MAINTENANCE_TEST
        cache_rendezvous_fail(&maintenance);
#endif
#ifdef SMP_MP_TYPE_TEST
        cache_mp_transition_fail(&mp_transition);
#endif
        shootdown_fail(&controller);panic("SMP CONTROLLER FAILED ENTRY_CLOSED RESOURCES_RETAINED");}
}
static void remote_command(volatile uint32_t *lapic,uint32_t ap,enum remote_op op,uint64_t gen,uint64_t value) {
    uint32_t seq=box.irq_count+1;
    box.operation=op;box.generation=gen;box.argument=value;
    __atomic_store_n(&box.request,seq,__ATOMIC_RELEASE);
    controller_require(icr(lapic,ap,0x40)&&wait_value(&box.ack,seq));
#ifdef SMP_KICK_TEST
    if(op==OP_WARM)return; /* wait only after BSP has observed heartbeat and sent kick */
    controller_require(wait_value(&box.done,seq));
#endif
    controller_require(box.completed_generation==gen);
}
#ifdef SMP_MAINTENANCE_TEST
static void maintenance_run(volatile uint32_t *lapic,uint32_t ap){
    uint64_t epoch;
    controller_require(cache_rendezvous_init(&maintenance,3));
    controller_require(cache_rendezvous_begin(&maintenance,1,controller.running==0,shared_ept.page[7]!=0,&epoch));
    controller_require(!cache_rendezvous_entry_allowed(&maintenance,epoch));
    controller_require(cache_maintenance_precheck(shared_ept.page[7],&bsp_cache));
    remote_command(lapic,ap,OP_MAINTENANCE,epoch,CACHE_RENDEZVOUS_PRECHECK);
    controller_require(box.completed_phase==CACHE_RENDEZVOUS_PRECHECK&&cache_maintenance_same(&bsp_cache,&ap_cache));
    controller_require(cache_rendezvous_ack(&maintenance,0,epoch,CACHE_RENDEZVOUS_PRECHECK)&&cache_rendezvous_ack(&maintenance,1,epoch,CACHE_RENDEZVOUS_PRECHECK));
    log_text("SMP.CACHE_PREFLIGHT_CPUS=2 shared_CR3=");log_hex(bsp_cache.cr3);log_text("SMP.CACHE_PAT_MATCH=");log_hex(bsp_cache.pat);
    controller_require(cache_rendezvous_advance(&maintenance,epoch));
    controller_require(cache_maintenance_tlb(&bsp_cache));
    remote_command(lapic,ap,OP_MAINTENANCE,epoch,CACHE_RENDEZVOUS_TLB);
    controller_require(box.completed_phase==CACHE_RENDEZVOUS_TLB&&cache_rendezvous_ack(&maintenance,0,epoch,CACHE_RENDEZVOUS_TLB)&&cache_rendezvous_ack(&maintenance,1,epoch,CACHE_RENDEZVOUS_TLB));
    log_text("SMP.CACHE_TLB_ACK_CPUS=2 epoch=");log_hex(epoch);
    controller_require(cache_rendezvous_advance(&maintenance,epoch)&&!cache_rendezvous_entry_allowed(&maintenance,epoch));
    controller_require(cache_maintenance_cache(&bsp_cache));
    remote_command(lapic,ap,OP_MAINTENANCE,epoch,CACHE_RENDEZVOUS_CACHE);
    controller_require(box.completed_phase==CACHE_RENDEZVOUS_CACHE&&cache_rendezvous_ack(&maintenance,0,epoch,CACHE_RENDEZVOUS_CACHE)&&cache_rendezvous_ack(&maintenance,1,epoch,CACHE_RENDEZVOUS_CACHE));
    log_text("SMP.CACHE_WBINVD_ACK_CPUS=2 epoch=");log_hex(epoch);
    controller_require(cache_rendezvous_advance(&maintenance,epoch)&&!cache_rendezvous_entry_allowed(&maintenance,epoch));
    controller_require(cache_rendezvous_finish(&maintenance,epoch)&&cache_rendezvous_entry_allowed(&maintenance,epoch));
}
#endif
#ifdef SMP_MP_TYPE_TEST
#include "smp_cache_transition.inc"
#endif
static void controller_run(volatile uint32_t *lapic,uint32_t ap) {
#ifdef SMP_KICK_TEST
    controller_require(shootdown_init(&controller,3));
    remote_command(lapic,ap,OP_ON,1,0);
    controller_require(shootdown_enter(&controller,0,1));
    controller_require(smp_guest_local_warm(&bsp_guest));
    controller_require(shootdown_leave(&controller,0));
    controller_require(shootdown_enter(&controller,1,1));
    remote_command(lapic,ap,OP_WARM,1,0);
    bool heartbeat=false;uint64_t old=0;
    for(unsigned i=0;i<2000;i++) {
        if(smp_ept_heartbeat(&shared_ept,&old)){heartbeat=true;break;}
        controller_require(!__atomic_load_n(&box.fault.valid,__ATOMIC_ACQUIRE)&&delay(1193));
    }
    controller_require(heartbeat&&old==UINT64_C(0x1122334455667788));
    log_text("SMP.RUNNING_HEARTBEAT guest_IF0=1\n");
    controller_require(icr(lapic,ap,0x41));
    controller_require(wait_value(&box.done,box.irq_count));
    controller_require(box.completed_generation==1&&ap_guest.kicked);
    controller_require(shootdown_leave(&controller,1));
    log_text("SMP.KICK_QUIESCED reason=");log_hex(ap_guest.exit_reason);
    log_text("SMP.KICK_VECTOR_INFO=");log_hex(ap_guest.exit_info);
    uint64_t gen=0;
    controller_require(shootdown_begin(&controller,1,&gen));
#ifdef SMP_MAINTENANCE_TEST
    maintenance_run(lapic,ap); /* initial preflight/maintenance stays WB */
#ifdef SMP_MP_TYPE_TEST
    mp_transition_run(lapic,ap,true);
#endif
    controller_require(shootdown_published(&controller));
#else
    controller_require(smp_ept_select_data(&shared_ept,1)&&shootdown_published(&controller));
#endif
    controller_require(smp_guest_local_invalidate(&bsp_guest)&&shootdown_ack(&controller,0,gen));
    remote_command(lapic,ap,OP_FLUSH,gen,0);
    controller_require(shootdown_ack(&controller,1,gen));
    controller_require(smp_ept_stop(&shared_ept)); /* only after both completed flush ACKs */
    controller_require(shootdown_enter(&controller,1,gen));
#ifdef SMP_MAINTENANCE_TEST
    controller_require(cache_rendezvous_entry_allowed(&maintenance,maintenance.generation));
    const uint64_t resumed_value=UINT64_C(0x1122334455667788);
#else
    const uint64_t resumed_value=UINT64_C(0x8877665544332211);
#endif
    remote_command(lapic,ap,OP_RESUME,gen,resumed_value);
    controller_require(shootdown_leave(&controller,1));
    controller_require(shootdown_enter(&controller,0,gen));
    controller_require(smp_guest_local_resume(&bsp_guest,resumed_value));
    controller_require(shootdown_leave(&controller,0));
#ifdef SMP_MP_TYPE_TEST
    log_text("SMP.MP_UC_AP_READ=");log_hex(ap_guest.frame.rax);
    log_text("SMP.MP_UC_BSP_READ=");log_hex(bsp_guest.frame.rax);
#elif defined(SMP_MAINTENANCE_TEST)
    log_text("SMP.CACHE_UNCHANGED_AP_READ=");log_hex(ap_guest.frame.rax);
    log_text("SMP.CACHE_UNCHANGED_BSP_READ=");log_hex(bsp_guest.frame.rax);
#else
    log_text("SMP.KICK_NEW_AP_READ=");log_hex(ap_guest.frame.rax);
    log_text("SMP.KICK_NEW_BSP_READ=");log_hex(bsp_guest.frame.rax);
#endif
#ifdef SMP_MP_TYPE_TEST
    controller_require(shootdown_begin(&controller,gen,&gen));
    mp_transition_run(lapic,ap,false);
    controller_require(shootdown_published(&controller));
    controller_require(smp_guest_local_invalidate(&bsp_guest)&&shootdown_ack(&controller,0,gen));
    remote_command(lapic,ap,OP_FLUSH,gen,0);controller_require(shootdown_ack(&controller,1,gen));
    controller_require(cache_mp_transition_entry_allowed(&mp_transition,mp_transition.generation));
    controller_require(shootdown_enter(&controller,1,gen));
    remote_command(lapic,ap,OP_RESUME,gen,resumed_value);controller_require(shootdown_leave(&controller,1));
    controller_require(shootdown_enter(&controller,0,gen));
    controller_require(smp_guest_local_resume(&bsp_guest,resumed_value)&&shootdown_leave(&controller,0));
    log_text("SMP.MP_BOTH_WB_READ_AFTER_RESTORE=");log_hex(bsp_guest.frame.rax);
#endif
    remote_command(lapic,ap,OP_CLEAR,gen,0);controller_require(smp_guest_local_clear(&bsp_guest));
    remote_command(lapic,ap,OP_OFF,gen,0);controller_require(smp_vmx_local_leave(&bsp_vmx)==SMP_VMX_OK);
    remote_command(lapic,ap,OP_PARK,gen,0);
#else
    controller_require(shootdown_init(&controller,3));
    remote_command(lapic,ap,OP_ON,1,0);
    controller_require(shootdown_enter(&controller,1,1));
    remote_command(lapic,ap,OP_WARM,1,0);
    controller_require(shootdown_leave(&controller,1));
    controller_require(shootdown_enter(&controller,0,1));
    controller_require(smp_guest_local_warm(&bsp_guest));
    controller_require(shootdown_leave(&controller,0));
    log_text("SMP.CONTROLLER_BOTH_WARM actual_VMCS_retained=1\n");
    for(unsigned round=0;round<3;round++) {
        uint64_t gen=0;
        controller_require(shootdown_begin(&controller,controller.generation,&gen));
        unsigned index=(round&1)?0:1;
        uint64_t value=index?UINT64_C(0x8877665544332211):UINT64_C(0x1122334455667788);
        controller_require(smp_ept_select_data(&shared_ept,index));
        controller_require(shootdown_published(&controller));
        controller_require(smp_guest_local_invalidate(&bsp_guest));
        controller_require(shootdown_ack(&controller,0,gen));
        remote_command(lapic,ap,OP_FLUSH,gen,0);
        controller_require(shootdown_ack(&controller,1,gen));
        controller_require(shootdown_enter(&controller,1,gen));
        remote_command(lapic,ap,OP_RESUME,gen,value);
        controller_require(shootdown_leave(&controller,1));
        controller_require(shootdown_enter(&controller,0,gen));
        controller_require(smp_guest_local_resume(&bsp_guest,value));
        controller_require(shootdown_leave(&controller,0));
        log_text("SMP.CONTROLLER_COMPLETE_GENERATION=");log_hex(gen);
        log_text("SMP.CONTROLLER_BSP_READ=");log_hex(bsp_guest.frame.rax);
        log_text("SMP.CONTROLLER_AP_READ=");log_hex(ap_guest.frame.rax);
    }
    remote_command(lapic,ap,OP_CLEAR,controller.generation,0);
    controller_require(smp_guest_local_clear(&bsp_guest));
    remote_command(lapic,ap,OP_OFF,controller.generation,0);
    controller_require(smp_vmx_local_leave(&bsp_vmx)==SMP_VMX_OK);
    remote_command(lapic,ap,OP_PARK,controller.generation,0);
#endif
}
#endif
void smp_probe(const struct syle_boot_info *boot) {
#ifdef SMP_VMX_TEST
    uint32_t va,vb,vc,vd;
    __asm__ volatile("cpuid":"=a"(va),"=b"(vb),"=c"(vc),"=d"(vd):"a"(1),"c"(0));
    if(!(vc&(1u<<5))) {log_text("SMP.N20_BLOCKED CPUID_VMX0 no_VMX_MSR_or_instruction\n");return;}
#endif
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
    if(!(vd&(1u<<16))||!(vd&(1u<<12))){
        log_text("SMP.N37_BLOCKED PAT_OR_MTRR_UNAVAILABLE no_allocation no_AP no_transition\n");return;
    }
    unsigned wt_slot;uint64_t wt_bits;
    if(!cache_page_pat_slot(msr(0x277),4,&wt_slot,&wt_bits)){
        log_text("SMP.N37_BLOCKED NO_EXISTING_WT_PAT_SLOT no_allocation no_AP no_transition\n");return;
    }
#endif
    uint64_t apic=msr(0x1b);
    if(!(apic&(1u<<11))||(apic&(1u<<10))||!(apic&(1u<<8)))panic("SMP BLOCKED XAPIC BSP REQUIRED");
    uint32_t bsp=cpu_id();struct smp_acpi_topology top;
    if(!low_owned(boot)||!smp_acpi_discover(boot->acpi_rsdp,bsp,read_physical,(void *)boot,&top))panic("SMP BLOCKED BOOT/ACPI CONTRACT");
    if(top.lapic_base!=(apic&UINT64_C(0xfffff000)))panic("SMP LAPIC ADDRESS MISMATCH");
#ifdef SMP_VMX_TEST
    bsp_vmx.region_hpa=pmm_alloc_page_below(UINT64_C(0x100000000));
    ap_vmx.region_hpa=pmm_alloc_page_below(UINT64_C(0x100000000));
    if(!bsp_vmx.region_hpa||!ap_vmx.region_hpa||bsp_vmx.region_hpa==ap_vmx.region_hpa)panic("SMP VMX REGIONS UNAVAILABLE");
    bsp_vmx.region_view=paging_phys_to_virt(bsp_vmx.region_hpa);
    ap_vmx.region_view=paging_phys_to_virt(ap_vmx.region_hpa);
#ifdef SMP_NESTED_TEST
    if(!smp_nested_prepare(bsp,top.ap_id))panic("SMP NESTED PREPARE FAILED RETAINED");
#endif
#ifdef SMP_GUEST_TEST
    if(!
#ifdef SMP_KICK_TEST
       smp_ept_prepare_running(&shared_ept)
#else
       smp_ept_prepare(&shared_ept)
#endif
       )panic("SMP SHARED EPT PREPARE FAILED");
    bsp_guest.vmcs_hpa=pmm_alloc_page_below(UINT64_C(0x100000000));
    ap_guest.vmcs_hpa=pmm_alloc_page_below(UINT64_C(0x100000000));
    if(!bsp_guest.vmcs_hpa||!ap_guest.vmcs_hpa||bsp_guest.vmcs_hpa==ap_guest.vmcs_hpa)panic("SMP VMCS ALLOC FAILED");
    bsp_guest.vmcs_view=paging_phys_to_virt(bsp_guest.vmcs_hpa);
    ap_guest.vmcs_view=paging_phys_to_virt(ap_guest.vmcs_hpa);
    bsp_guest.owner_apic_id=bsp;ap_guest.owner_apic_id=top.ap_id;
    bsp_guest.eptp=ap_guest.eptp=shared_ept.eptp;
    bsp_guest.stack_top=0xa000;ap_guest.stack_top=0xb000;
    bsp_guest.expected_data=ap_guest.expected_data=UINT64_C(0x1122334455667788);
    bsp_guest.owner_token=UINT64_C(0xb500);ap_guest.owner_token=UINT64_C(0xa500);
#ifdef SMP_KICK_TEST
    extern const uint32_t smp_guest_running_payload_call_offset,smp_guest_running_payload_loop_begin_offset,smp_guest_running_payload_loop_end_offset;
    bsp_guest.vmcall_gpa=ap_guest.vmcall_gpa=0x6000+smp_guest_running_payload_call_offset;
    ap_guest.kick_mode=true;
#ifdef SMP_MP_TYPE_TEST
    bsp_guest.cache_guard=ap_guest.cache_guard=true;
#endif
    ap_guest.loop_start=0x6000+smp_guest_running_payload_loop_begin_offset;
    ap_guest.loop_end=0x6000+smp_guest_running_payload_loop_end_offset;
#else
    bsp_guest.vmcall_gpa=ap_guest.vmcall_gpa=0x6000+smp_guest_payload_call_offset;
#endif
#endif
#endif
    box.expected_id=top.ap_id;box.lapic=top.lapic_base;
    void *low=paging_phys_to_virt(boot->ap_trampoline.base);
    if(!smp_bootstrap_prepare(low,boot->ap_trampoline.base,paging_root(),
        (uintptr_t)(ap_stack+sizeof ap_stack),ap_main,&box)||
       !paging_smp_prepare(boot->ap_trampoline.base,top.lapic_base))panic("SMP MAPPING/PREPARE FAILED");
    volatile uint32_t *lapic=(void *)(uintptr_t)top.lapic_base;
    if((lapic[0x20/4]>>24)!=bsp)panic("SMP BSP ID MISMATCH");
    log_text("SMP.BSP_ID=");log_hex(bsp);log_text("SMP.TARGET_AP_ID=");log_hex(top.ap_id);
    uint32_t sipi=0x600|(boot->ap_trampoline.base>>12);
    /* From this point AP may be live, including after timeout. Never free
     * trampoline/stack/descriptor/mailbox pages or continue other work. */
    paging_note_ap_start(); /* retain even if startup later fails */
    if(!icr(lapic,top.ap_id,0xc500)||!delay(11932)||
       !icr(lapic,top.ap_id,0x8500)||!delay(239)||
       !icr(lapic,top.ap_id,sipi)||!delay(239)||
       !icr(lapic,top.ap_id,sipi)||!wait_value(&box.online,1))panic("SMP AP START FAILED RESOURCES RETAINED");
    if(box.actual_id!=top.ap_id||box.actual_id==bsp)panic("SMP WRONG AP ID");
    log_text("SMP.AP_ONLINE_ID=");log_hex(box.actual_id);
#ifdef SMP_VMX_TEST
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
    if(!typed_bsp_preflight())return;
#endif
    enum smp_vmx_status enter_status=smp_vmx_local_enter(&bsp_vmx);
    if(enter_status!=SMP_VMX_OK){log_text("SMP.BSP_VMX_STATUS=");log_hex(enter_status);panic("SMP BSP VMX ENTER FAILED RETAINED");}
    log_text("SMP.VMX_BSP_ROOT_ENTERED_ID=");log_hex(bsp_vmx.owner_apic_id);
#ifdef SMP_NESTED_TEST
#ifdef VMX_NESTED_ATOMIC_UC_TEST
    if(!atomic_uc_bsp_provision())panic("SMP N46B PROVISION FAILED RETAIN");
#endif
    smp_nested_run();
    return;
#endif
#endif
#ifdef SMP_CONTROLLER_TEST
    controller_run(lapic,top.ap_id);
#else
    for(uint32_t seq=1;seq<STOP_SEQUENCE;seq++) {
        box.argument=UINT64_C(0x1020304050600000)+seq;
        __atomic_store_n(&box.request,seq,__ATOMIC_RELEASE);
#ifdef SMP_IPI_TEST
        if(!icr(lapic,top.ap_id,0x40))panic("SMP IPI SEND FAILED RETAINED");
#endif
        if(!wait_value(&box.ack,seq)||box.result!=(box.argument^UINT64_C(0x53594c4541500000)^top.ap_id))panic("SMP MAILBOX FAILED RETAINED");
        log_text("SMP.MAILBOX_ACK=");log_hex(seq);
#ifdef SMP_VMX_TEST
        if(seq==1) {
            if(!bsp_vmx.root_live||!ap_vmx.root_live||bsp_vmx.owner_apic_id==ap_vmx.owner_apic_id)panic("SMP TWO ROOT OWNERS FAILED");
            log_text("SMP.VMX_AP_ROOT_ENTERED_ID=");log_hex(ap_vmx.owner_apic_id);
        }
        if(seq==2) {
            if(smp_vmx_local_check(&bsp_vmx)!=SMP_VMX_OK||ap_vmx.last_vmcs!=UINT64_MAX)panic("SMP ROOT CHECK FAILED RETAINED");
            log_text("SMP.VMX_BOTH_VMPTRST_NONE_PREWARM\n");
#ifdef SMP_GUEST_TEST
#ifdef SMP_SHOOTDOWN_TEST
            if(!smp_guest_local_warm(&bsp_guest))
#else
            if(!smp_guest_local_run(&bsp_guest))
#endif
{log_text("SMP.BSP_GUEST_STATUS=");log_hex(bsp_guest.status);panic("SMP BSP GUEST FAILED RETAINED");}
            if(smp_ept_stack_word(&shared_ept,0)!=bsp_guest.owner_token||smp_ept_stack_word(&shared_ept,1)!=ap_guest.owner_token)panic("SMP PRIVATE STACK TOKEN FAILED");
            log_text("SMP.GUEST_BSP_READ=");log_hex(bsp_guest.expected_data);
            log_text("SMP.GUEST_AP_READ=");log_hex(ap_guest.expected_data);
            log_text("SMP.GUEST_SHARED_EPTP=");log_hex(shared_ept.eptp);
#ifdef SMP_SHOOTDOWN_TEST
            if(!bsp_guest.vmcs_live||!ap_guest.vmcs_live)panic("SMP WARM VMCS NOT RETAINED");
            if(!smp_ept_replace_data(&shared_ept)||!smp_guest_local_invalidate(&bsp_guest))panic("SMP LOCAL SHOOTDOWN FAILED RETAINED");
            log_text("SMP.SHOOTDOWN_LOCAL_COMPLETE old_backing_retained=1\n");
#endif
#endif
        }
#ifdef SMP_SHOOTDOWN_TEST
        if(seq==3)log_text("SMP.SHOOTDOWN_REMOTE_ACK local_INVEPT_completed=1\n");
        if(seq==4){
            if(!smp_guest_local_resume(&bsp_guest,UINT64_C(0x8877665544332211))||!smp_guest_local_clear(&bsp_guest))panic("SMP RESUME NEW MAPPING FAILED RETAINED");
            log_text("SMP.NEW_BSP_READ=");log_hex(bsp_guest.frame.rax);
            log_text("SMP.NEW_AP_READ=");log_hex(ap_guest.frame.rax);
        }
#endif
        if(seq==OFF_SEQUENCE) {
            if(ap_vmx.root_live||smp_vmx_local_leave(&bsp_vmx)!=SMP_VMX_OK)panic("SMP LOCAL VMXOFF FAILED RETAINED");
            log_text("SMP.VMX_BOTH_LOCAL_OFF_CR_RESTORED\n");
        }
#endif
    }
#ifdef SMP_IPI_TEST
    __atomic_store_n(&box.request,STOP_SEQUENCE,__ATOMIC_RELEASE);
    if(!icr(lapic,top.ap_id,0x40)||!wait_value(&box.ack,STOP_SEQUENCE))panic("SMP STOP IPI FAILED RETAINED");
#else
    __atomic_store_n(&box.request,UINT32_MAX,__ATOMIC_RELEASE);
#endif
#endif
    if(!wait_value(&box.parked,1))panic("SMP PARK FAILED RETAINED");
#ifdef SMP_IPI_TEST
    if(box.irq_count!=STOP_SEQUENCE)panic("SMP IPI COUNT FAILED");
#ifdef SMP_VMX_TEST
    if(bsp_vmx.root_live||ap_vmx.root_live)panic("SMP ROOT STILL LIVE");
    if(!pmm_free_page(bsp_vmx.region_hpa)||!pmm_free_page(ap_vmx.region_hpa))panic("SMP REGION FREE FAILED");
#ifdef SMP_GUEST_TEST
    if(bsp_guest.vmcs_live||ap_guest.vmcs_live)panic("SMP VMCS STILL LIVE");
    if(!pmm_free_page(bsp_guest.vmcs_hpa)||!pmm_free_page(ap_guest.vmcs_hpa))panic("SMP VMCS FREE FAILED");
    smp_ept_release(&shared_ept);
#ifdef SMP_CONTROLLER_TEST
#ifdef SMP_MP_TYPE_TEST
    log_text("SMP.N31B_PASS WB_UC_WB=1 actual_guest_CPUs=2 all_phase_ACK=2 restored_before_free=1\n");
#elif defined(SMP_MAINTENANCE_TEST)
    log_text("SMP.N31A_PASS actual_kick=1 maintenance_CPUs=2 unchanged_WB=1 phase_ACK=1 type_transition=0\n");
#elif defined(SMP_KICK_TEST)
    log_text("SMP.N24_PASS running_guest_kick=1 actual_reason1=1 quiesce_before_publish=1 flush_CPUs=2\n");
#else
    log_text("SMP.N23_PASS generations=3 participants=2 gated_resumes=6 retained_VMCS=1\n");
#endif
#elif defined(SMP_SHOOTDOWN_TEST)
    log_text("SMP.N22_PASS warmed_CPUs=2 actual_INVEPT_CPUs=2 resumed_CPUs=2 retained_VMCS=1\n");
#else
    log_text("SMP.N21_PASS actual_guest_CPUs=2 shared_EPT_read=2 private_stacks=2 host_restore=2\n");
#endif
#else
    log_text("SMP.N20_PASS distinct_CPU_VMXON=2 local_VMPTRST=2 local_VMXOFF=2 no_VMCS=1\n");
#endif
#else
    log_text("SMP.N19_PASS actual_fixed_IPI=4 EOI_before_ACK=1 HLT_idle=1 parked=1\n");
#endif
#else
    log_text("SMP.N18_PASS actual_AP=1 mailbox=3 parked=1 resources_retained=1\n");
#endif
}
