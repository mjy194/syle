#include "../nested_events.h"
#include "../nested_vmx.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void reject(struct nested_entry_request r) {assert(!nested_entry_validate(&r));}
static void write_field(struct nvmx *s,uint64_t f,uint64_t v) {
    assert(nvmx_execute(s,(struct nvmx_request){.op=NVMX_WRITE,.field=f,.value=v}).status==NVMX_SUCCESS);
}
static uint64_t peek(struct nvmx *s,uint64_t f) {uint64_t v;assert(nvmx_peek(s,f,&v));return v;}
int main(void) {
    struct nested_entry_request r={.info=0x80000306,.rflags=2};
    assert(nested_entry_validate(&r));
    for(unsigned bit=0;bit<31;bit++){struct nested_entry_request x=r;x.info^=1u<<bit;reject(x);}
    for(unsigned bit=0;bit<32;bit++) {
        struct nested_entry_request x=r;x.error=1u<<bit;reject(x);
        x=r;x.length=1u<<bit;reject(x);
        x=r;x.blocking=1u<<bit;reject(x);
    }
    struct nested_entry_request x=r;x.vectoring=0x80000306;reject(x);
    x=r;x.rflags|=256;reject(x);x=r;x.pending_debug=1;reject(x);
    x=r;x.info=0x306;assert(nested_entry_validate(&x));
    x.info=0x7fffffff;assert(nested_entry_validate(&x));
    x=r;x.rflags=0x202;assert(nested_entry_validate(&x)); /* IF irrelevant for #UD */
    for(unsigned origin=NESTED_ORIGIN_L0;origin<=NESTED_ORIGIN_L1;origin++) {
        struct nested_entry_event e={0};assert(nested_entry_arm(&e,origin,&r));
        struct nested_entry_event before=e;
        assert(!nested_entry_arm(&e,origin,&r)&&!memcmp(&e,&before,sizeof e));
        assert(!nested_entry_observe(&e,false)&&!memcmp(&e,&before,sizeof e));
        assert(!nested_entry_release(&e));
        assert(nested_entry_observe(&e,true));assert(!nested_entry_observe(&e,true));
        assert(nested_entry_release(&e));assert(!e.armed&&!e.observed&&!e.origin);
    }
    /* GP attributes accept every architecturally allowed 16-bit error, not
     * just the diagnostic selector. High bits, missing error-valid and all
     * unrelated vectors/types/reserved encodings are rejected atomically. */
    struct nested_entry_request gp={.info=0x80000b0d,.error=0x28,.rflags=2};
    for(unsigned error=0;error<=0xffff;error++) {gp.error=error;assert(nested_entry_validate(&gp));}
    gp.error=0x28;
    for(unsigned bit=16;bit<32;bit++){x=gp;x.error|=1u<<bit;reject(x);}
    for(unsigned bit=0;bit<31;bit++){x=gp;x.info^=1u<<bit;reject(x);}
    struct nested_event_input route={.exit_info=gp.info,.exit_error=0x28,.bitmap_l0=0x2000,.rflags=2};
    struct nested_event_injection output={0};
    assert(nested_event_route(&route,&output)==NESTED_EVENT_REINJECT_L2);
    assert(output.info==gp.info&&output.error==0x28&&!output.length);
    route.bitmap12=0x2000;assert(nested_event_route(&route,&output)==NESTED_EVENT_REFLECT_L1);
    route.entry_pending=0x80000306;
    struct nested_event_injection prior=output;
    assert(nested_event_route(&route,&output)==NESTED_EVENT_REJECT&&!memcmp(&output,&prior,sizeof output));
    struct nested_entry_event conflict={0};assert(nested_entry_arm(&conflict,NESTED_ORIGIN_L0,&r));
    assert(!nested_entry_arm(&conflict,NESTED_ORIGIN_L1,&gp));
    /* PF inversion rule, not a normal bitmap test. Include impossible match
     * bits outside mask, and both bitmap values for each combination. */
    for(unsigned bit=0;bit<2;bit++)for(unsigned mask=0;mask<16;mask++)
    for(unsigned match=0;match<32;match++)for(unsigned error=0;error<16;error++) {
        bool expected=bit?((error&mask)==match):((error&mask)!=match);
        assert(nested_pf_intercept(bit<<14,mask,match,error)==expected);
        struct nested_event_input pf={.exit_info=0x80000b0e,.exit_error=error,
            .bitmap_l0=1u<<14,.bitmap12=bit<<14,.pf_mask12=mask,.pf_match12=match,.rflags=2};
        assert(nested_event_route(&pf,&output)==(expected?NESTED_EVENT_REFLECT_L1:NESTED_EVENT_REINJECT_L2));
        assert(output.info==0x80000b0e&&output.error==error);
    }
    struct nested_entry_request pf={.info=0x80000b0e,.rflags=2};
    for(unsigned e=0;e<16;e++){pf.error=e;assert(nested_entry_validate(&pf));}
    pf.error=0;
    for(unsigned bit=4;bit<32;bit++){x=pf;x.error=1u<<bit;reject(x);}
    x=pf;x.info^=1u<<11;reject(x);
    struct nvmx s;
    assert(nvmx_init(&s,0xd000,0xe000,48));
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_ON,.operand=0xd000,.region_revision=NVMX_REVISION}).status==NVMX_SUCCESS);
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_CLEAR,.operand=0xe000}).status==NVMX_SUCCESS);
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_PTRLD,.operand=0xe000,.region_revision=NVMX_REVISION}).status==NVMX_SUCCESS);
    write_field(&s,0x4016,0x80000306);
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_LAUNCH}).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_entry_fail(&s,7,2).status==NVMX_FAIL_VALID);
    assert(peek(&s,0x4016)==0x80000306);
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_LAUNCH}).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_failed_entry_finish(&s,0x80000021,0));
    assert(peek(&s,0x4016)==0x80000306&&!s.launched);
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_LAUNCH}).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_entry_commit(&s));assert(peek(&s,0x4016)==0x80000306);
    assert(!nvmx_entry_commit(&s)); /* L0 internal exit does not recommit */
    assert(peek(&s,0x4016)==0x80000306);
    assert(nvmx_return_from_l2(&s));assert(peek(&s,0x4016)==0x306);
    struct nvmx before=s;assert(!nvmx_return_from_l2(&s)&&!memcmp(&s,&before,sizeof s));
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_RESUME}).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_entry_commit(&s)&&nvmx_return_from_l2(&s));assert(peek(&s,0x4016)==0x306);
    write_field(&s,0x4016,0x80000b0d);write_field(&s,0x4018,0x28);
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_RESUME}).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_failed_entry_finish(&s,0x80000021,0));
    assert(peek(&s,0x4016)==0x80000b0d&&peek(&s,0x4018)==0x28&&s.launched);
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_RESUME}).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_entry_commit(&s)&&nvmx_return_from_l2(&s));
    assert(peek(&s,0x4016)==0xb0d&&peek(&s,0x4018)==0x28);
    write_field(&s,0x4016,0x80000b0e);write_field(&s,0x4018,0);
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_RESUME}).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_failed_entry_finish(&s,0x80000021,0));
    assert(peek(&s,0x4016)==0x80000b0e);
    assert(nvmx_execute(&s,(struct nvmx_request){.op=NVMX_RESUME}).status==NVMX_ENTRY_REQUEST);
    assert(nvmx_entry_commit(&s)&&nvmx_return_from_l2(&s));
    assert(peek(&s,0x4016)==0xb0e);
    puts("PASS nested entry #UD validation, lifecycle, failure preservation and normal-exit valid clear");
}
