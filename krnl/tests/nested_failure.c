#include "../nested_profile.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct nvmx_result x(struct nvmx *s,enum nvmx_op op,uint64_t field,uint64_t value) {
    return nvmx_execute(s,(struct nvmx_request){.op=op,.operand=op==NVMX_ON?0xd000:0xe000,.field=field,.value=value,.region_revision=NVMX_REVISION,.flags=0xad7});
}
int main(void) {
    struct nvmx s;assert(nvmx_init(&s,0xd000,0xe000,48));
    assert(x(&s,NVMX_ON,0,0).status==NVMX_SUCCESS);
    struct nvmx_result invalid=x(&s,NVMX_READ,0x681e,0);
    assert(invalid.status==NVMX_FAIL_INVALID&&(invalid.flags&0x8d5)==1);
    assert(x(&s,NVMX_CLEAR,0,0).status==NVMX_SUCCESS);
    assert(x(&s,NVMX_PTRLD,0,0).status==NVMX_SUCCESS);
    struct nested_profile p={.cr0=0x80010033,.cr4=0x2020,.efer=0x500,.pat=0x7040600070406,.l2_entry=0x6100,.l2_resume=0x6103,.l1_exit=0x6200,.entry_failure_probe=true};
    struct nested_field_value t[NVMX_FIELDS];size_t n=nested_profile_table(&p,t,NVMX_FIELDS);
    for(size_t i=0;i<n;i++)assert(x(&s,NVMX_WRITE,t[i].field,t[i].value).status==NVMX_SUCCESS);
    uint64_t fields[]={0x4002,0xc02},bad[]={1,0},good[]={0,8};
    for(unsigned i=0;i<2;i++) {
        assert(x(&s,NVMX_WRITE,fields[i],bad[i]).status==NVMX_SUCCESS);
        assert(x(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
        assert(nested_early_failure(&s,&p)==7+i);
        struct nvmx before=s;
        struct nvmx_result r=nvmx_entry_fail(&s,7+i,0xad7);
        assert(r.status==NVMX_FAIL_VALID&&(r.flags&0x8d5)==0x40&&!s.launched&&!s.prepared);
        for(size_t j=0;j<n;j++){uint64_t a,b;assert(nvmx_peek(&before,t[j].field,&a)&&nvmx_peek(&s,t[j].field,&b)&&a==b);}
        assert(x(&s,NVMX_WRITE,fields[i],good[i]).status==NVMX_SUCCESS);
    }
    assert(x(&s,NVMX_WRITE,0x6820,0).status==NVMX_SUCCESS);
    assert(x(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nested_profile_check(&s,&p)==NP_EXPECTED_GUEST_FAILURE);
    struct nvmx before=s;
    assert(!nvmx_failed_entry_finish(&s,33,0)&&!memcmp(&s,&before,sizeof s));
    assert(!nvmx_failed_entry_finish(&s,0x80000021,1)&&!memcmp(&s,&before,sizeof s));
    assert(nvmx_failed_entry_finish(&s,0x80000021,0)&&!s.prepared&&!s.launched&&!s.running);
    for(size_t i=0;i<n;i++){uint64_t a,b;assert(nvmx_peek(&before,t[i].field,&a)&&nvmx_peek(&s,t[i].field,&b)&&a==b);}
    uint64_t value;assert(nvmx_peek(&s,0x4400,&value)&&value==8);
    assert(nvmx_peek(&s,0x4402,&value)&&value==0x80000021);
    assert(x(&s,NVMX_RESUME,0,0).error==5);
    assert(x(&s,NVMX_WRITE,0x6820,2).status==NVMX_SUCCESS);
    assert(x(&s,NVMX_LAUNCH,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nested_profile_check(&s,&p)==NP_VALID&&nvmx_entry_commit(&s));
    assert(nvmx_return_from_l2(&s));assert(x(&s,NVMX_RESUME,0,0).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_failed_entry_finish(&s,0x80000021,0)&&s.launched); /* resume preserves prior state */
    puts("PASS nested early/late failure distinction and no-save atomicity");
}
