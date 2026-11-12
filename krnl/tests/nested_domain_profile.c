#include "../nested_profile.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static struct nvmx_result run(struct nvmx *s, enum nvmx_op op,
                              uint64_t field, uint64_t value) {
    return nvmx_execute(s, (struct nvmx_request){
        .op=op, .operand=op==NVMX_ON?s->on_region:s->vmcs_region,
        .region_revision=NVMX_REVISION, .field=field, .value=value, .flags=2
    });
}
static void check(struct nvmx *s, const struct nested_profile *p,
                  enum nested_profile_status expected) {
    assert(run(s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nested_profile_check(s,p)==expected);
    assert(nvmx_entry_cancel(s));
}
static uint64_t read_field(struct nvmx *s,uint64_t field) {
    uint64_t value;
    assert(nvmx_peek(s,field,&value));
    return value;
}
int main(void) {
    struct nested_profile p={
        .cr0=0x80010033,.cr4=0x2020,.efer=0x500,
        .pat=0x0007040600070406,.l2_entry=0x6100,
        .l2_resume=0x6103,.l1_exit=0x6200,
        .nested_ept=true,.shared_domain=true
    };
    struct nested_field_value table[NVMX_FIELDS];
    for(unsigned cpu=0;cpu<2;cpu++) {
        p.domain_cpu=cpu;
        size_t count=nested_profile_table(&p,table,NVMX_FIELDS);
        assert(count);
        struct nvmx s;
        uint64_t bias=cpu?0x20000:0;
        assert(nvmx_init(&s,0xd000+bias,0xe000+bias,48));
        assert(run(&s,NVMX_ON,0,0).status==NVMX_SUCCESS);
        assert(run(&s,NVMX_CLEAR,0,0).status==NVMX_SUCCESS);
        assert(run(&s,NVMX_PTRLD,0,0).status==NVMX_SUCCESS);
        for(size_t i=0;i<count;i++)
            assert(run(&s,NVMX_WRITE,table[i].field,table[i].value).status==NVMX_SUCCESS);
        assert(read_field(&s,0x6802)==0x1000+bias);
        assert(read_field(&s,0x6c02)==0x1000+bias);
        assert(read_field(&s,0x6816)==0x5000); /* GDT GVA, not owner GPA */
        assert(read_field(&s,0x6818)==0x5800);
        assert(read_field(&s,0x6c0a)==0x5100);
        assert(read_field(&s,0x681c)==0x9000);
        assert(read_field(&s,0x6c14)==0x8000);
        assert(read_field(&s,0x201a)==0x1001e);
        assert(read_field(&s,0x4002)==(1u<<31));
        assert(read_field(&s,0x401e)==2);
        check(&s,&p,NP_VALID);
        const uint64_t fields[]={0x6802,0x6c02,0x201a,0x201a,0x201a,0x401e,0x4002};
        const uint64_t values[]={cpu?0x1000:0x21000,cpu?0x1000:0x21000,
                                 0x1401e,0x10018,0x1005e,0,0};
        const enum nested_profile_status errors[]={NP_GUEST,NP_HOST,NP_CONTROLS,
            NP_CONTROLS,NP_CONTROLS,NP_CONTROLS,NP_CONTROLS};
        for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++) {
            uint64_t old=read_field(&s,fields[i]);
            assert(run(&s,NVMX_WRITE,fields[i],values[i]).status==NVMX_SUCCESS);
            check(&s,&p,errors[i]);
            assert(run(&s,NVMX_WRITE,fields[i],old).status==NVMX_SUCCESS);
        }
        p.domain_contexts=true;
        assert(run(&s,NVMX_WRITE,0x201a,0x1401e).status==NVMX_SUCCESS);
        check(&s,&p,NP_VALID);
        assert(run(&s,NVMX_WRITE,0x201a,0x1801e).status==NVMX_SUCCESS);
        check(&s,&p,NP_CONTROLS);
        p.domain_eviction=true;check(&s,&p,NP_VALID);
        p.domain_eviction=false;
        assert(run(&s,NVMX_WRITE,0x201a,0x1001e).status==NVMX_SUCCESS);
        p.domain_contexts=false;
        p.domain_typed_uc=true;check(&s,&p,NP_VALID);
        p.domain_contexts=true;assert(!nested_profile_table(&p,table,NVMX_FIELDS));
        p.domain_typed_contexts=true;check(&s,&p,NP_VALID);
        assert(run(&s,NVMX_WRITE,0x201a,0x1401e).status==NVMX_SUCCESS);check(&s,&p,NP_VALID);
        assert(run(&s,NVMX_WRITE,0x201a,0x1801e).status==NVMX_SUCCESS);check(&s,&p,NP_CONTROLS);
        assert(run(&s,NVMX_WRITE,0x201a,0x1005e).status==NVMX_SUCCESS);check(&s,&p,NP_CONTROLS);
        p.domain_eviction=true;assert(!nested_profile_table(&p,table,NVMX_FIELDS));p.domain_eviction=false;
        p.domain_typed_dynamic=true;
        assert(!nested_profile_table(&p,table,NVMX_FIELDS)); /* explicit eviction required */
        p.domain_eviction=true;
        const uint64_t dynamic_keys[]={0x1001e,0x1401e,0x1801e};
        for(unsigned k=0;k<3;k++) {
            assert(run(&s,NVMX_WRITE,0x201a,dynamic_keys[k]).status==NVMX_SUCCESS);
            check(&s,&p,NP_VALID);
        }
        const uint64_t rejected_keys[]={0x1c01e,0x1005e,0x1405e,0x1805e,0x1001c,0x18018};
        for(unsigned k=0;k<sizeof rejected_keys/sizeof rejected_keys[0];k++) {
            assert(run(&s,NVMX_WRITE,0x201a,rejected_keys[k]).status==NVMX_SUCCESS);
            check(&s,&p,NP_CONTROLS);
        }
        const size_t required[]={offsetof(struct nested_profile,shared_domain),
            offsetof(struct nested_profile,domain_typed_uc),offsetof(struct nested_profile,domain_contexts),
            offsetof(struct nested_profile,domain_typed_contexts),offsetof(struct nested_profile,domain_eviction)};
        for(unsigned k=0;k<sizeof required/sizeof required[0];k++) {
            struct nested_profile bad=p;
            *(bool *)((unsigned char *)&bad+required[k])=false;
            assert(!nested_profile_table(&bad,table,NVMX_FIELDS));
        }
        p.domain_atomic_ad=true;assert(!nested_profile_table(&p,table,NVMX_FIELDS));p.domain_atomic_ad=false;
        p.memory_types=true;assert(!nested_profile_table(&p,table,NVMX_FIELDS));p.memory_types=false;
        assert(run(&s,NVMX_WRITE,0x201a,0x1001e).status==NVMX_SUCCESS);
        assert(run(&s,NVMX_WRITE,0x6820,0x46).status==NVMX_SUCCESS);check(&s,&p,NP_GUEST);
        assert(run(&s,NVMX_WRITE,0x6820,2).status==NVMX_SUCCESS);check(&s,&p,NP_VALID);
        p.domain_typed_dynamic=false;
        assert(!nested_profile_table(&p,table,NVMX_FIELDS)); /* no change to legacy typed-context profile */
        p.domain_eviction=false;
        p.domain_contexts=false;assert(!nested_profile_table(&p,table,NVMX_FIELDS));
        p.domain_contexts=true;p.domain_typed_uc=false;assert(!nested_profile_table(&p,table,NVMX_FIELDS));
        p.domain_typed_uc=true;p.domain_typed_contexts=false;p.domain_contexts=false;
        assert(run(&s,NVMX_WRITE,0x201a,0x1001e).status==NVMX_SUCCESS);
        p.domain_atomic_ad=true;assert(!nested_profile_table(&p,table,NVMX_FIELDS));p.domain_atomic_ad=false;
        p.shared_domain=false;assert(!nested_profile_table(&p,table,NVMX_FIELDS));p.shared_domain=true;
        p.domain_typed_uc=false;
        p.domain_atomic_ad=true;
        check(&s,&p,NP_CONTROLS); /* AD0 no longer matches */
        assert(run(&s,NVMX_WRITE,0x201a,0x1005e).status==NVMX_SUCCESS);
        check(&s,&p,NP_VALID);
        p.domain_contexts=true;assert(!nested_profile_table(&p,table,NVMX_FIELDS));p.domain_contexts=false;
        p.domain_eviction=true;assert(!nested_profile_table(&p,table,NVMX_FIELDS));p.domain_eviction=false;
        p.shared_domain=false;assert(!nested_profile_table(&p,table,NVMX_FIELDS));p.shared_domain=true;
        p.ept_ad=true;assert(!nested_profile_table(&p,table,NVMX_FIELDS));p.ept_ad=false;
        p.domain_atomic_ad=false;check(&s,&p,NP_CONTROLS);
        assert(run(&s,NVMX_WRITE,0x201a,0x1001e).status==NVMX_SUCCESS);
        assert(run(&s,NVMX_WRITE,0x681e,p.l2_resume).status==NVMX_SUCCESS);
        check(&s,&p,NP_VALID);
    }
    p.domain_cpu=2;
    assert(!nested_profile_table(&p,table,NVMX_FIELDS));
    p.domain_cpu=0;p.nested_ept=false;
    assert(!nested_profile_table(&p,table,NVMX_FIELDS));
    p.nested_ept=true;
    const size_t extensions[]={
#define EXT(f) offsetof(struct nested_profile,f)
        EXT(ept_map),EXT(ept_sparse),EXT(ept_contexts),EXT(ept_dynamic),
        EXT(ept_large),EXT(ept_ad),EXT(ept_ad_modes),EXT(memory_types),
        EXT(uc_contexts),EXT(uc_pat),EXT(state_roundtrip),EXT(entry_failure_probe),
        EXT(exception_routing),EXT(entry_injection),EXT(gp_injection),
        EXT(pf_injection),EXT(pf_routing)
#undef EXT
    };
    for(unsigned i=0;i<sizeof(extensions)/sizeof(extensions[0]);i++) {
        struct nested_profile bad=p;
        *(bool *)((unsigned char *)&bad+extensions[i])=true;
        assert(!nested_profile_table(&bad,table,NVMX_FIELDS));
    }
    /* New owner field is inactive for legacy callers. */
    p.shared_domain=false;p.domain_cpu=123;
    assert(nested_profile_table(&p,table,NVMX_FIELDS));
    puts("PASS restricted shared-domain owner CR3, fixed GVA and WB AD0 EPTP");
    return 0;
}
