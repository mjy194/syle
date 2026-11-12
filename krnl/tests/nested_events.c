#include "../nested_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct nested_event_input base(void) {
    return (struct nested_event_input){.exit_info=0x80000306,.rflags=2,.bitmap_l0=64};
}
static void rejected(struct nested_event_input in) {
    struct nested_event_injection out={1,2,3},before=out;
    struct nested_event_input original=in;
    assert(nested_event_route(&in,&out)==NESTED_EVENT_REJECT);
    assert(!memcmp(&out,&before,sizeof out));
    assert(!memcmp(&in,&original,sizeof in));
}
static void accepted(struct nested_event_input in,enum nested_event_action action) {
    struct nested_event_injection out={1,2,3};
    struct nested_event_input original=in;
    assert(nested_event_route(&in,&out)==action);
    assert(out.info==0x80000306 && !out.error && !out.length);
    assert(!memcmp(&in,&original,sizeof in));
}
int main(void) {
    struct nested_event_input in=base();
    accepted(in,NESTED_EVENT_REINJECT_L2);
    in.bitmap12=64;accepted(in,NESTED_EVENT_REFLECT_L1);
    in.bitmap_l0=0;accepted(in,NESTED_EVENT_REFLECT_L1);
    in.bitmap12=0;rejected(in);
    in.bitmap_l0=~64U;in.bitmap12=~64U;rejected(in);
    assert(nested_exception_bitmap(0x55,0xaa)==0xff);
    assert(nested_exception_bitmap(0,0)==0);
    assert(nested_exception_bitmap(~0U,0)==~0U);
    for(unsigned bit=0;bit<32;bit++) {
        in=base();in.exit_reason=1U<<bit;rejected(in);
        in=base();in.exit_info^=1U<<bit;rejected(in);
        in=base();in.blocking=1U<<bit;rejected(in);
    }
    for(unsigned bit=0;bit<64;bit++) {
        in=base();in.pending_debug=UINT64_C(1)<<bit;rejected(in);
    }
    for(unsigned vector=0;vector<256;vector++) {
        if(vector==6) continue;
        in=base();in.exit_info=0x80000300|vector;rejected(in);
    }
    for(unsigned type=0;type<8;type++) {
        if(type==3) continue;
        in=base();in.exit_info=0x80000006|(type<<8);rejected(in);
    }
    in=base();in.rflags|=1U<<8;rejected(in);
    in=base();in.vectoring=0x80000306;rejected(in);
    in=base();in.entry_pending=0x80000306;rejected(in);
    in=base();in.vectoring=0x80000000;rejected(in);
    in=base();in.entry_pending=0x80000000;rejected(in);
    /* Nonvalid VMCS info is not a pending event. */
    in=base();in.vectoring=0x7fffffff;in.entry_pending=0x7fffffff;
    accepted(in,NESTED_EVENT_REINJECT_L2);
    in=base();in.rflags|=1U<<9;accepted(in,NESTED_EVENT_REINJECT_L2);
    struct nested_event_injection out={1,2,3};
    assert(nested_event_route(NULL,&out)==NESTED_EVENT_REJECT);
    assert(out.info==1 && out.error==2 && out.length==3);
    assert(nested_event_route(&in,NULL)==NESTED_EVENT_REJECT);
    puts("PASS nested #UD exception ownership policy");
}
