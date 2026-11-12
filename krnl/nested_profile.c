#include "nested_profile.h"
static bool contract(const struct nested_profile *p) {
    if(p && p->domain_root_allowed && ((!p->domain_typed_dynamic&&!p->domain_atomic_catalog)||!p->domain_initial_eptp))return false;
    if(p && p->domain_atomic_catalog && (!p->shared_domain||!p->domain_contexts||!p->domain_eviction||
       p->domain_atomic_ad||p->domain_typed_uc||p->domain_typed_contexts||p->domain_typed_dynamic||
       !p->domain_root_allowed||p->domain_initial_eptp!=0x1005e))return false;
    if(p && p->domain_typed_dynamic && (!p->domain_typed_uc||!p->shared_domain||
       !p->domain_typed_contexts||!p->domain_contexts||!p->domain_eviction||p->domain_atomic_ad))return false;
    if(p && p->domain_typed_contexts && (!p->domain_typed_uc||!p->domain_contexts||
       (p->domain_eviction&&!p->domain_typed_dynamic)||p->domain_atomic_ad))return false;
    if(p && p->domain_typed_uc && (!p->shared_domain||p->domain_atomic_ad||
       (p->domain_contexts&&!p->domain_typed_contexts)||(p->domain_eviction&&!p->domain_typed_dynamic)))return false;
    if(p && p->domain_atomic_ad && (!p->shared_domain||p->domain_contexts||p->domain_eviction))return false;
    if(p && p->domain_eviction && !p->domain_contexts)return false;
    if(p && p->domain_contexts && !p->shared_domain)return false;
    if(p && p->shared_domain &&
       (p->domain_cpu>1 || !p->nested_ept || p->ept_map || p->ept_sparse ||
        p->ept_contexts || p->ept_dynamic || p->ept_large || p->ept_ad ||
        p->ept_ad_modes || p->memory_types || p->uc_contexts || p->uc_pat ||
        p->state_roundtrip || p->entry_failure_probe || p->exception_routing ||
        p->entry_injection || p->gp_injection || p->pf_injection || p->pf_routing))
        return false;
    return p && (!p->exception_routing||(p->l2_finish>=0x6000&&p->l2_finish<0x7000)) && p->l2_entry>=0x6000&&p->l2_entry<0x7000 &&
        p->l2_resume>=0x6000&&p->l2_resume<0x7000 && p->l2_entry!=p->l2_resume &&
        p->l1_exit>=0x6000&&p->l1_exit<0x7000 &&
        (p->cr0&UINT64_C(0x80000001))==UINT64_C(0x80000001) &&
        (p->cr4&UINT64_C(0x2020))==UINT64_C(0x2020) && !(p->cr4&(1u<<12)) &&
        (p->efer&0x500)==0x500;
}
size_t nested_profile_table(const struct nested_profile *p,struct nested_field_value *out,size_t capacity) {
    if(!contract(p)||!out||capacity<NVMX_FIELDS) return 0;
    const uint64_t guest_cr3=p->shared_domain && p->domain_cpu==1?0x21000:0x1000;
    size_t n=0;
#define ADD(f,v) out[n++]=(struct nested_field_value){f,v}
    ADD(0x4000,0);ADD(0x4002,p->nested_ept?(1u<<31):0);ADD(0x4004,p->pf_routing?0x4000:0);ADD(0x4006,p->pf_routing?2:0);ADD(0x4008,p->pf_routing?2:0);ADD(0x400a,0);
    ADD(0x400c,(1u<<9)|(p->memory_types?((1u<<18)|(1u<<19)):0));ADD(0x400e,0);ADD(0x4010,0);ADD(0x4012,(1u<<9)|(p->memory_types?(1u<<14):0));ADD(0x4014,0);
    ADD(0x4016,0);ADD(0x4018,0);ADD(0x401a,0);ADD(0x401e,p->nested_ept?2:0);
    if(p->nested_ept) {ADD(0x201a,p->domain_root_allowed?p->domain_initial_eptp:(p->ept_ad||p->domain_atomic_ad)?0x1005e:0x1001e);};
    ADD(0x6000,0);ADD(0x6002,0);ADD(0x6004,p->cr0);ADD(0x6006,p->cr4);
    ADD(0x2800,UINT64_MAX);ADD(0x2802,0);ADD(0x2804,p->uc_pat?UINT64_C(0x0606060606040706):p->memory_types?UINT64_C(0x0606060606060006):p->pat);ADD(0x2806,p->efer);
    ADD(0x2c00,p->pat);ADD(0x2c02,p->efer);
    for(unsigned i=0;i<8;i++) {
        uint64_t sel=16,limit=UINT32_MAX,ar=0xc093,base=0;
        if(i==1){sel=8;ar=0xa09b;}
        if(i==6){sel=0;limit=0;ar=1u<<16;}
        if(i==7){sel=24;limit=103;ar=0x8b;base=0x5100;}
        ADD(0x800+2*i,sel);ADD(0x4800+2*i,limit);ADD(0x4814+2*i,ar);ADD(0x6806+2*i,base);
    }
    ADD(0x4810,39);ADD(0x4812,p->pf_injection?239:(p->gp_injection?223:(p->exception_routing?111:0)));ADD(0x6816,0x5000);ADD(0x6818,0x5800);
    ADD(0x4824,0);ADD(0x4826,0);ADD(0x6822,0);
    ADD(0x6800,p->cr0);ADD(0x6802,guest_cr3);ADD(0x6804,p->cr4);ADD(0x681a,0x400);
    ADD(0x681c,0x9000);ADD(0x681e,p->l2_entry);ADD(0x6820,2);
    ADD(0x482a,0);ADD(0x6824,0);ADD(0x6826,0);
    for(unsigned i=0;i<7;i++){ADD(0xc00+2*i,i==1?8:(i==6?24:16));}
    ADD(0x6c00,p->cr0);ADD(0x6c02,guest_cr3);ADD(0x6c04,p->cr4);
    ADD(0x6c06,0);ADD(0x6c08,0);ADD(0x6c0a,0x5100);ADD(0x6c0c,0x5000);ADD(0x6c0e,0x5800);
    ADD(0x4c00,0);ADD(0x6c10,0);ADD(0x6c12,0);ADD(0x6c14,0x8000);ADD(0x6c16,p->l1_exit);
#undef ADD
    return n;
}
enum nested_profile_status nested_profile_check(const struct nvmx *s,const struct nested_profile *p) {
    struct nested_field_value table[NVMX_FIELDS];
    size_t count=nested_profile_table(p,table,NVMX_FIELDS);
    if(!count||!s||!s->root||s->current==NVMX_NONE||!s->prepared||s->running) return NP_INVALID_CONTRACT;
    bool expected_failure=false;
    for(size_t i=0;i<count;i++) {
        uint64_t value,field=table[i].field;
        if(!nvmx_peek(s,field,&value)) return NP_INVALID_CONTRACT;
        if(p->domain_root_allowed&&field==0x201a){
            if(p->domain_atomic_catalog&&(value&4095)!=0x5e)return NP_CONTROLS;
            if(p->domain_root_allowed(p->domain_root_context,value))continue;
            return NP_CONTROLS;
        }
        if(p->uc_contexts&&field==0x201a&&(value==0x1001e||value==0xb01e||value==0xc01e))continue;
        if(p->ept_dynamic&&field==0x201a&&value>=0x1001e&&value<=0x2c01e&&((value-0x1001e)%0x4000)==0)continue;
        if(p->ept_ad_modes&&field==0x201a&&(value==0x1001e||value==0x1005e))continue;
        if(p->domain_eviction&&field==0x201a&&value==0x1801e)continue;
        if(p->domain_contexts&&field==0x201a&&(value==0x1001e||value==0x1401e))continue;
        if(p->ept_contexts&&field==0x201a&&(value==(p->ept_ad?0x1005e:0x1001e)||value==(p->ept_ad?0x1405e:0x1401e)))continue;
        if(field==0x681e && value==p->l2_resume) continue;
        if(p->domain_typed_uc&&field==0x681e&&value==p->l2_finish&&value>=0x6000&&value<0x7000)continue;
        if((p->ept_map||p->ept_sparse)&&field==0x681e&&value>=0x6000&&value<0x7000)continue;
        if(p->pf_routing&&((field==0x4004&&(value==0||value==0x4000))||
           ((field==0x4006||field==0x4008)&&value<=UINT32_MAX)||
           (field==0x681e&&value>=0x6000&&value<0x7000)||
           (field==0x4018&&value<=0xf)))continue;
        if(p->pf_injection&&((field==0x4016&&value==0x80000b0e)||
           (field==0x4004&&value==0x4000)))continue;
        if(p->gp_injection&&((field==0x4016&&value==0x80000b0d)||
           (field==0x4018&&value<=0xffff)||(field==0x4004&&value==0x2000)||
           (field==0x681e&&value==p->l2_gp_fault)))continue;
        if(p->entry_injection&&field==0x4016&&
           (!(value&UINT64_C(0x80000000))||value==0x80000306))continue;
        if(p->exception_routing&&((field==0x681e&&value==p->l2_finish)||
           (field==0x4004&&value==0x40)||(field==0x6820&&value==0x10002)))continue;
        if(p->state_roundtrip&&((field==0x681c&&value==0x8700)||(field==0x6820&&value==0x46)))continue;
        /* Sparse payload uses real CMP instructions before EPT faults.
         * Preserve arithmetic condition flags and RF, never normalize them.
         * This trusted IF=0 profile still excludes TF/DF/IOPL/VM/reserved bits. */
        if(p->ept_sparse&&field==0x6820&&(value&2)&&
           !(value&~UINT64_C(0x108d7)))continue;
        if(p->nested_ept&&field==0x6820&&value==0x10002) continue; /* preserve fault RF */
        if(p->entry_failure_probe&&field==0x6820&&value==0) {expected_failure=true;continue;}
        if(value==table[i].value) continue;
        unsigned type=(field>>10)&3;
        if(type==0) return NP_CONTROLS;
        if(type==3) return NP_HOST;
        return NP_GUEST;
    }
    return expected_failure?NP_EXPECTED_GUEST_FAILURE:NP_VALID;
}

/* Only two proven-invalid early fields, with every other profile field valid.
 * This is not a generic VM-entry validator and never controls physical host. */
unsigned nested_early_failure(const struct nvmx *s,const struct nested_profile *p) {
    const uint64_t field[]={0x4002,0xc02},invalid[]={1,0},fixed[]={0,8};
    for(unsigned i=0;i<2;i++) {
        uint64_t value;if(!nvmx_peek(s,field[i],&value)||value!=invalid[i])continue;
        struct nvmx copy=*s;copy.prepared=false;
        struct nvmx_result r=nvmx_execute(&copy,(struct nvmx_request){.op=NVMX_WRITE,.field=field[i],.value=fixed[i]});
        if(r.status!=NVMX_SUCCESS)continue;
        copy.prepared=true;
        if(nested_profile_check(&copy,p)==NP_VALID)return i?8:7;
    }
    return 0;
}
