#include "../nested_profile.h"
#include <assert.h>
#include <stdio.h>
static struct nvmx_result exec(struct nvmx *s,enum nvmx_op op,uint64_t field,uint64_t value) {
    return nvmx_execute(s,(struct nvmx_request){.op=op,.operand=op==NVMX_ON?0xd000:0xe000,.region_revision=NVMX_REVISION,.field=field,.value=value,.flags=2});
}
int main(void) {
    uint64_t en=(1u<<2)|(1u<<14)|(1u<<15),ex=(1u<<2)|(1u<<18)|(1u<<20);
    assert(nested_state_save_controls(en,ex,2));
    assert(!nested_state_save_controls(en,ex&~(1u<<20),2));
    assert(!nested_state_save_controls(en,ex&~(1u<<18),2));
    assert(!nested_state_save_controls(en&~4u,ex,2));
    assert(!nested_state_save_controls(en,ex,6));
    struct nested_profile p={.cr0=0x80010033,.cr4=0x2020,.efer=0x500,.pat=0x0007040600070406,.l2_entry=0x6100,.l2_resume=0x6103,.l1_exit=0x6200};
    struct nested_field_value table[NVMX_FIELDS];
    size_t count=nested_profile_table(&p,table,NVMX_FIELDS);
    assert(count&&count<=NVMX_FIELDS);
    assert(!nested_profile_table(&p,table,NVMX_FIELDS-1));
    struct nested_profile bad=p;bad.l1_exit=UINT64_C(0xffff800000006200);
    assert(!nested_profile_table(&bad,table,NVMX_FIELDS));
    struct nvmx s;assert(nvmx_init(&s,0xd000,0xe000,48));
    assert(exec(&s,NVMX_ON,0,0).status==NVMX_SUCCESS);
    assert(exec(&s,NVMX_CLEAR,0,0).status==NVMX_SUCCESS);
    assert(exec(&s,NVMX_PTRLD,0,0).status==NVMX_SUCCESS);
    for(size_t i=0;i<count;i++) assert(exec(&s,NVMX_WRITE,table[i].field,table[i].value).status==NVMX_SUCCESS);
    assert(exec(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nested_profile_check(&s,&p)==NP_VALID);assert(nvmx_entry_cancel(&s));
    uint64_t fields[]={0x4002,0x6c02,0x681c};
    enum nested_profile_status errors[]={NP_CONTROLS,NP_HOST,NP_GUEST};
    for(unsigned i=0;i<3;i++) {
        uint64_t old;assert(nvmx_peek(&s,fields[i],&old));
        assert(exec(&s,NVMX_WRITE,fields[i],0xdeadbeef).status==NVMX_SUCCESS);
        assert(exec(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
        assert(nested_profile_check(&s,&p)==errors[i]);assert(nvmx_entry_cancel(&s));
        assert(exec(&s,NVMX_WRITE,fields[i],old).status==NVMX_SUCCESS);
    }
    assert(exec(&s,NVMX_WRITE,0x681e,p.l2_resume).status==NVMX_SUCCESS);
    assert(exec(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nested_profile_check(&s,&p)==NP_VALID);
    assert(nvmx_entry_cancel(&s));
    p.state_roundtrip=true;
    assert(exec(&s,NVMX_WRITE,0x681c,0x8700).status==NVMX_SUCCESS);
    assert(exec(&s,NVMX_WRITE,0x6820,0x46).status==NVMX_SUCCESS);
    assert(exec(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nested_profile_check(&s,&p)==NP_VALID);assert(nvmx_entry_cancel(&s));
    assert(exec(&s,NVMX_WRITE,0x6820,0x146).status==NVMX_SUCCESS);
    assert(exec(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nested_profile_check(&s,&p)==NP_GUEST);
    assert(nvmx_entry_cancel(&s));
    p.state_roundtrip=false;p.nested_ept=true;p.ept_sparse=true;
    count=nested_profile_table(&p,table,NVMX_FIELDS);assert(count);
    for(size_t i=0;i<count;i++)assert(exec(&s,NVMX_WRITE,table[i].field,table[i].value).status==NVMX_SUCCESS);
    const uint64_t flags[]={2,0x46,0x10046,0x108d7,0,0x10044,0x146,0x246,0x446,0x3046,0x20046,UINT64_C(1)<<63};
    for(unsigned i=0;i<sizeof(flags)/sizeof(flags[0]);i++) {
        assert(exec(&s,NVMX_WRITE,0x6820,flags[i]).status==NVMX_SUCCESS);
        assert(exec(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
        assert(nested_profile_check(&s,&p)==(i<4?NP_VALID:NP_GUEST));
        uint64_t actual;assert(nvmx_peek(&s,0x6820,&actual)&&actual==flags[i]);
        assert(nvmx_entry_cancel(&s));
    }
    p.nested_ept=false;p.ept_sparse=false;
    p.state_roundtrip=false;p.exception_routing=true;p.entry_injection=true;
    p.gp_injection=true;p.pf_injection=true;p.pf_routing=true;
    p.l2_finish=0x6300;p.l2_gp_fault=0x6310;
    count=nested_profile_table(&p,table,NVMX_FIELDS);assert(count);
    for(size_t i=0;i<count;i++)assert(exec(&s,NVMX_WRITE,table[i].field,table[i].value).status==NVMX_SUCCESS);
    uint64_t mask,match;
    assert(nvmx_peek(&s,0x4006,&mask)&&mask==2);
    assert(nvmx_peek(&s,0x4008,&match)&&match==2);
    for(unsigned bits=0;bits<2;bits++)for(unsigned k=0;k<3;k++) {
        uint64_t values[]={0,2,UINT32_MAX};
        assert(exec(&s,NVMX_WRITE,0x4004,bits<<14).status==NVMX_SUCCESS);
        assert(exec(&s,NVMX_WRITE,0x4006,values[k]).status==NVMX_SUCCESS);
        assert(exec(&s,NVMX_WRITE,0x4008,values[2-k]).status==NVMX_SUCCESS);
        assert(exec(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
        assert(nested_profile_check(&s,&p)==NP_VALID);assert(nvmx_entry_cancel(&s));
    }
    assert(exec(&s,NVMX_WRITE,0x681e,0x7000).status==NVMX_SUCCESS);
    assert(exec(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nested_profile_check(&s,&p)==NP_GUEST);
    puts("PASS strict nested IA32e profile whitelist; no hardware entry");
}
