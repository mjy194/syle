#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
extern unsigned char nested_source_replace_call0[],nested_source_replace_call1[],nested_source_replace_call2[],nested_source_replace_call3[];
extern unsigned char nested_source_replace_call4[],nested_source_replace_call5[],nested_source_replace_call6[],nested_source_replace_call7[];
#endif
#ifdef VMX_NESTED_ATOMIC_MULTIROUND_TEST
extern unsigned char nested_atomic_multiround_call0[];
extern unsigned char nested_atomic_multiround_call1[];
extern unsigned char nested_atomic_multiround_call2[];
extern unsigned char nested_atomic_multiround_call3[];
extern unsigned char nested_atomic_multiround_call4[];
extern unsigned char nested_atomic_multiround_call5[];
extern unsigned char nested_atomic_multiround_call6[];
extern unsigned char nested_atomic_multiround_call7[];
extern unsigned char nested_atomic_multiround_call8[];
extern unsigned char nested_atomic_multiround_call9[];
extern unsigned char nested_atomic_multiround_call10[];
extern unsigned char nested_atomic_multiround_call11[];
extern unsigned char nested_atomic_multiround_call12[];
#endif
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
#include "nested_owner_atomic.h"
#ifdef VMX_NESTED_ATOMIC_RETRACK_TEST
extern unsigned char nested_atomic_retrack_call0[],nested_atomic_retrack_call1[],nested_atomic_retrack_call2[],nested_atomic_retrack_call3[];
extern unsigned char nested_atomic_retrack_call4[],nested_atomic_retrack_call5[],nested_atomic_retrack_call6[],nested_atomic_retrack_call7[];
#else
extern unsigned char nested_atomic_catalog_call0[],nested_atomic_catalog_call1[],nested_atomic_catalog_call2[],nested_atomic_catalog_call3[];
extern unsigned char nested_atomic_catalog_call4[],nested_atomic_catalog_call5[],nested_atomic_catalog_call6[],nested_atomic_catalog_call7[];
#endif
#endif
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
#include "nested_owner_dynamic_typed.h"
static bool catalog_root_allowed(void *context,uint64_t key){return nested_owner_catalog_key(context,key);}
#endif
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
extern unsigned char nested_typed_dynamic_call0[],nested_typed_dynamic_call1[],nested_typed_dynamic_call2[],nested_typed_dynamic_call3[],nested_typed_dynamic_call4[],nested_typed_dynamic_call5[],nested_typed_dynamic_call6[],nested_typed_dynamic_call7[],nested_typed_dynamic_call8[];
#endif
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
extern unsigned char nested_typed_context_call0[],nested_typed_context_call1[],nested_typed_context_call2[],nested_typed_context_call3[],nested_typed_context_call4[],nested_typed_context_call5[];
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
extern unsigned char nested_typed_warm_call[],nested_typed_uc_call[],nested_typed_wb_call[],nested_typed_wb_entry[];
#endif
#ifdef VMX_NESTED_OWNER_AD_RESET_TEST
extern unsigned char nested_ad_reset_call[];
#endif
#if defined(VMX_NESTED_LOCAL_INVEPT_TEST) || defined(VMX_NESTED_OWNER_CONTEXT_TEST)
#include "nested_local_invept.h"
extern unsigned char nested_owner_context_call[];
extern unsigned char nested_local_second_call[];
#endif
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
#include "nested_kick.h"
extern unsigned char nested_domain_kick_begin[],nested_domain_kick_loop_end[];
#endif
#ifdef VMX_NESTED_VCPU_TEST
#include "nested_vcpu.h"
#include "nested_host_state.h"
extern unsigned nested_vcpu_enter(struct nested_vcpu_frame *,unsigned);
#endif
#ifdef VMX_NESTED_UC_TEST
#include "cache_registered.h"
extern const unsigned char nested_uc_warm_call[],nested_uc_done_call[];
#ifdef VMX_NESTED_UC_CONTEXT_TEST
extern const unsigned char nested_uc_context_call[];
#endif
#endif
#ifdef VMX_NESTED_MEMORY_TEST
#include "nested_memory_type.h"
extern const unsigned char nested_memory_call[];
#endif
#ifdef VMX_NESTED_EPT_AD_CONTEXT_TEST
#include "nested_ept_ad_context_hw.h"
#ifdef VMX_NESTED_EPT_AD_MODE_TEST
extern const unsigned char nested_ad_mode_call[];
#else
extern const unsigned char nested_ad_context_call[];
#endif
#endif
#ifdef VMX_NESTED_EPT_AD_FAIL_TEST
extern const unsigned char nested_ad_fail_read[],nested_ad_fail_read_done[],nested_ad_fail_write[],nested_ad_fail_done[];
#endif
#ifdef VMX_NESTED_EPT_AD_TEST
#include "nested_ept_ad_hw.h"
extern const unsigned char nested_ad_read_call[],nested_ad_write_call[],nested_ad_reread_call[],nested_ad_rewrite_call[];
#endif
#ifdef VMX_NESTED_EPT_MISCONFIG_TEST
extern const unsigned char nested_misconfig_fault0[],nested_misconfig_fault1[],nested_misconfig_fault2[],nested_misconfig_done_call[];
#endif
#ifdef VMX_NESTED_EPT_LARGE_TEST
extern const unsigned char nested_large_warm_call[],nested_large_write_fault[],nested_large_read_fault[],nested_large_done_call[];
#endif
#ifdef VMX_NESTED_EPT_DYNAMIC_TEST
extern const unsigned char nested_dynamic_call[];
#endif
#ifdef VMX_NESTED_EPT_CONTEXT_TEST
#include "nested_ept_contexts.h"
extern const unsigned char nested_context_call[];
#endif
#ifdef VMX_NESTED_EVENT_TEST
#include "nested_events.h"
#ifdef VMX_NESTED_GP_INJECT_TEST
extern const unsigned char nested_gp_fault[];
#ifdef VMX_NESTED_PF_INJECT_TEST
#ifndef VMX_NESTED_VCPU_TEST
extern uint64_t vmx_host_cr2,vmx_guest_cr2;
#endif
#define L0_EVENT_BITMAP 0x4040
#else
#define L0_EVENT_BITMAP 0x2040
#endif
#else
#define L0_EVENT_BITMAP 0x40
#endif
extern const unsigned char nested_event_first_call[],nested_event_finish[];
#endif
#ifdef VMX_NESTED_EPT_TEST
#ifdef VMX_NESTED_EPT_SPARSE_TEST
#include "nested_ept_sparse_hw.h"
#define nested_ept_hw nested_ept_sparse_hw
#define nested_ept_hw_destroy nested_ept_sparse_hw_destroy
extern const unsigned char nested_sparse_warm_call[],nested_sparse_write_fault[],nested_sparse_read_fault[],nested_sparse_done_call[];
#elif defined(VMX_NESTED_EPT_MAP_TEST)
#include "nested_ept_map_hw.h"
#define nested_ept_hw nested_ept_map_hw
#define nested_ept_hw_init nested_ept_map_hw_init
#define nested_ept_hw_rebuild nested_ept_map_hw_rebuild
#define nested_ept_hw_destroy nested_ept_map_hw_destroy
extern const unsigned char nested_map_warm_call[],nested_map_write_fault[],nested_map_after_write[],nested_map_read_fault[],nested_map_done_call[];
#else
#include "nested_ept_hw.h"
#endif
#endif
#include "nested_guest.h"
#include "nested_profile.h"
#include "nested_operand.h"
#include "vmx_guest.h"
#include "paging.h"
#include "memory.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
#ifndef VMX_NESTED_VCPU_TEST
extern struct vmx_guest_registers vmx_guest_regs;
#endif
extern int vmx_guest_enter(unsigned resume);
#ifdef VMX_NESTED_STATE_TEST
extern const unsigned char nested_state_first_call[],nested_state_second_call[];
#endif
extern const unsigned char nested_l1_exit[],nested_l2_entry[],nested_l2_resume[];
#ifdef VMX_NESTED_PARALLEL_TEST
extern const unsigned char nested_parallel_call[];
#endif
struct desc {uint16_t limit;uint64_t base;} __attribute__((packed));
static bool get(uint64_t f,uint64_t *v) {
    unsigned char fail;__asm__ volatile("vmread %2,%1;setna %0":"=qm"(fail),"=r"(*v):"r"(f):"cc");return !fail;
}
static bool put(uint64_t f,uint64_t v) {
    unsigned char fail;__asm__ volatile("vmwrite %2,%1;setna %0":"=qm"(fail):"r"(f),"r"(v):"cc","memory");return !fail;
}
static bool load(uint64_t p) {unsigned char fail;__asm__ volatile("vmptrld %1;setna %0":"=qm"(fail):"m"(p):"cc","memory");return !fail;}
static bool clear(uint64_t p) {unsigned char fail;__asm__ volatile("vmclear %1;setna %0":"=qm"(fail):"m"(p):"cc","memory");return !fail;}
static uint8_t *ram(struct ept_fixture *f,uint64_t pa) {
    if(pa<0x1000||pa>=((uint64_t)EPT_GUEST_PAGES+1)*4096) return 0;
    return (uint8_t *)paging_phys_to_virt(f->pages[(pa>>12)-1])+(pa&4095);
}
static bool walk(struct ept_fixture *f,uint64_t expected_cr3,uint64_t va,bool write,bool execute,uint64_t *pa) {
    uint64_t table,cr4;
    if(!get(0x6802,&table)||table!=expected_cr3||!get(0x6804,&cr4)||(cr4&(1u<<12))||va>=0x800000000000ULL) return false;
    for(int shift=39;shift>=12;shift-=9) {
        uint64_t *p=(void *)ram(f,table);
        if(!p||(table&4095))return false;
        uint64_t e=p[(va>>shift)&511];
        if(!(e&1)||(write&&!(e&2))||(execute&&(e>>63))||(e&~UINT64_C(0x800ffffffffff063)))return false;
        table=e&UINT64_C(0xffffffffff000);
    }
    *pa=table|(va&4095);return true;
}
static void regs(struct vmx_guest_registers *g,uint64_t r[16],uint64_t rsp) {
    r[0]=g->rax;r[1]=g->rcx;r[2]=g->rdx;r[3]=g->rbx;r[4]=rsp;r[5]=g->rbp;r[6]=g->rsi;r[7]=g->rdi;
    r[8]=g->r8;r[9]=g->r9;r[10]=g->r10;r[11]=g->r11;r[12]=g->r12;r[13]=g->r13;r[14]=g->r14;r[15]=g->r15;
}
static bool setreg(struct vmx_guest_registers *g,unsigned r,uint64_t value) {
    uint64_t *p[]={&g->rax,&g->rcx,&g->rdx,&g->rbx,0,&g->rbp,&g->rsi,&g->rdi,&g->r8,&g->r9,&g->r10,&g->r11,&g->r12,&g->r13,&g->r14,&g->r15};
    if(r==4)return put(0x681c,value);
    if(r>15)return false;
    *p[r]=value;return true;
}
#if defined(VMX_NESTED_DOMAIN_TEST) && !defined(VMX_NESTED_OWNER_SPARSE_TEST)
static bool domain_flush(uint64_t eptp) {
    struct {uint64_t eptp,reserved;} d __attribute__((aligned(16)))={eptp,0};
    unsigned char fail;uint64_t type=1;
    __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(d),"r"(type):"cc","memory");return !fail;
}
#endif
static uint64_t label(const unsigned char *p) {return 0x6000+(uintptr_t)p-(uintptr_t)ept_payload_start;}
static bool virtual_msr(const struct nested_profile *p,uint32_t msr,uint64_t *v) {
    switch(msr) {
    case 0x3a:*v=5;return true;
    case 0x480:*v=NVMX_REVISION|(UINT64_C(4096)<<32)|(UINT64_C(6)<<50);return true;
    case 0x481:*v=0;return true;
    case 0x482:*v=p->nested_ept?(UINT64_C(1)<<63):0;return true;
    case 0x48b:if(!p->nested_ept)return false;*v=UINT64_C(2)<<32;return true;
    case 0x48c:if(!p->nested_ept)return false;*v=(1u<<6)|(1u<<14)|(1u<<20)|(1u<<25)|((p->ept_contexts||p->domain_contexts)?(1u<<26):0)|(p->ept_large?((1u<<16)|(1u<<17)):0)
#ifdef VMX_NESTED_ATOMIC_GIGAPAGE_TEST
    |(p->domain_atomic_catalog?((1u<<16)|(1u<<17)):0)
#elif defined(VMX_NESTED_ATOMIC_LARGEPAGE_TEST)
    |(p->domain_atomic_catalog?(1u<<16):0)
#endif
    |((p->ept_ad||p->domain_atomic_ad||p->domain_atomic_catalog)?(1u<<21):0);return true;
    case 0x483:*v=((UINT64_C(512)|(p->memory_types?((1u<<18)|(1u<<19)):0))<<32)|512;return true;
    case 0x484:*v=((UINT64_C(512)|(p->memory_types?(1u<<14):0))<<32)|512;return true;
    case 0x485:*v=0;return true;
    case 0x486:case 0x487:*v=p->cr0;return true;
    case 0x488:case 0x489:*v=p->cr4;return true;
    default:return false;
    }
}
/* Reconstruct virtual VM-exit host load into 01 GUEST fields. Physical 02
 * host fields never receive a value from VMCS12. Strict validated profile. */
static bool reflect_host(const struct nested_profile *p) {
    if(p->memory_types&&!put(0x2804,p->pat))return false;
    if(!put(0x6800,p->cr0)||!put(0x6802,p->shared_domain?(0x1000+0x20000*p->domain_cpu):0x1000)||!put(0x6804,p->cr4)||
       !put(0x681c,0x8000)||!put(0x681e,p->l1_exit)||!put(0x6820,2)||
       !put(0x681a,0x400)||!put(0x2802,0)||!put(0x4824,0)||!put(0x4826,0)||!put(0x6822,0))return false;
    for(unsigned i=0;i<8;i++) {
        uint64_t sel=16,lim=UINT32_MAX,ar=0xc093,base=0;
        if(i==1){sel=8;ar=0xa09b;}
        if(i==6){sel=0;lim=0;ar=1u<<16;}
        if(i==7){sel=24;lim=103;ar=0x8b;base=0x5100;}
        if(!put(0x800+2*i,sel)||!put(0x4800+2*i,lim)||!put(0x4814+2*i,ar)||!put(0x6806+2*i,base))return false;
    }
    return put(0x4810,0xffff)&&put(0x4812,0xffff)&&put(0x6816,0x5000)&&put(0x6818,0x5800)&&
        put(0x482a,0)&&put(0x6824,0)&&put(0x6826,0);
}
#ifdef VMX_NESTED_VCPU_TEST
static uint32_t context_cpu(void){uint32_t a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));return b>>24;}
#define vmx_guest_regs (vcpu->frame.regs)
#define vmx_guest_cr2 (vcpu->frame.guest_cr2)
#define vmx_host_cr2 (vcpu->frame.host_cr2)
#ifdef VMX_NESTED_PARALLEL_TEST
#include "nested_parallel.h"
#ifdef VMX_NESTED_RECLAIM_TEST
extern unsigned char nested_reclaim_second_call[];
#endif
#define log_text(s) nested_parallel_trace_text(vcpu->parallel,(s))
#define log_hex(v) nested_parallel_trace_hex(vcpu->parallel,(v))
#define panic(s) nested_parallel_fault_stop(vcpu->parallel,1,(s))
#endif
bool nested_guest_run_context(struct nested_vcpu *vcpu,uint64_t vmcs01,uint64_t basic,struct ept_fixture *f) {
    if(!vcpu||vcpu->owner_apic!=context_cpu()||vcpu->vmcs01!=vmcs01||f!=&vcpu->fixture)return false;
#else
bool nested_guest_run(uint64_t vmcs01,uint64_t basic,struct ept_fixture *f) {
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    log_text("NESTED.TYPED_ADAPTER_BEGIN\n");
#endif
    bool success=false,launched01=false,launched02=false,in_l2=false,owned02=false;
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
    if(!vcpu->parallel->source_bind||!vcpu->parallel->source_bind(vcpu->parallel,&launched01,&launched02))
        panic("N47B SOURCE ADAPTER BIND RETAIN");
#endif
#ifdef VMX_NESTED_ATOMIC_RETRACK_TEST
    if(!vcpu->parallel->retrack_ref)panic("N42B MISSING REFERENCE ADAPTER RETAIN");
    vcpu->parallel->retrack_ref->local_launched01=&launched01;
    vcpu->parallel->retrack_ref->local_launched02=&launched02;
    vcpu->parallel->retrack_ref->composed_eptp=&vcpu->parallel->composed_eptp;
#endif
#if defined(VMX_NESTED_DOMAIN_KICK_TEST) || defined(VMX_NESTED_OWNER_SPARSE_TEST)
    bool internal_resume=false;
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    bool kick_handled=false;
#endif
#endif
    uint64_t vmcs02=0;
#ifdef VMX_NESTED_EVENT_TEST
    bool internal_injection=false;
#ifdef VMX_NESTED_INJECT_TEST
    struct nested_entry_event l1_event={0};
    unsigned l1_deliveries=0;
#endif
    unsigned internal_uds=0,reflected_uds=0,event_calls=0;
#endif
#ifdef VMX_NESTED_FAILURE_TEST
    bool failure_armed=false;unsigned failed_entries=0;
#endif
#ifdef VMX_NESTED_EPT_TEST
    struct nested_ept_hw composed={0};
    bool composed_ready=false,invept_done=false;
#if defined(VMX_NESTED_EPT_AD_TEST) && !defined(VMX_NESTED_EPT_AD_CONTEXT_TEST)
    struct nested_ept_ad_cache ad_cache={0};
#endif
#ifdef VMX_NESTED_EPT_CONTEXT_TEST
    struct nested_ept_contexts contexts={0};
    uint64_t selected12=0;
    struct nested_ept_context_item *hardware_context=NULL;
#endif
#if defined(VMX_NESTED_EPT_MAP_TEST) || defined(VMX_NESTED_EPT_SPARSE_TEST)
    unsigned invept_count=0;
#endif
#endif
#ifdef VMX_NESTED_UC_TEST
    bool uc_live=false,uc_pinned=false;
#endif
    uint64_t config_gpa=0xc000,on_gpa=0xd000,object_gpa=0xe000,expected_cr3=0x1000;
#ifdef VMX_NESTED_DOMAIN_TEST
    config_gpa+=vcpu->parallel->cpu_index*0x20000;on_gpa+=vcpu->parallel->cpu_index*0x20000;
    object_gpa+=vcpu->parallel->cpu_index*0x20000;expected_cr3+=vcpu->parallel->cpu_index*0x20000;
#endif
    struct nvmx model;
    struct nested_profile profile={.l2_entry=label(nested_l2_entry),.l2_resume=label(nested_l2_resume),.l1_exit=label(nested_l1_exit)};
#ifdef VMX_NESTED_DOMAIN_TEST
    profile.shared_domain=true;
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    profile.domain_atomic_catalog=true;profile.domain_eviction=true;
    profile.domain_initial_eptp=0x1005e;profile.domain_root_allowed=nested_owner_atomic_allowed;
    profile.domain_root_context=&vcpu->parallel->atomic_catalog;
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    profile.domain_typed_uc=true;
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
    profile.domain_typed_contexts=true;
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
    profile.domain_typed_dynamic=true;
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
    profile.domain_initial_eptp=vcpu->parallel->sparse.dynamic_typed->initial_key;
    profile.domain_root_allowed=catalog_root_allowed;
    profile.domain_root_context=vcpu->parallel->sparse.dynamic_typed;
#endif
#endif
#else
    profile.l2_finish=label(nested_typed_wb_entry);
#endif
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
    profile.domain_atomic_ad=true;
#endif
#ifdef VMX_NESTED_OWNER_CONTEXT_TEST
    profile.domain_contexts=true;
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    profile.domain_eviction=true;
#endif
#endif
    profile.domain_cpu=vcpu->parallel->cpu_index;profile.nested_ept=true;
#endif
#ifdef VMX_NESTED_EPT_TEST
    profile.nested_ept=true;
#ifdef VMX_NESTED_UC_PAT_TEST
    (void)invept_done;profile.uc_pat=true;
#endif
#ifdef VMX_NESTED_EPT_SPARSE_TEST
    profile.ept_sparse=true;
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000008),"c"(0));
    profile.physical_bits=(a&255)>48?48:(a&255);
    if(profile.physical_bits<40){log_text("NESTED.SPARSE_BLOCKED MAXPHYS_LT40\n");goto done;}
    unsigned sparse_fills=0,high_seen=0;
#ifdef VMX_NESTED_EPT_MISCONFIG_TEST
    (void)high_seen;
#endif
#ifdef VMX_NESTED_EPT_AD_TEST
    (void)high_seen;profile.ept_ad=true;profile.ept_large=true;
#endif
#ifdef VMX_NESTED_EPT_LARGE_TEST
    profile.ept_large=true;
    const struct nested_ept_policy large_policy={profile.physical_bits,true,true};
#endif
#ifdef VMX_NESTED_EPT_CONTEXT_TEST
    (void)high_seen;profile.ept_contexts=true;
    composed.fixture=f;
    struct nested_sparse_ops context_ops=nested_sparse_hw_ops();
#ifdef VMX_NESTED_UC_CONTEXT_TEST
    profile.uc_contexts=true;
    if(!nested_ept_contexts_init_config(&contexts,&context_ops,&composed,profile.physical_bits,2,true,false,cache_registered_context_lifecycle,f))goto done;
#elif defined(VMX_NESTED_EPT_AD_CONTEXT_TEST)
#ifdef VMX_NESTED_EPT_AD_MODE_TEST
    profile.ept_ad_modes=true;
    if(!nested_ept_contexts_init_mixed(&contexts,&context_ops,&composed,profile.physical_bits,4,false,nested_ad_context_lifecycle,&composed))goto done;
#else
    if(!nested_ept_contexts_init_config(&contexts,&context_ops,&composed,profile.physical_bits,4,false,true,nested_ad_context_lifecycle,&composed))goto done;
#endif
#elif defined(VMX_NESTED_EPT_DYNAMIC_TEST)
    profile.ept_dynamic=true;
    if(!nested_ept_contexts_init_budget(&contexts,&context_ops,&composed,profile.physical_bits,6,true))goto done;
#else
    if(!nested_ept_contexts_init(&contexts,&context_ops,&composed,profile.physical_bits))goto done;
#endif
#endif
#endif
#ifdef VMX_NESTED_EPT_MAP_TEST
    profile.ept_map=true;
#endif
#endif
#ifdef VMX_NESTED_STATE_TEST
    profile.state_roundtrip=true;
#endif
#ifdef VMX_NESTED_FAILURE_TEST
    profile.entry_failure_probe=true;
#endif
#ifdef VMX_NESTED_EVENT_TEST
#ifdef VMX_NESTED_INJECT_TEST
    profile.entry_injection=true;
#ifdef VMX_NESTED_GP_INJECT_TEST
#ifdef VMX_NESTED_PF_INJECT_TEST
    profile.pf_injection=true;
#ifdef VMX_NESTED_PF_ROUTE_TEST
    profile.pf_routing=true;
#endif
#endif
    profile.gp_injection=true;profile.l2_gp_fault=label(nested_gp_fault);
#endif
#endif
    profile.exception_routing=true;profile.l2_finish=label(nested_event_finish);
#endif
    struct nested_field_value table[NVMX_FIELDS],snapshot[NVMX_FIELDS+8];
    struct desc gdtr,idtr;__asm__ volatile("sgdt %0;sidt %1":"=m"(gdtr),"=m"(idtr));
#ifdef VMX_NESTED_MEMORY_TEST
    profile.memory_types=true;(void)high_seen;
#endif
    if(!get(0x6800,&profile.cr0)||!get(0x6804,&profile.cr4)||!get(0x2806,&profile.efer)||!get(0x2804,&profile.pat))goto done;
    size_t count=nested_profile_table(&profile,table,NVMX_FIELDS),sn=0;
    if(!count||!nvmx_init(&model,on_gpa,object_gpa,48))goto done;
    for(size_t i=0;i<count;i++) {
        snapshot[sn].field=table[i].field;
        if(!get(snapshot[sn].field,&snapshot[sn].value))goto done;
        sn++;
    }
    uint64_t extra[]={0x2010,0x201a};
    for(unsigned i=0;i<2;i++){snapshot[sn].field=extra[i];if(!get(extra[i],&snapshot[sn].value))goto done;sn++;}
#ifdef VMX_NESTED_STATE_TEST
    uint64_t physical_exit,physical_entry,secondary;
    /* VMREAD of these guest MSR fields is current only with save controls. */
    if(!get(0x400c,&physical_exit)||!get(0x4012,&physical_entry)||
       !get(0x401e,&secondary)||!nested_state_save_controls(physical_entry,physical_exit,secondary))goto done;
#endif
#ifdef VMX_NESTED_MEMORY_TEST
    uint64_t mem_entry,mem_exit;
    if(!get(0x4012,&mem_entry)||!get(0x400c,&mem_exit)||
       !(mem_entry&(1u<<14))||!(mem_exit&(1u<<18))||!(mem_exit&(1u<<19))||
       (profile.cr0&UINT64_C(0x60000000)))goto done;
#endif
    uint64_t primary;
    if(!get(0x4002,&primary)||(primary&(1u<<28)))goto done; /* all MSRs exit */
    uint64_t *input=(void *)ram(f,config_gpa);
    for(size_t i=0;i<count;i++){input[2*i]=table[i].field;input[2*i+1]=table[i].value;}
    *(uint64_t *)ram(f,config_gpa+0x800)=count;*(uint64_t *)ram(f,config_gpa+0x810)=on_gpa;*(uint64_t *)ram(f,config_gpa+0x818)=object_gpa;
#ifdef VMX_NESTED_EPT_TEST
    *(uint64_t *)ram(f,0xc830)=0x1001e;*(uint64_t *)ram(f,0xc838)=0;
    *(uint64_t *)ram(f,0x8000)=0x42;*(uint64_t *)ram(f,0xf000)=0x12345678;
#ifdef VMX_NESTED_EPT_MAP_TEST
    *(uint64_t *)ram(f,0x9000)=0x43;*(uint64_t *)ram(f,0xa000)=0x99;
#endif
#endif
#ifdef VMX_NESTED_PF_ROUTE_TEST
    *(uint64_t *)ram(f,0x8000)=0x42;
#endif
#ifdef VMX_NESTED_EPT_CONTEXT_TEST
    *(uint64_t *)ram(f,0x8000)=0x11;*(uint64_t *)ram(f,0x9000)=0x22;
    *(uint64_t *)ram(f,0xa000)=0x33;*(uint64_t *)ram(f,0xb000)=0x44;
#endif
#ifdef VMX_NESTED_EPT_AD_TEST
    *(uint64_t *)ram(f,0x8000)=0x11;
#endif
    *(uint32_t *)ram(f,on_gpa)=NVMX_REVISION;*(uint32_t *)ram(f,object_gpa)=NVMX_REVISION;
    #ifdef VMX_NESTED_PARALLEL_TEST
    vmcs02=vcpu->parallel->reserved02;
#else
    vmcs02=pmm_alloc_page();
#endif
    if(!vmcs02||!vmx_memory_region_wb(vmcs02))goto done;
    uint32_t *region=paging_phys_to_virt(vmcs02);if(!region)goto done;
    for(unsigned i=0;i<1024;i++)region[i]=0;
    region[0]=(uint32_t)basic&0x7fffffff;
    if(!clear(vmcs02))goto done;
    owned02=true;
#ifdef VMX_NESTED_VCPU_TEST
    if(!nested_vcpu_register02(vcpu,context_cpu(),vmcs02))goto done;
#endif
    if(!load(vmcs02))goto done;
    for(size_t i=0;i<sn;i++)if(!put(snapshot[i].field,snapshot[i].value))goto done;
#ifdef VMX_NESTED_DOMAIN_TEST
#ifdef VMX_NESTED_OWNER_SPARSE_TEST
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    log_text("NESTED.TYPED_SPARSE_START\n");
#endif
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    {uint32_t low,high;__asm__ volatile("rdmsr":"=a"(low),"=d"(high):"c"(0x48c));
     (void)high;vcpu->parallel->atomic_catalog.physical_ad=!!(low&(1u<<21));
     if(!vcpu->parallel->atomic_catalog.physical_ad)goto done;}
#endif
    if(!nested_owner_sparse_start(&vcpu->parallel->sparse,f))goto done;
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    log_text("NESTED.TYPED_SPARSE_READY\n");
#endif
    vcpu->parallel->composed_eptp=vcpu->parallel->sparse.multiple?
        nested_ept_contexts_active(&vcpu->parallel->sparse.contexts)->eptp:vcpu->parallel->sparse.tree.eptp;
#else
    if(!domain_flush(vcpu->parallel->composed_eptp))goto done;
#endif
#endif
    if(!load(vmcs01))goto done;
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    if(!vcpu->parallel->typed_checkpoint||!vcpu->parallel->typed_checkpoint(vcpu->parallel,2))goto done;
#endif
    vmx_guest_regs=(struct vmx_guest_registers){0};
#ifdef VMX_NESTED_VCPU_TEST
    vmx_guest_regs.r14=UINT64_C(0x1432143214321432);vmx_guest_regs.r15=UINT64_C(0x1532153215321532);
#ifdef VMX_NESTED_PARALLEL_TEST
    vmx_guest_regs.r14+=vcpu->parallel->cpu_index;vmx_guest_regs.r15+=vcpu->parallel->cpu_index;
    vcpu->frame.guest_cr2=UINT64_C(0x32320000)+vcpu->parallel->cpu_index*4096;
#endif
#endif
#ifdef VMX_NESTED_UC_TEST
    cache_registered_begin(f);uc_live=true;
#endif
    unsigned l2_exits=0;
#ifdef VMX_NESTED_PF_INJECT_TEST
    /* One architectural CR2 shared by virtual L1/L2, never a VMCS field.
     * L0's own CR2 is saved separately by the physical entry wrapper. */
    vmx_guest_cr2=0;
#endif
#if defined(VMX_NESTED_EPT_SPARSE_TEST) || defined(VMX_NESTED_OWNER_SPARSE_TEST)
#if defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST) || defined(VMX_NESTED_ATOMIC_MULTIROUND_TEST) || defined(VMX_NESTED_SOURCE_REPLACE_TEST)
    const unsigned max_steps=2048;
#else
    const unsigned max_steps=768;
#endif
#else
    const unsigned max_steps=256;
#endif
    for(unsigned step=0;step<max_steps;step++) {
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
        /* BEFORE any ordinary owner/config/registry access. Parked owners
         * execute only their explicit maintenance command or exact release. */
        if(!nsrr_before_entry(vcpu->parallel->source_round,vcpu->parallel->cpu_index))break;
#endif
#ifdef VMX_NESTED_PF_INJECT_TEST
        uint64_t original_cr2,returned_cr2;
        __asm__ volatile("mov %%cr2,%0":"=r"(original_cr2));
        __asm__ volatile("mov %0,%%cr2"::"r"(UINT64_C(0x33330000)):"memory");
#endif
#ifdef VMX_NESTED_UC_PAT_TEST
        if(in_l2){uint64_t pat,cr0,mask,shadow,primary;
            if(!get(0x2804,&pat)||!get(0x6800,&cr0)||!get(0x6000,&mask)||!get(0x6004,&shadow)||
               !get(0x4002,&primary)||(primary&(1u<<28))||
               (mask&UINT64_C(0x60000000))!=UINT64_C(0x60000000)||(shadow&UINT64_C(0x60000000))||
               !cache_registered_pat_entry(pat,cr0))break;
        }
#endif
#if defined(VMX_NESTED_WT_TEST) || defined(VMX_NESTED_OWNER_TYPED_TEST) || defined(VMX_NESTED_ATOMIC_UC_TEST)
        {uint64_t cr0,mask,shadow,primary;
            if(!get(0x6800,&cr0)||!get(0x6000,&mask)||!get(0x6004,&shadow)||!get(0x4002,&primary)||
               (cr0&UINT64_C(0x60000000))||(shadow&UINT64_C(0x60000000))||
               (mask&UINT64_C(0x60000000))!=UINT64_C(0x60000000)||(primary&(1u<<28)))break;
        }
#endif
#ifdef VMX_NESTED_VCPU_TEST
        uint64_t current;unsigned char ptrfail;
        __asm__ volatile("vmptrst %1;setna %0":"=qm"(ptrfail),"=m"(current)::"cc","memory");
        struct nested_host_state host_state;
        if(ptrfail||current!=(in_l2?vmcs02:vmcs01)||!nested_host_state_capture(&host_state))break;
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
        uint64_t actual_eptp;
        if(!get(0x201a,&actual_eptp)||
           !nested_owner_atomic_entry(&vcpu->parallel->atomic_catalog,current,actual_eptp,in_l2))break;
#endif
        if(!nested_vcpu_authorize(vcpu,context_cpu(),current,in_l2?launched02:launched01))break;
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
        if(!vcpu->parallel->typed_before_entry||
           !vcpu->parallel->typed_before_entry(vcpu->parallel,in_l2))break;
#endif
#ifndef VMX_NESTED_PF_INJECT_TEST
        uint64_t ctx_original_cr2,ctx_returned_cr2;
        __asm__ volatile("mov %%cr2,%0":"=r"(ctx_original_cr2));
        __asm__ volatile("mov %0,%%cr2"::"r"(UINT64_C(0x44440000)):"memory");
#endif
        #ifdef VMX_NESTED_DOMAIN_TEST
#if defined(VMX_NESTED_DOMAIN_KICK_TEST) || defined(VMX_NESTED_OWNER_SPARSE_TEST)
        if(internal_resume) {
            if(!nested_domain_dispatch_physical_resume(vcpu->parallel->domain,vcpu->parallel->cpu_index,vcpu->parallel->domain_token,&model))panic("DOMAIN INTERNAL RESUME GATE");
            internal_resume=false;
        } else
#endif
        if(!nested_domain_dispatch_physical_enter(vcpu->parallel->domain,vcpu->parallel->cpu_index,vcpu->parallel->domain_token,&model,in_l2))panic("DOMAIN ENTER GATE RETAIN");
#endif
        unsigned ctx_status=nested_vcpu_enter(&vcpu->frame,in_l2?launched02:launched01);
#ifndef VMX_NESTED_PF_INJECT_TEST
        __asm__ volatile("mov %%cr2,%0":"=r"(ctx_returned_cr2));
        __asm__ volatile("mov %0,%%cr2"::"r"(ctx_original_cr2):"memory");
        if(ctx_returned_cr2!=0x44440000)panic("NESTED CTX HOST CR2 LOST");
#endif
        int entered=ctx_status==0?1:0;
#else
        int entered=vmx_guest_enter(in_l2?launched02:launched01);
#endif
#ifdef VMX_NESTED_PF_INJECT_TEST
        __asm__ volatile("mov %%cr2,%0":"=r"(returned_cr2));
        __asm__ volatile("mov %0,%%cr2"::"r"(original_cr2):"memory");
        if(returned_cr2!=0x33330000)panic("NESTED L0 CR2 RESTORE FAILED");
#endif
        __asm__ volatile("lgdt %0;lidt %1"::"m"(gdtr),"m"(idtr):"memory");
#ifdef VMX_NESTED_VCPU_TEST
        if(!nested_host_state_matches(&host_state))panic("NESTED CTX HOST STATE LOST RETAIN");
        uint64_t ctx_reason=0;
        if(!ctx_status&&!get(0x4402,&ctx_reason))ctx_status=4;
        #ifdef VMX_NESTED_DOMAIN_TEST
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
        if(!ctx_status&&ctx_reason==1) {
            uint64_t kinfo,krip,kflags,kblock,kvector;
            if(!in_l2||kick_handled||vcpu->parallel->cpu_index!=1||
               !get(0x4404,&kinfo)||kinfo!=0x80000041||
               !get(0x681e,&krip)||krip<label(nested_domain_kick_begin)||
               krip>=label(nested_domain_kick_loop_end)||
               !get(0x6820,&kflags)||(kflags&((1u<<9)|(1u<<8)))||
               !get(0x4824,&kblock)||kblock||
               !get(0x4408,&kvector)||(kvector&(1u<<31))||
               !nested_kick_consume(vcpu->parallel->lapic_base,(uint32_t)kinfo)||
               !nested_domain_dispatch_physical_pause(vcpu->parallel->domain,1,vcpu->parallel->domain_token,&model,ctx_status,ctx_reason))
                panic("DOMAIN KICK OWNERSHIP FAILURE RETAIN");
            kick_handled=true;
        } else
#endif
#ifdef VMX_NESTED_OWNER_SPARSE_TEST
        if(!ctx_status&&ctx_reason==48) {
            if(!in_l2||!nested_domain_dispatch_physical_suspend_ept(vcpu->parallel->domain,vcpu->parallel->cpu_index,vcpu->parallel->domain_token,&model,ctx_status,ctx_reason))
                panic("DOMAIN EPT SUSPEND RETAIN");
        } else
#endif
        if(!nested_domain_dispatch_physical_leave(vcpu->parallel->domain,vcpu->parallel->cpu_index,vcpu->parallel->domain_token,&model,in_l2,ctx_status,ctx_reason))panic("DOMAIN EXIT GATE RETAIN");
#endif
        if(!nested_vcpu_complete(vcpu,context_cpu(),current,ctx_status,ctx_reason))break;
        uint64_t sentinel_delta=0;
#ifdef VMX_NESTED_PARALLEL_TEST
        sentinel_delta=vcpu->parallel->cpu_index;
        if(vcpu->frame.guest_cr2!=UINT64_C(0x32320000)+sentinel_delta*4096)break;
#endif
        if(vmx_guest_regs.r14!=UINT64_C(0x1432143214321432)+sentinel_delta||vmx_guest_regs.r15!=UINT64_C(0x1532153215321532)+sentinel_delta)break;
#endif
        if(entered!=1){log_text("NESTED.PHYSICAL_ENTRY_FAILED\n");break;}
        uint64_t reason,rip,rsp,flags,length,blocking,debug,vectoring;
        if(!get(0x4402,&reason))break;
#ifdef VMX_NESTED_FAILURE_TEST
        if(in_l2&&(reason&(UINT64_C(1)<<31))) {
            uint64_t qualification;
            if(!failure_armed||failed_entries||reason!=0x80000021||
               !get(0x6400,&qualification)||qualification||
               !nvmx_failed_entry_finish(&model,reason,qualification)||
               !load(vmcs01)||!reflect_host(&profile))break;
            /* No VMREAD guest snapshot, no commit and no undefined exit-info.
             * Physical and virtual launch state both remain clear. */
            failed_entries++;failure_armed=false;in_l2=false;
            log_text("NESTED.FAILED_ENTRY_REFLECTED reason=80000021 launch_still_clear=1\n");continue;
        }
        if(failure_armed)break; /* expected invalid state must never execute L2 */
#endif
        if(in_l2)launched02=true;else launched01=true;
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
        if(reason==1) {
            log_text("NESTED.L0_KICK reason1 info80000041 IF0 original_RIP=");
            uint64_t stopped_rip;if(!get(0x681e,&stopped_rip))break;log_hex(stopped_rip);
            if(!vcpu->parallel->reclaim_checkpoint(vcpu->parallel))break;
            internal_resume=true;
            continue; /* L0 internal event: no virtual exit fields or RIP advance. */
        }
#endif
        if(!get(0x681e,&rip)||!get(0x681c,&rsp)||!get(0x6820,&flags)||
           !get(0x4824,&blocking)||!get(0x6822,&debug)||!get(0x4408,&vectoring)||
           blocking||debug||(flags&(1u<<8))||(vectoring&(1u<<31)))break;
        log_text(in_l2?"NESTED.L2_EXIT=":"NESTED.L1_EXIT=");log_hex(reason);
#ifdef VMX_NESTED_OWNER_SPARSE_TEST
        if(in_l2&&reason==48){
            uint64_t qualification,gpa;
            if(!get(0x6400,&qualification)||!get(0x2400,&gpa))break;
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
            uint64_t actual_pin_controls;
            if(!get(0x4000,&actual_pin_controls))break;
            if(!nested_owner_atomic_fault(&vcpu->parallel->atomic_catalog,gpa,qualification,actual_pin_controls)){
                const struct nested_atomic_rejection *r=&vcpu->parallel->atomic_catalog.rejection;
                if(__atomic_load_n(&r->valid,__ATOMIC_ACQUIRE)){
                    log_text("N41C_REJECT_REASON=");log_hex(r->reason);
                    log_text("N41C_REJECT_QUALIFICATION=");log_hex(r->qualification);
                    log_text("N41C_REJECT_GPA=");log_hex(r->gpa);
                    log_text("N41C_REJECT_PIN_CONTROLS=");log_hex(r->pin_controls);
                    log_text("N41C_REJECT_STAGE=");log_hex(r->stage);
                    if(r->permissions_valid){log_text("N41C_REJECT_PRIVATE_PERMISSIONS=");log_hex(r->actual_permissions);}
                    if(r->stage==NOA_REJECT_BRIDGE){log_text("N41C_REJECT_BRIDGE_STATUS=");log_hex(r->bridge_status);}
                }
                break;
            }
            log_text("N41C_FAULT_QUALIFICATION=");log_hex(qualification);
            if((qualification&0x180)==0x80)log_text("N41C_PAGEWALK_RW=1\n");
#else
            if(!(qualification&7)||
#ifdef VMX_NESTED_OWNER_AD_TEST
               ((qualification&0x80)&&!(qualification&0x100)&&(qualification&3)!=3)||
#endif
               !nested_owner_sparse_miss(&vcpu->parallel->sparse,gpa,qualification&7))break;
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
            log_text("NESTED.OWNER_AD_NEED=");log_hex(qualification&7);
            if((qualification&0x80)&&!(qualification&0x100)){
                log_text("NESTED.OWNER_AD_PAGEWALK_RW=1\n");
            }
#endif
            log_text("NESTED.OWNER_EPT_INTERNAL_FILL=");log_hex(gpa);
#ifdef VMX_NESTED_OWNER_EVICT_TEST
            log_text("NESTED.OWNER_FILL_KEY=");log_hex(vcpu->parallel->sparse.selected12);
#endif
            internal_resume=true;continue;
        }
#endif
        if(in_l2) {
#ifdef VMX_NESTED_PF_ROUTE_TEST
#include "nested_pf_route_adapter.inc"
#elif defined(VMX_NESTED_EVENT_TEST)
            /* Virtual entry commits once, even if L0 handles several physical
             * exits before exposing an exit to L1. */
            if(model.prepared) {if(!nvmx_entry_commit(&model))break;}
            else if(!model.running)break;
            uint64_t info=0,error=0,bitmap,pending;
            if(!nvmx_peek(&model,0x4004,&bitmap)||!get(0x4016,&pending))break;
#ifdef VMX_NESTED_INJECT_TEST
            if(l1_event.armed) {
                /* Ambiguous intermediate delivery exits are not retried. The
                 * normal virtual exit below, not this observation, clears 12. */
                if(internal_injection||reason!=18||rip!=label(nested_event_finish)||
                   vmx_guest_regs.r13!=2||l1_deliveries||
                   !nested_entry_observe(&l1_event,true)||!put(0x4016,pending&~UINT64_C(0x80000000)))break;
                pending&=~UINT64_C(0x80000000);l1_deliveries++;
#ifdef VMX_NESTED_GP_INJECT_TEST
#ifdef VMX_NESTED_PF_INJECT_TEST
                if(vmx_guest_cr2!=0xf000)break;
                log_text("NESTED.L1_PF_DELIVERY_CONFIRMED CR2=f000 handler_frame_verified=1\n");
#else
                log_text("NESTED.L1_GP_DELIVERY_CONFIRMED error=28 handler_frame_verified=1\n");
#endif
#else
                log_text("NESTED.L1_UD_DELIVERY_CONFIRMED bitmap12_still_set=1\n");
#endif
            }
#endif
            if(internal_injection) {
                /* Only this trusted handler's completed VMCALL proves delivery.
                 * Any intervening exit fails closed instead of reinjecting. */
                if(reason!=18||rip!=label(nested_event_first_call)||vmx_guest_regs.r13!=1||
                   !put(0x4016,0))break;
                internal_injection=false;pending=0;
                log_text("NESTED.L0_UD_DELIVERY_CONFIRMED\n");
            }
            if(reason==0) {
                if(!get(0x4404,&info))break;
                if((info&0x80000800)==0x80000800&&!get(0x4406,&error))break;
                struct nested_event_input input={.exit_reason=(uint32_t)reason,.exit_info=(uint32_t)info,.exit_error=(uint32_t)error,
                    .vectoring=(uint32_t)vectoring,.entry_pending=(uint32_t)pending,.blocking=(uint32_t)blocking,
                    .pending_debug=debug,.rflags=flags,.bitmap12=(uint32_t)bitmap,.bitmap_l0=L0_EVENT_BITMAP};
                struct nested_event_injection injection;
                enum nested_event_action action=nested_event_route(&input,&injection);
                if(action==NESTED_EVENT_REINJECT_L2) {
                    /* Internal PF needs explicit CR2/event ownership handling;
                     * not implemented by the UD-only internal path. */
                    if(injection.info!=0x80000306)break;
                    if(internal_uds||!put(0x4018,injection.error)||!put(0x401a,injection.length)||
                       !put(0x4016,injection.info))break;
                    internal_injection=true;internal_uds++;
                    log_text("NESTED.UD_OWNER_L0_REINJECT_SAME_RIP\n");continue;
                }
                if(action!=NESTED_EVENT_REFLECT_L1||reflected_uds||vmx_guest_regs.r13!=1)break;
#ifdef VMX_NESTED_GP_INJECT_TEST
#ifdef VMX_NESTED_PF_INJECT_TEST
                uint64_t gla,pmask,pmatch;
                if(info!=0x80000b0e||error||rip!=profile.l2_gp_fault||vmx_guest_cr2!=0x22220000||
                   !get(0x6400,&gla)||gla!=0xf000||!nvmx_peek(&model,0x4006,&pmask)||
                   !nvmx_peek(&model,0x4008,&pmatch)||pmask||pmatch||
                   !nvmx_exit_store(&model,0x6400,gla))break;
                log_text("NESTED.PF_DIRECT_EXIT CR2_unchanged=22220000 qualification=f000\n");
#else
                if(info!=0x80000b0d||error!=0x28||rip!=profile.l2_gp_fault)break;
#endif
#ifdef VMX_NESTED_PF_INJECT_TEST
                log_text("NESTED.PF_OWNER_L1_REFLECT actual_error=");log_hex(error);
#else
                log_text("NESTED.GP_OWNER_L1_REFLECT actual_error=");log_hex(error);
#endif
#else
                log_text("NESTED.UD_OWNER_L1_REFLECT\n");
#endif
                reflected_uds++;
            } else if(reason==18) {
                if(event_calls>=2||rip!=label(event_calls?nested_event_finish:nested_event_first_call)||
#ifdef VMX_NESTED_INJECT_TEST
                   vmx_guest_regs.r13!=(event_calls?2u:1u)||
#else
                   vmx_guest_regs.r13!=1||
#endif
                   !get(0x440c,&length)||length!=3||
                   !nvmx_exit_store(&model,0x440c,length))break;
                event_calls++;
            } else break;
            if((info&0x80000800)==0x80000800&&!nvmx_exit_store(&model,0x4406,error))break;
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x4404,info)||!nvmx_return_from_l2(&model)||
               !load(vmcs01)||!reflect_host(&profile))break;
#ifdef VMX_NESTED_INJECT_TEST
            if(l1_event.armed&&!nested_entry_release(&l1_event))break;
#endif
#elif defined(VMX_NESTED_UC_PAT_TEST)
#include "nested_uc_pat_adapter.inc"
#elif defined(VMX_NESTED_UC_CONTEXT_TEST)
#include "nested_uc_context_adapter.inc"
#elif defined(VMX_NESTED_UC_TEST)
#include "nested_uc_adapter.inc"
#elif defined(VMX_NESTED_MEMORY_TEST)
#include "nested_memory_adapter.inc"
#elif defined(VMX_NESTED_EPT_AD_CONTEXT_TEST)
#ifdef VMX_NESTED_EPT_AD_MODE_TEST
#include "nested_ept_ad_mode_adapter.inc"
#else
#include "nested_ept_ad_context_adapter.inc"
#endif
#elif defined(VMX_NESTED_EPT_AD_FAIL_TEST)
#include "nested_ept_ad_fail_adapter.inc"
#elif defined(VMX_NESTED_EPT_AD_TEST)
#include "nested_ept_ad_adapter.inc"
#elif defined(VMX_NESTED_EPT_MISCONFIG_TEST)
#include "nested_ept_misconfig_adapter.inc"
#elif defined(VMX_NESTED_EPT_LARGE_TEST)
#include "nested_ept_large_adapter.inc"
#elif defined(VMX_NESTED_EPT_DYNAMIC_TEST)
#include "nested_ept_dynamic_adapter.inc"
#elif defined(VMX_NESTED_EPT_CONTEXT_TEST)
#include "nested_ept_context_adapter.inc"
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
#include "nested_ept_sparse_adapter.inc"
#elif defined(VMX_NESTED_EPT_MAP_TEST)
#include "nested_ept_map_adapter.inc"
#elif defined(VMX_NESTED_EPT_TEST)
            uint64_t q=0,gpa=0,gla=0;
            if(l2_exits==0) {
                if(reason!=48||!composed_ready||rip!=profile.l2_resume||rsp!=0x9000||
                   !get(0x6400,&q)||!get(0x2400,&gpa)||!get(0x640a,&gla)||
                   gpa!=0xf000||gla!=0xf000||(q&0x187)!=0x182)break;
                /* Classify against the cached, successfully published EPT12
                 * plan. Never auto-refresh mappings without L1 INVEPT. */
                struct nested_ept_result *plan=&composed.plan[15];
                if(plan->status!=NESTED_EPT_OK||plan->l1_gpa!=0x8000||
                   (plan->effective12&2)||!(plan->effective12&1))break;
                q=(q&UINT64_C(0x187))|((uint64_t)plan->effective12<<3);
                if(!nvmx_entry_commit(&model)||!nvmx_exit_store(&model,0x6400,q)||
                   !nvmx_exit_store(&model,0x2400,gpa)||!nvmx_exit_store(&model,0x640a,gla))break;
                length=0; /* EPT fault length undefined; L1 retries unchanged RIP. */
                log_text("NESTED.EPT12_PERMISSION_REFLECTED GPA=f000 alias=8000\n");
            } else {
                if(l2_exits!=1||reason!=18||!invept_done||!get(0x440c,&length)||length!=3||
                   *(uint64_t *)ram(f,0x8000)!=0x77||*(uint64_t *)ram(f,0xf000)!=0x12345678||
                   !nvmx_entry_commit(&model))break;
                log_text("NESTED.EPT_ALIAS_PROVED L1_8000=77 L1_f000_unchanged\n");
            }
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x440c,length)||!nvmx_exit_store(&model,0x4404,0)||
               !nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile))break;
#else
            if(reason!=18||!get(0x440c,&length)||length!=3||
               #ifdef VMX_NESTED_STATE_TEST
               rip!=label(l2_exits?nested_state_second_call:nested_state_first_call)||
               rsp!=(l2_exits?0x8700u:0x8800u)||
#else
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
               l2_exits>=8||rip!=label(l2_exits==0?nested_source_replace_call0:l2_exits==1?nested_source_replace_call1:
                   l2_exits==2?nested_source_replace_call2:l2_exits==3?nested_source_replace_call3:
                   l2_exits==4?nested_source_replace_call4:l2_exits==5?nested_source_replace_call5:
                   l2_exits==6?nested_source_replace_call6:nested_source_replace_call7)||rsp!=0x9000||flags!=2||
#elif defined(VMX_NESTED_ATOMIC_MULTIROUND_TEST)
               l2_exits>=13||rip!=label(l2_exits==0?nested_atomic_multiround_call0:l2_exits==1?nested_atomic_multiround_call1:l2_exits==2?nested_atomic_multiround_call2:l2_exits==3?nested_atomic_multiround_call3:l2_exits==4?nested_atomic_multiround_call4:l2_exits==5?nested_atomic_multiround_call5:l2_exits==6?nested_atomic_multiround_call6:l2_exits==7?nested_atomic_multiround_call7:l2_exits==8?nested_atomic_multiround_call8:l2_exits==9?nested_atomic_multiround_call9:l2_exits==10?nested_atomic_multiround_call10:l2_exits==11?nested_atomic_multiround_call11:nested_atomic_multiround_call12)||rsp!=0x9000||flags!=2||
#elif defined(VMX_NESTED_ATOMIC_RETRACK_TEST)
               rip!=label(l2_exits==0?nested_atomic_retrack_call0:l2_exits==1?nested_atomic_retrack_call1:
                   l2_exits==2?nested_atomic_retrack_call2:l2_exits==3?nested_atomic_retrack_call3:
                   l2_exits==4?nested_atomic_retrack_call4:l2_exits==5?nested_atomic_retrack_call5:
                   l2_exits==6?nested_atomic_retrack_call6:nested_atomic_retrack_call7)||rsp!=0x9000||flags!=2||
#elif defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
               rip!=label(l2_exits==0?nested_atomic_catalog_call0:l2_exits==1?nested_atomic_catalog_call1:
                   l2_exits==2?nested_atomic_catalog_call2:l2_exits==3?nested_atomic_catalog_call3:
                   l2_exits==4?nested_atomic_catalog_call4:l2_exits==5?nested_atomic_catalog_call5:
                   l2_exits==6?nested_atomic_catalog_call6:nested_atomic_catalog_call7)||rsp!=0x9000||flags!=2||
#elif defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
               rip!=label(l2_exits==0?nested_typed_dynamic_call0:l2_exits==1?nested_typed_dynamic_call1:
                   l2_exits==2?nested_typed_dynamic_call2:l2_exits==3?nested_typed_dynamic_call3:
                   l2_exits==4?nested_typed_dynamic_call4:l2_exits==5?nested_typed_dynamic_call5:
                   l2_exits==6?nested_typed_dynamic_call6:l2_exits==7?nested_typed_dynamic_call7:nested_typed_dynamic_call8)||rsp!=0x9000||
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
               rip!=label(l2_exits==0?nested_typed_context_call0:l2_exits==1?nested_typed_context_call1:
                   l2_exits==2?nested_typed_context_call2:l2_exits==3?nested_typed_context_call3:
                   l2_exits==4?nested_typed_context_call4:nested_typed_context_call5)||rsp!=0x9000||
#else
               rip!=(l2_exits?
#ifdef VMX_NESTED_OWNER_TYPED_TEST
                   label(l2_exits==1?nested_typed_uc_call:nested_typed_wb_call):
#elif defined(VMX_NESTED_OWNER_AD_RESET_TEST)
                   label(nested_ad_reset_call):
#elif defined(VMX_NESTED_OWNER_CONTEXT_TEST)
                   label(nested_owner_context_call):
#elif defined(VMX_NESTED_LOCAL_INVEPT_TEST)
                   label(nested_local_second_call):
#elif defined(VMX_NESTED_RECLAIM_TEST)
                   label(nested_reclaim_second_call):
#else
                   profile.l2_resume:
#endif
#ifdef VMX_NESTED_PARALLEL_TEST
                   label(nested_parallel_call)
#else
                   profile.l2_entry
#endif
                   )||rsp!=0x9000||
#endif
#endif
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
               l2_exits>=8)break;
            /* N47 checkpoint is deliberately AFTER private reflection below. */
#elif defined(VMX_NESTED_ATOMIC_MULTIROUND_TEST)
               l2_exits>=13)break;
            if(!vcpu->parallel->atomic_checkpoint||!vcpu->parallel->atomic_checkpoint(vcpu->parallel,l2_exits))break;
#elif defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
               l2_exits>=8)break;
            if(!vcpu->parallel->atomic_checkpoint||!vcpu->parallel->atomic_checkpoint(vcpu->parallel,l2_exits))break;
#elif defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
               l2_exits>=9)break;
            static const uint64_t dynamic_keys[]={0x1001e,0x1401e,0x1801e,0x1001e,0x1801e,0x1401e,0x1001e,0x1401e,0x1001e};
            if(vcpu->parallel->sparse.selected12!=dynamic_keys[l2_exits])break;
            if((l2_exits==3||l2_exits==6||l2_exits==8)&&(!vcpu->parallel->typed_checkpoint||
               !vcpu->parallel->typed_checkpoint(vcpu->parallel,l2_exits==3?0:l2_exits==6?1:3)))break;
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
               l2_exits>=6)break;
            if(vcpu->parallel->sparse.selected12!=((l2_exits&1)?0x1401eu:0x1001eu))break;
            if((l2_exits==1||l2_exits==3)&&(!vcpu->parallel->typed_checkpoint||
               !vcpu->parallel->typed_checkpoint(vcpu->parallel,l2_exits==1?0:1)))break;
#elif defined(VMX_NESTED_OWNER_CONTEXT_TEST)
#ifdef VMX_NESTED_OWNER_EVICT_TEST
               l2_exits>=6)break;
            static const uint64_t expected_keys[]={0x1001e,0x1401e,0x1801e,0x1001e,0x1001e,0x1801e};
            uint64_t expected_value=expected_keys[l2_exits]==0x1801e?UINT64_C(0x99aa99aa99aa99aa):
                expected_keys[l2_exits]==0x1401e?UINT64_C(0x7788778877887788):UINT64_C(0x3344334433443344);
#else
               l2_exits>=5)break;
            static const uint64_t expected_keys[]={0x1001e,0x1401e,0x1001e,0x1401e,0x1001e};
            uint64_t expected_value=(l2_exits&1)?UINT64_C(0x7788778877887788):UINT64_C(0x3344334433443344);
#endif
            if(vcpu->parallel->sparse.selected12!=expected_keys[l2_exits]||vmx_guest_regs.rax!=expected_value)break;
            log_text("NESTED.OWNER_CONTEXT_VALUE=");log_hex(vmx_guest_regs.rax);
#ifdef VMX_NESTED_OWNER_EVICT_TEST
            if(l2_exits==3||l2_exits==4){
#else
            if(l2_exits==1||l2_exits==2){
#endif
                vcpu->parallel->context_stage=l2_exits+1;
                if(!vcpu->parallel->local_checkpoint(vcpu->parallel))break;
            }
#else
#ifdef VMX_NESTED_OWNER_TYPED_TEST
               l2_exits>=3)break;
            if(l2_exits<2&&(!vcpu->parallel->typed_checkpoint||!vcpu->parallel->typed_checkpoint(vcpu->parallel,l2_exits)))break;
#elif defined(VMX_NESTED_OWNER_AD_RESET_TEST)
               l2_exits>=3)break;
            if(l2_exits<2&&(!vcpu->parallel->ad_checkpoint||
               !vcpu->parallel->ad_checkpoint(vcpu->parallel,l2_exits)))break;
#else
               l2_exits>=2)break;
#endif
#endif
#ifndef VMX_NESTED_DOMAIN_TEST
            if(!nvmx_entry_commit(&model))break;
#endif
#ifdef VMX_NESTED_STATE_TEST
            uint64_t state_fields[NVMX_FIELDS],state_values[NVMX_FIELDS];unsigned nstate=0;
            for(size_t i=0;i<count;i++)if(((table[i].field>>10)&3)==2&&table[i].field!=0x2800) {
                state_fields[nstate]=table[i].field;
                if(!get(table[i].field,&state_values[nstate]))goto done;
                nstate++;
            }
            if(!nvmx_exit_snapshot(&model,state_fields,state_values,nstate))break;
            log_text("NESTED.STATE_SNAPSHOT_FIELDS=");log_hex(nstate);
#endif
#ifdef VMX_NESTED_LOCAL_INVEPT_TEST
            if(!l2_exits && !vcpu->parallel->local_checkpoint(vcpu->parallel))break;
#endif
#ifdef VMX_NESTED_RECLAIM_TEST
            /* Both actual first L2 exits have been verified and consumer
             * leave completed; no virtual L1 execution before the ticket. */
            if(!l2_exits
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
               &&!kick_handled
#endif
               && (!vcpu->parallel->reclaim_checkpoint ||
               !vcpu->parallel->reclaim_checkpoint(vcpu->parallel)))break;
#endif
            if(!nvmx_exit_store(&model,0x681e,rip)||!nvmx_exit_store(&model,0x681c,rsp)||
               !nvmx_exit_store(&model,0x6820,flags)||!nvmx_exit_store(&model,0x4402,reason)||
               !nvmx_exit_store(&model,0x440c,length)||!nvmx_exit_store(&model,0x4404,0)||
               !nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile))break;
#endif
            in_l2=false;
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
            /* Full physical_leave has committed above. Private model return,
             * reflected host and actual VMCS01 must all precede arrival. */
            if(model.running||!vcpu->parallel->atomic_checkpoint||
               !vcpu->parallel->atomic_checkpoint(vcpu->parallel,l2_exits))break;
#endif
            l2_exits++;log_text("NESTED.REFLECT_TO_L1_COUNT=");log_hex(l2_exits);continue;
        }
        if(!get(0x440c,&length)||!length||length>15||rip>UINT64_MAX-length)break;
        if(reason==18) {
            #ifdef VMX_NESTED_PF_ROUTE_TEST
            if(model.root||l2_exits!=6||internal_uds!=2||reflected_uds!=2||event_calls!=4||
               l1_event.armed||l1_deliveries!=2||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_EVENT_TEST)
            if(model.root||l2_exits!=3||internal_uds!=1||reflected_uds!=1||event_calls!=2||
               internal_injection||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_UC_CONTEXT_TEST)
            if(model.root||l2_exits!=7||invept_count!=2||contexts.poisoned||contexts.count!=2||cache_registered_context_refs()!=2||*(uint64_t *)ram(f,0xa800)!=7||*(volatile uint64_t *)ram(f,0xf000)!=0x7d||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_UC_TEST)
            if(model.root||l2_exits!=2||invept_count!=1||composed.tree.poisoned||*(uint64_t *)ram(f,0xc820)!=2||*(volatile uint64_t *)ram(f,0xf000)!=0x99||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_MEMORY_TEST)
            if(model.root||l2_exits!=1||*(uint64_t *)ram(f,0xc820)!=1||*(uint64_t *)ram(f,0x8000)!=0x99||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_EPT_AD_CONTEXT_TEST)
#ifdef VMX_NESTED_EPT_AD_MODE_TEST
            if(model.root||l2_exits!=6||invept_count!=3||contexts.poisoned||
               *(uint64_t *)ram(f,0xc820)!=6||*(uint64_t *)ram(f,0x8000)!=0x99||vmx_guest_regs.rax!=0x4e31)break;
#else
            if(model.root||l2_exits!=6||invept_count!=2||contexts.poisoned||
               *(uint64_t *)ram(f,0xc820)!=6||*(uint64_t *)ram(f,0x8000)!=0x88||
               *(uint64_t *)ram(f,0x9000)!=0x77||vmx_guest_regs.rax!=0x4e31)break;
#endif
#elif defined(VMX_NESTED_EPT_AD_FAIL_TEST)
            if(model.root||l2_exits!=4||invept_count!=2||composed.tree.poisoned||composed.tree.generation!=3||
               *(uint64_t *)ram(f,0xc820)!=4||*(uint64_t *)ram(f,0x8000)!=0x77||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_EPT_AD_TEST)
            if(model.root||l2_exits!=4||invept_count!=1||composed.tree.poisoned||composed.tree.generation!=2||
               *(uint64_t *)ram(f,0xc820)!=4||*(uint64_t *)ram(f,0x8000)!=0x88||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_EPT_MISCONFIG_TEST)
            if(model.root||l2_exits!=4||invept_count!=3||composed.tree.poisoned||composed.tree.generation!=4||
               *(uint64_t *)ram(f,0xc820)!=4||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_EPT_LARGE_TEST)
            if(model.root||l2_exits!=4||invept_count!=2||composed.tree.poisoned||composed.tree.generation!=3||
               *(uint64_t *)ram(f,0xc820)!=4||high_seen!=3||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_EPT_DYNAMIC_TEST)
            if(model.root||l2_exits!=12||invept_count!=2||contexts.poisoned||
               *(uint64_t *)ram(f,0xc820)!=12||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_EPT_CONTEXT_TEST)
            if(model.root||l2_exits!=8||invept_count!=2||contexts.poisoned||
               *(uint64_t *)ram(f,0xc820)!=8||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
            if(model.root||l2_exits!=4||invept_count!=2||composed.tree.poisoned||composed.tree.generation!=3||
               *(uint64_t *)ram(f,0xc820)!=4||high_seen!=15||sparse_fills<4||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_EPT_MAP_TEST)
            if(model.root||l2_exits!=4||invept_count!=2||composed.poisoned||composed.generation!=3||
               *(uint64_t *)ram(f,0xc820)!=4||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_FAILURE_TEST)
            if(model.root||l2_exits!=1||failed_entries!=1||vmx_guest_regs.rax!=0x4e31)break;
#else
#ifdef VMX_NESTED_ATOMIC_MULTIROUND_TEST
            if(model.root||l2_exits!=13||*(uint64_t *)ram(f,config_gpa+0x820)!=13||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
            if(model.root||l2_exits!=8||*(uint64_t *)ram(f,config_gpa+0x820)!=8||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
            if(model.root||l2_exits!=9||*(uint64_t *)ram(f,config_gpa+0x820)!=9||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
            if(model.root||l2_exits!=6||*(uint64_t *)ram(f,config_gpa+0x820)!=6||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_OWNER_AD_RESET_TEST) || defined(VMX_NESTED_OWNER_TYPED_TEST)
            if(model.root||l2_exits!=3||*(uint64_t *)ram(f,config_gpa+0x820)!=3||vmx_guest_regs.rax!=0x4e31)break;
#elif defined(VMX_NESTED_OWNER_CONTEXT_TEST)
#ifdef VMX_NESTED_OWNER_EVICT_TEST
            if(model.root||l2_exits!=6||vmx_guest_regs.rax!=0x4e31)break;
#else
            if(model.root||l2_exits!=5||vmx_guest_regs.rax!=0x4e31)break;
#endif
#else
            if(model.root||l2_exits!=2||vmx_guest_regs.rax!=0x4e31)break;
#endif
#endif
#if !defined(VMX_NESTED_EPT_TEST) && !defined(VMX_NESTED_FAILURE_TEST)
#ifdef VMX_NESTED_PF_ROUTE_TEST
            if(*(uint64_t *)ram(f,0xc820)!=4||*(uint64_t *)ram(f,0x8000)!=0x42)break;
#else
            /* Per-vCPU config GPA; CPU1 must not sample CPU0's live counter. */
#ifdef VMX_NESTED_ATOMIC_MULTIROUND_TEST
            if(*(uint64_t *)ram(f,config_gpa+0x820)!=13)break;
#elif defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
            if(*(uint64_t *)ram(f,config_gpa+0x820)!=8)break;
#elif defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
            if(*(uint64_t *)ram(f,config_gpa+0x820)!=9)break;
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
            if(*(uint64_t *)ram(f,config_gpa+0x820)!=6)break;
#elif defined(VMX_NESTED_OWNER_AD_RESET_TEST) || defined(VMX_NESTED_OWNER_TYPED_TEST)
            if(*(uint64_t *)ram(f,config_gpa+0x820)!=3)break;
#elif defined(VMX_NESTED_OWNER_CONTEXT_TEST)
#ifdef VMX_NESTED_OWNER_EVICT_TEST
            if(*(uint64_t *)ram(f,config_gpa+0x820)!=6)break;
#else
            if(*(uint64_t *)ram(f,config_gpa+0x820)!=5)break;
#endif
#else
            if(*(uint64_t *)ram(f,config_gpa+0x820)!=2)break;
#endif
#endif
#endif
#ifdef VMX_NESTED_INJECT_TEST
#ifdef VMX_NESTED_PF_ROUTE_TEST
            if(l1_event.armed||l1_deliveries!=2)break;
#else
            if(l1_event.armed||l1_deliveries!=1)break;
#endif
#endif
            success=true;
#ifdef VMX_NESTED_EPT_TEST
#ifdef VMX_NESTED_WT_TEST
            log_text("NESTED.N30_GUEST_PASS registered_WT=1 aliases=2 explicit_INVEPT=1\n");
#elif defined(VMX_NESTED_UC_CONTEXT_TEST)
            log_text("NESTED.N28_GUEST_PASS contexts=3 budget=2 eviction=1 type1=1 type2=1 refs=2\n");
#elif defined(VMX_NESTED_UC_TEST)
            log_text("NESTED.N27_GUEST_PASS registered_UC=1 aliases=2 explicit_INVEPT=1\n");
#elif defined(VMX_NESTED_MEMORY_TEST)
            log_text("NESTED.N17_PASS WB_IPAT=1 PAT_alias_WB_UC=1 hardware_nonWB=0\n");
#elif defined(VMX_NESTED_EPT_AD_CONTEXT_TEST)
#ifdef VMX_NESTED_EPT_AD_MODE_TEST
            log_text("NESTED.N16C_PASS same_root_modes=1 explicit_INVEPT=1 AD0_no_writeback=1 AD1_retrack=1\n");
#else
            log_text("NESTED.N16B_PASS context_owned_AD=1 type1_isolation=1 type2_retrack=1\n");
#endif
#elif defined(VMX_NESTED_EPT_AD_FAIL_TEST)
            log_text("NESTED.N16A_PASS nonpresent_prefix_A=1 terminal_unchanged=1 denied_write_D0=1 repaired_retry=1\n");
#elif defined(VMX_NESTED_EPT_AD_TEST)
            log_text("NESTED.N15_PASS A_all_levels=1 first_write_D=1 clear_INVEPT_retrack=1 paging_D=1\n");
#elif defined(VMX_NESTED_EPT_MISCONFIG_TEST)
            log_text("NESTED.N25_PASS EPT12_misconfig49=3 repaired_INVEPT_retry=3 L0_fault_not_reflected=1\n");
#elif defined(VMX_NESTED_EPT_LARGE_TEST)
            log_text("NESTED.N14_PASS EPT12_2MiB_1GiB=1 EPT02_4KiB=1 revoked_and_unmapped=1\n");
#elif defined(VMX_NESTED_EPT_DYNAMIC_TEST)
            log_text("NESTED.N13_PASS eight_contexts=1 budget=6 safe_eviction=1 type1=1 type2=1\n");
#elif defined(VMX_NESTED_EPT_CONTEXT_TEST)
            log_text("NESTED.N12_PASS isolated_contexts=2 type1=1 type2=1 stale_preserved=1\n");
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
            log_text("NESTED.N11_PASS sparse_four_levels=1 internal_fills=");log_hex(sparse_fills);
            log_text("NESTED.N11_EPOCHS=3 INVEPT=2 no_fixed_GPA_window=1\n");
#elif defined(VMX_NESTED_EPT_MAP_TEST)
            log_text("NESTED.N10_PASS warmed_multi_alias=2 revoked_write=1 unmapped_read=1 remap_new_backing=1 INVEPT=2\n");
#else
            log_text("NESTED.N2_PASS EPT12_composed INVEPT_retry_alias_proved\n");
#endif
#else
#ifdef VMX_NESTED_EVENT_TEST
#ifdef VMX_NESTED_INJECT_TEST
#ifdef VMX_NESTED_GP_INJECT_TEST
#ifdef VMX_NESTED_PF_INJECT_TEST
#ifdef VMX_NESTED_PF_ROUTE_TEST
            log_text("NESTED.N9_PASS PF_four_way=4 L0_inject=2 L1_reflect_inject=2 shared_CR2=1\n");
#else
            log_text("NESTED.N8_PASS PF_not_EPT=1 shared_CR2_L1_prepared=1 L0_CR2_restored=1\n");
#endif
#else
            log_text("NESTED.N7_PASS actual_GP28_reflected_reinjected=1 frame_error_popped=1 entryvalid_cleared=1\n");
#endif
#else
            log_text("NESTED.N6_PASS L0_reinject=1 L1_inject=1 L2_handler=2 entryvalid_cleared=1\n");
#endif
#else
            log_text("NESTED.N5_PASS L0_reinject=1 L1_reflect=1 L2_handler=1\n");
#endif
#elif defined(VMX_NESTED_FAILURE_TEST)
            log_text("NESTED.N4_PASS invalid_valid_failed_entry_distinct=1 corrected_launch=1\n");
#elif defined(VMX_NESTED_STATE_TEST)
            log_text("NESTED.N3_PASS GPR_RSP_RFLAGS_GDTR_IDTR_roundtrip=2\n");
#else
            log_text("NESTED.N1_PASS launch=1 resume=1 reflected_vmcall=2\n");
#endif
#endif
            break;
        }
        if(reason==10) {
            uint32_t leaf=vmx_guest_regs.rax;
            vmx_guest_regs.rax=vmx_guest_regs.rbx=vmx_guest_regs.rcx=vmx_guest_regs.rdx=0;
            if(leaf==0)vmx_guest_regs.rax=1;
            if(leaf==1){vmx_guest_regs.rcx=(1u<<31)|(1u<<5);vmx_guest_regs.rdx=1u<<6;}
            if(leaf==0x80000000)vmx_guest_regs.rax=0x80000008;
            if(leaf==0x80000001)vmx_guest_regs.rdx=1u<<29;
            if(leaf==0x80000008)vmx_guest_regs.rax=(profile.physical_bits?profile.physical_bits:48)|(48u<<8);
            if(length!=2||!put(0x681e,rip+2))break;
            continue;
        }
        if(reason==31) {
            uint64_t value;if(length!=2||!virtual_msr(&profile,(uint32_t)vmx_guest_regs.rcx,&value))break;
            vmx_guest_regs.rax=(uint32_t)value;vmx_guest_regs.rdx=(uint32_t)(value>>32);
            if(!put(0x681e,rip+2))break;
            continue;
        }
        uint64_t codepa,r[16],cs;
        if(!get(0x802,&cs)||(cs&3)||!walk(f,expected_cr3,rip,false,true,&codepa)||codepa<0x6000||codepa>=0x7000)break;
        regs(&vmx_guest_regs,r,rsp);struct nested_operand op;
        if(!nested_decode_operand(ram(f,codepa),4096-(codepa&4095),rip,r,reason,&op)||length!=op.length)break;
        struct nvmx_request request={.op=op.op,.flags=flags};uint64_t pa=0;uint8_t *memory=0;
        if(op.memory) {
            uint64_t info,qualification,segmentbase;
            if(!get(0x440e,&info)||!get(0x6400,&qualification)||
               !nested_memory_exit_matches(info,qualification,r,op.ea)||
               !get(0x6806+2*((info>>15)&7),&segmentbase)||segmentbase)break;
            if((op.ea&7)||!walk(f,expected_cr3,op.ea,op.op==NVMX_PTRST,false,&pa)||!(memory=ram(f,pa))||(pa&4095)>(op.op==NVMX_INVEPT?4080u:4088u))break;
            if(op.op!=NVMX_PTRST&&op.op!=NVMX_INVEPT) {
                request.operand=*(uint64_t *)memory;
                if(request.operand!=on_gpa&&request.operand!=object_gpa)break;
                request.region_revision=*(uint32_t *)ram(f,request.operand);
            }
        } else if(op.op==NVMX_READ||op.op==NVMX_WRITE) {request.field=r[op.regB];request.value=r[op.regA];}
#if defined(VMX_NESTED_LOCAL_INVEPT_TEST) || defined(VMX_NESTED_OWNER_CONTEXT_TEST)
        if(op.op==NVMX_INVEPT){
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
            break; /* No reset/clear/retrack authority in this immutable profile. */
#endif
            if(!model.root||(op.ea&15)||!memory)break;
            const uint64_t *descriptor=(const void *)memory;
            uint64_t type=r[op.regB],eptp12=descriptor[0];
            enum nested_local_invept_status check=
#ifdef VMX_NESTED_OWNER_CONTEXT_TEST
                nested_local_invept_check_allcontexts(type,eptp12,descriptor[1],48);
#else
                nested_local_invept_check(type,eptp12,descriptor[1],48);
#endif
            if(check==NESTED_LOCAL_INVEPT_UNSUPPORTED)break;
            if(check==NESTED_LOCAL_INVEPT_INVALID28){
                struct nvmx_result failed=nvmx_invept_result(&model,false,flags);
                if(!put(0x6820,failed.flags)||!put(0x681e,rip+length))break;
                continue;
            }
            struct nested_domain_invept_ticket ticket;
            if(!nested_domain_dispatch_local_invept_begin(vcpu->parallel->domain,vcpu->parallel->cpu_index,
               vcpu->parallel->domain_token,&model,(unsigned)type,type==1?(eptp12&~UINT64_C(4095)):0,&ticket))break;
            /* One root currently cached. Other valid roots are local no-ops;
             * never resolve their backing or treat INVEPT as a table read. */
#ifdef VMX_NESTED_OWNER_CONTEXT_TEST
            if(!nested_owner_sparse_invalidate(&vcpu->parallel->sparse,type,eptp12))break;
#else
            if((eptp12&~UINT64_C(4095))==0x10000&&
               !nested_owner_sparse_reset(&vcpu->parallel->sparse))break;
#endif
            if(!nested_domain_dispatch_local_invept_complete(vcpu->parallel->domain,vcpu->parallel->cpu_index,
               vcpu->parallel->domain_token,&model,ticket))break;
            vcpu->parallel->local_invalidations++;
            if(!vcpu->parallel->local_invalidated(vcpu->parallel))break;
            struct nvmx_result flushed=nvmx_invept_result(&model,true,flags);
            if(flushed.status!=NVMX_SUCCESS||!put(0x6820,flushed.flags)||!put(0x681e,rip+length))break;
            log_text("NESTED.LOCAL_INVEPT_OWNER=");log_hex(vcpu->parallel->cpu_index);
            log_text("NESTED.LOCAL_INVEPT_GENERATION=");log_hex(ticket.sequence);
            continue;
        }
#endif
#ifdef VMX_NESTED_EPT_TEST
        if(op.op==NVMX_INVEPT) {
#ifdef VMX_NESTED_EPT_AD_CONTEXT_TEST
#include "nested_ept_context_invept.inc"
#elif defined(VMX_NESTED_EPT_AD_TEST)
#ifdef VMX_NESTED_EPT_AD_FAIL_TEST
            if(!model.root||!composed_ready||composed.tree.poisoned||invept_count>=2||l2_exits!=(invept_count?3u:1u)||(op.ea&15))break;
#else
            if(!model.root||!composed_ready||composed.tree.poisoned||invept_count||l2_exits!=2||(op.ea&15))break;
#endif
            uint64_t *descriptor=(void *)memory;
            /* This profile never changes one root between AD0/AD1. Other
             * valid operands are unsupported, not guessed VMfail errors. */
            if(r[op.regB]!=1||descriptor[0]!=0x1005e||descriptor[1])break;
            if(!nested_ept_ad_hw_reset(&ad_cache,&composed))break;
            struct nvmx_result flushed=nvmx_invept_result(&model,true,flags);
            if(flushed.status!=NVMX_SUCCESS||!put(0x6820,flushed.flags)||!put(0x681e,rip+length))break;
            invept_count++;invept_done=true;
            log_text("NESTED.AD_CLEAR_INVEPT_DONE\n");continue;
#elif defined(VMX_NESTED_EPT_CONTEXT_TEST)
#include "nested_ept_context_invept.inc"
#else
#ifdef VMX_NESTED_UC_TEST
            if(!model.root||!composed_ready||composed.tree.poisoned||invept_count||l2_exits!=1||(op.ea&15))break;
#elif defined(VMX_NESTED_EPT_MISCONFIG_TEST)
            if(!model.root||!composed_ready||composed.tree.poisoned||
               !nested_misconfig_repair_pending(invept_count,l2_exits)||(op.ea&15))break;
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
            if(!model.root||!composed_ready||composed.tree.poisoned||invept_count>=2||
               l2_exits!=(invept_count?3u:1u)||(op.ea&15))break;
#elif defined(VMX_NESTED_EPT_MAP_TEST)
            if(!model.root||!composed_ready||composed.poisoned||invept_count>=2||
               l2_exits!=(invept_count?3u:1u)||(op.ea&15))break;
#else
            if(!model.root||!composed_ready||l2_exits!=1||invept_done||(op.ea&15))break;
#endif
            uint64_t *descriptor=(void *)memory;
            enum nested_invept_status check=nested_invept_check(r[op.regB],descriptor[0],descriptor[1]);
            if(check==NESTED_INVEPT_INVALID) {
                struct nvmx_result failed=nvmx_invept_result(&model,false,flags);
                if(!put(0x6820,failed.flags)||!put(0x681e,rip+length))break;
                continue;
            }
            /* Other architecturally valid EPTPs are outside this profile, not
             * mislabeled as architecturally invalid operands. */
            if(check!=NESTED_INVEPT_VALID)break;
#ifdef VMX_NESTED_EPT_SPARSE_TEST
            if(!nested_ept_sparse_hw_reset(&composed))break;
#else
            if(!nested_ept_hw_rebuild(&composed,f))break;
#endif
            struct nvmx_result flushed=nvmx_invept_result(&model,true,flags);
            if(flushed.status!=NVMX_SUCCESS||!put(0x6820,flushed.flags)||!put(0x681e,rip+length))break;
#ifdef VMX_NESTED_EPT_SPARSE_TEST
            invept_count++;log_text("NESTED.SPARSE_EPOCH_RESET=");log_hex(composed.tree.generation);
#elif defined(VMX_NESTED_EPT_MAP_TEST)
            invept_count++;
            log_text("NESTED.MAP_PUBLISHED_GENERATION=");log_hex(composed.generation);
#endif
            invept_done=true;log_text("NESTED.L1_INVEPT_PHYSICAL_EPT02_FLUSHED\n");continue;
#endif
        }
#endif
        struct nvmx_result result;
#ifdef VMX_NESTED_DOMAIN_TEST
        if(!nested_domain_dispatch_execute(vcpu->parallel->domain,vcpu->parallel->cpu_index,vcpu->parallel->domain_token,&model,request,&result))break;
#else
        result=nvmx_execute(&model,request);
#endif
        if(result.status==NVMX_ENTRY_REQUEST) {
#ifdef VMX_NESTED_UC_PAT_TEST
            uint64_t requested_pat,requested_cr0;
            if(!nvmx_peek(&model,0x2804,&requested_pat)||!nvmx_peek(&model,0x6800,&requested_cr0))break;
            if(!cache_registered_pat_entry(requested_pat,requested_cr0)){
                if(l2_exits==2&&invept_count==0&&requested_pat==UINT64_C(0x0106060606040706)&&
                   requested_cr0==profile.cr0&&*(uint64_t *)ram(f,0xc820)==2&&
                   *(volatile uint64_t *)ram(f,0xf000)==0x99&&!composed.tree.poisoned){
                    log_text("NESTED.N29_EXPECTED_UNSAFE_PAT_REFUSAL cached_leaf=1 no_physical_entry=1\n");
                    success=true;goto done;
                }
                break;
            }
#endif
#ifdef VMX_NESTED_FAILURE_TEST
            unsigned early=nested_early_failure(&model,&profile);
            if(early) {
                struct nvmx_result failed=nvmx_entry_fail(&model,early,flags);
                if(failed.status!=NVMX_FAIL_VALID||!put(0x6820,failed.flags)||!put(0x681e,rip+length))break;
                log_text("NESTED.VIRTUAL_EARLY_VMFAIL_VALID=");log_hex(early);continue;
            }
            enum nested_profile_status checked=nested_profile_check(&model,&profile);
            if(checked==NP_EXPECTED_GUEST_FAILURE&&!failed_entries&&!l2_exits)failure_armed=true;
            else if(checked!=NP_VALID)break;
#else
#ifdef VMX_NESTED_EPT_SPARSE_TEST
            uint64_t entry_flags=0,entry_rip=0;
            if(!nvmx_peek(&model,0x6820,&entry_flags)||!nvmx_peek(&model,0x681e,&entry_rip))break;
            log_text("NESTED.SPARSE_ENTRY_RFLAGS=");log_hex(entry_flags);
            log_text("NESTED.SPARSE_ENTRY_RIP=");log_hex(entry_rip);
            enum nested_profile_status sparse_checked=nested_profile_check(&model,&profile);
            if(sparse_checked!=NP_VALID){log_text("NESTED.SPARSE_PROFILE_REJECT=");log_hex(sparse_checked);break;}
#else
            if(nested_profile_check(&model,&profile)!=NP_VALID)break;
#endif
#endif
#ifdef VMX_NESTED_EPT_TEST
#ifdef VMX_NESTED_EPT_CONTEXT_TEST
            if(!nvmx_peek(&model,0x201a,&selected12)||
               nested_ept_contexts_select(&contexts,selected12)!=NESTED_CONTEXT_OK)break;
            struct nested_ept_sparse *active=nested_ept_contexts_active(&contexts);
            if(!active)break;
            composed.eptp=active->eptp;composed_ready=true;
#else
            if(!composed_ready) {
#ifdef VMX_NESTED_EPT_AD_TEST
                if(!nested_ept_ad_hw_init(&ad_cache,&composed,f,profile.physical_bits))break;
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
#ifdef VMX_NESTED_UC_TEST
                cache_registered_pin();uc_pinned=true;
#endif
                if(!nested_ept_sparse_hw_init(&composed,f,profile.physical_bits))break;
#ifdef VMX_NESTED_UC_TEST
                #ifdef VMX_NESTED_WT_TEST
                if(!nested_ept_sparse_set_typed_admission_wt(&composed.tree,cache_registered_admit,f))break;
#elif defined(VMX_NESTED_UC_PAT_TEST)
                if(!nested_ept_sparse_set_typed_admission_pat(&composed.tree,cache_registered_admit,f))break;
#else
                if(!nested_ept_sparse_set_typed_admission(&composed.tree,cache_registered_admit,f))break;
#endif
#endif
#else
                if(!nested_ept_hw_init(&composed,f))break;
#endif
                composed_ready=true;
            }
#ifdef VMX_NESTED_UC_PAT_TEST
            else if(!nested_uc_pat_cached_resume(l2_exits,invept_count))break;
#elif !defined(VMX_NESTED_EPT_AD_TEST)
            else if(!invept_done)break;
#else
            (void)invept_done;
#endif
#ifdef VMX_NESTED_EPT_SPARSE_TEST
            if(composed.tree.poisoned||!composed.tree.generation)break;
#elif defined(VMX_NESTED_EPT_MAP_TEST)
            if(composed.poisoned||!composed.generation)break;
#endif
#endif
#endif
#ifdef VMX_NESTED_EPT_CONTEXT_TEST
            (void)invept_done;
#endif
#ifdef VMX_NESTED_INJECT_TEST
            uint64_t einfo,eerror,elen,eblock,edebug,eflags;
            if(!nvmx_peek(&model,0x4016,&einfo)||!nvmx_peek(&model,0x4018,&eerror)||
               !nvmx_peek(&model,0x401a,&elen)||!nvmx_peek(&model,0x4824,&eblock)||
               !nvmx_peek(&model,0x6822,&edebug)||!nvmx_peek(&model,0x6820,&eflags))break;
            struct nested_entry_request er={.info=einfo,.error=eerror,.length=elen,
                .blocking=eblock,.pending_debug=edebug,.rflags=eflags,.vectoring=vectoring};
            if(internal_injection||l1_event.armed||!nested_entry_validate(&er))break;
            if(einfo&(UINT64_C(1)<<31)) {
                if(!nested_entry_arm(&l1_event,NESTED_ORIGIN_L1,&er))break;
#ifdef VMX_NESTED_GP_INJECT_TEST
#ifdef VMX_NESTED_PF_INJECT_TEST
                if(einfo!=0x80000b0e||vmx_guest_cr2!=0xf000)break;
                log_text("NESTED.L1_MOV_CR2_PREPARED_PF_INJECTION value=f000\n");
#else
                log_text("NESTED.L1_VM_ENTRY_GP_ARMED_SAME_RIP error=");log_hex(eerror);
#endif
#else
                log_text("NESTED.L1_VM_ENTRY_UD_ARMED_SAME_RIP\n");
#endif
            }
#endif
            if(!load(vmcs02))break;
#ifdef VMX_NESTED_DOMAIN_TEST
#ifdef VMX_NESTED_OWNER_CONTEXT_TEST
            uint64_t key;
            struct nested_owner_sparse *owner_cache=&vcpu->parallel->sparse;
            if(!nvmx_peek(&model,0x201a,&key)||!nested_owner_sparse_select(owner_cache,key))break;
            struct nested_ept_context_item *new_pin=nested_ept_contexts_pin_active(&owner_cache->contexts);
            if(!new_pin)break;
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
            uint64_t old_eptp=owner_cache->hardware_item?owner_cache->hardware_item->tree.eptp:0;
            if(!tr_overlap_switch_step(&owner_cache->overlap_switch,1,new_pin->tree.eptp,old_eptp))break;
#endif
            if(!put(0x201a,new_pin->tree.eptp))break; /* both references retained on failure */
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
            if(!tr_overlap_switch_step(&owner_cache->overlap_switch,2,new_pin->tree.eptp,old_eptp))break;
#endif
            if(owner_cache->hardware_item&&!nested_ept_contexts_unpin(&owner_cache->contexts,owner_cache->hardware_item))break;
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
            if(!tr_overlap_switch_step(&owner_cache->overlap_switch,3,new_pin->tree.eptp,old_eptp))break;
#endif
            owner_cache->hardware_item=new_pin;
            vcpu->parallel->composed_eptp=new_pin->tree.eptp;
#else
            if(!put(0x201a,vcpu->parallel->composed_eptp))break;
#endif
#endif
#ifdef VMX_NESTED_INJECT_TEST
            if(!put(0x4018,eerror)||!put(0x401a,elen)||!put(0x4016,einfo))break;
#endif
#ifdef VMX_NESTED_EVENT_TEST
            uint64_t bitmap12;
            if(!nvmx_peek(&model,0x4004,&bitmap12)||!put(0x4004,nested_exception_bitmap(L0_EVENT_BITMAP,(uint32_t)bitmap12)))break;
#ifdef VMX_NESTED_PF_ROUTE_TEST
            /* Physical02 catches every PF regardless of virtual12's filter. */
            if(!put(0x4006,0)||!put(0x4008,0))break;
#endif
#endif
#ifdef VMX_NESTED_EPT_TEST
#ifdef VMX_NESTED_EPT_CONTEXT_TEST
            /* VMCS02 still references the old pinned root until VMWRITE
             * succeeds. Failed switch retains BOTH pins until VMCLEAR. */
            struct nested_ept_context_item *new_context=nested_ept_contexts_pin_active(&contexts);
            if(!new_context||!put(0x201a,composed.eptp))break;
            if(hardware_context&&!nested_ept_contexts_unpin(&contexts,hardware_context))break;
            hardware_context=new_context;
#else
            if(!put(0x201a,composed.eptp))break;
#endif
#endif
            for(size_t i=0;i<count;i++)if(((table[i].field>>10)&3)==2) {
                uint64_t value;if(!nvmx_peek(&model,table[i].field,&value)||!put(table[i].field,value))goto done;
            }
            in_l2=true;continue;
        }
        if(result.status!=NVMX_SUCCESS&&result.status!=NVMX_FAIL_VALID&&result.status!=NVMX_FAIL_INVALID)break;
        if(result.status==NVMX_SUCCESS) {
            if(op.op==NVMX_READ&&!setreg(&vmx_guest_regs,op.regA,result.value))break;
            if(op.op==NVMX_PTRST)*(uint64_t *)memory=result.value;
        }
        if(!put(0x6820,result.flags)||!put(0x681e,rip+length))break;
    }
done:
#if defined(VMX_NESTED_ATOMIC_RETRACK_TEST) || defined(VMX_NESTED_SOURCE_REPLACE_TEST)
    /* An uncertain physical handoff must never fall through to VMCLEAR,
     * metadata unpin or pool destruction. Retain even on pre-round failure. */
    if(!success)panic("N42B FAILURE RETAIN BEFORE CLEANUP");
#endif
#ifdef VMX_NESTED_DOMAIN_TEST
    if(!success)nested_domain_dispatch_markfail(vcpu->parallel->domain);
#endif
#ifdef VMX_NESTED_VCPU_TEST
    if(vcpu->owner_apic!=context_cpu()||vcpu->busy)panic("NESTED CTX OWNER/PENDING RETAIN");
#endif
    if(vmcs02&&owned02&&!clear(vmcs02))panic("NESTED VMCS02 CLEAR FAILED");
#ifdef VMX_NESTED_VCPU_TEST
    if(vcpu->vmcs02&&!vcpu->cleared02&&!nested_vcpu_clear(vcpu,context_cpu(),vmcs02))panic("NESTED CTX CLEAR02 POLICY");
#endif
#ifdef VMX_NESTED_DOMAIN_TEST
#ifdef VMX_NESTED_OWNER_SPARSE_TEST
    if(vcpu->parallel->sparse.initialized&&!nested_owner_sparse_stop(&vcpu->parallel->sparse))
        panic("OWNER SPARSE DESTROY RETAIN");
#else
    if(!domain_flush(vcpu->parallel->composed_eptp))panic("DOMAIN EPT02 FLUSH RETAIN");
#endif
#endif
    if(!load(vmcs01))panic("NESTED VMCS01 RESTORE FAILED");
#ifdef VMX_NESTED_EPT_TEST
#ifdef VMX_NESTED_EPT_CONTEXT_TEST
    /* All physical VMCS02 references were cleared above. */
    nested_ept_contexts_release_all_after_clear(&contexts);
    if(!nested_ept_contexts_destroy(&contexts))panic("CONTEXT INVALIDATION FAILED RETAIN");
#endif
#if defined(VMX_NESTED_EPT_AD_TEST) && !defined(VMX_NESTED_EPT_AD_CONTEXT_TEST)
    nested_ept_ad_hw_destroy(&ad_cache,&composed);
#else
    nested_ept_hw_destroy(&composed);
#endif
#endif
#ifdef VMX_NESTED_UC_TEST
    if(uc_live){
        if(!clear(vmcs01))panic("UC VMCS01 CLEAR FAILED RETAIN");
        if(uc_pinned)cache_registered_unpin_after_destroy();
        cache_registered_finish(f);
        #ifdef VMX_NESTED_WT_TEST
        if(success)log_text("NESTED.N30_PASS WT01_WT02_hostWT=1 IPAT1=1 restored_WB=1\n");
#elif defined(VMX_NESTED_UC_PAT_TEST)
        if(success)log_text("NESTED.N29_PASS safe_IPAT0_three_aliases=1 unsafe_PAT_rejected=1 restored_WB=1\n");
#elif defined(VMX_NESTED_UC_CONTEXT_TEST)
        if(success)log_text("NESTED.N28_PASS all_contexts_destroyed=1 refs=0 restored_WB=1\n");
#else
        if(success)log_text("NESTED.N27_PASS UC01_UC02_hostUC=1 restored_WB=1\n");
#endif
    }
#endif
    #ifndef VMX_NESTED_PARALLEL_TEST
    if(vmcs02&&!pmm_free_page(vmcs02))panic("NESTED VMCS02 FREE FAILED");
#endif
#ifdef VMX_NESTED_VCPU_TEST
    if(success){log_text("NESTED.N32A_CONTEXT_PASS GPR_sentinels=2 per_call_host_CR2=1 owner=");log_hex(vcpu->owner_apic);log_text("NESTED.CTX_ENTRIES=");log_hex(vcpu->entries);}
#endif
    if(!success)log_text("NESTED.N1_FAILED_OR_UNSUPPORTED\n");
    return success;
}
