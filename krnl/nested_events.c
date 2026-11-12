#include "nested_events.h"
#include <stddef.h>
#define VALID UINT32_C(0x80000000)
#define UD_INFO UINT32_C(0x80000306)
#define UD_BIT (UINT32_C(1)<<6)
uint32_t nested_exception_bitmap(uint32_t bitmap_l0,uint32_t bitmap12) {
    return bitmap_l0|bitmap12;
}
bool nested_pf_intercept(uint32_t bitmap,uint32_t mask,uint32_t match,uint32_t error) {
    return ((bitmap&(1u<<14))!=0)==((error&mask)==match);
}
bool nested_hardware_exception(uint32_t info,uint32_t error,unsigned *vector) {
    if(!vector)return false;
    if(info==UD_INFO&&!error){*vector=6;return true;}
    /* SDM 27.2.1.3: error-code bits31:16 must be zero. Selector
     * EXT/IDT/TI bits are meaningful and are not rejected here. */
    if(info==UINT32_C(0x80000b0d)&&!(error&0xffff0000u)){*vector=13;return true;}
    /* Restricted baseline PF profile, P/W/U/RSVD only. No NX/SMEP, PK,
     * CET or SGX error semantics are advertised/implemented here. */
    if(info==UINT32_C(0x80000b0e)&&!(error&~0xfu)){*vector=14;return true;}
    return false;
}
enum nested_event_action nested_event_route(const struct nested_event_input *in,
                                            struct nested_event_injection *out) {
    unsigned vector;
    if(!in || !out || in->exit_reason!=0 ||
       !nested_hardware_exception(in->exit_info,in->exit_error,&vector) ||
       (in->vectoring&VALID) || (in->entry_pending&VALID) || in->blocking ||
       in->pending_debug || (in->rflags&(UINT64_C(1)<<8)) ||
       (vector!=14&&!(nested_exception_bitmap(in->bitmap_l0,in->bitmap12)&(1u<<vector))))
        return NESTED_EVENT_REJECT;
    bool wants_l1=vector==14?nested_pf_intercept(in->bitmap12,in->pf_mask12,in->pf_match12,in->exit_error):
        (in->bitmap12&(1u<<vector))!=0;
    if(!wants_l1&&!(in->bitmap_l0&(1u<<vector)))return NESTED_EVENT_REJECT;
    *out=(struct nested_event_injection){.info=in->exit_info,.error=in->exit_error,.length=0};
    return wants_l1?NESTED_EVENT_REFLECT_L1:NESTED_EVENT_REINJECT_L2;
}
bool nested_entry_validate(const struct nested_entry_request *r) {
    unsigned vector;
    /* Invalid info and residual error are ignored architecturally. Profile
     * still requires zero software length. Valid hardware events use attrs. */
    return r && (!(r->info&VALID)||nested_hardware_exception(r->info,r->error,&vector)) && !r->length &&
        !r->blocking && !(r->vectoring&VALID) && !r->pending_debug && !(r->rflags&(1u<<8));
}
bool nested_entry_arm(struct nested_entry_event *e,enum nested_entry_origin origin,
                      const struct nested_entry_request *r) {
    if(!e||e->armed||e->observed||e->origin!=NESTED_ORIGIN_NONE||
       (origin!=NESTED_ORIGIN_L0&&origin!=NESTED_ORIGIN_L1)||
       !nested_entry_validate(r)||!(r->info&VALID))return false;
    *e=(struct nested_entry_event){.origin=origin,.armed=true};return true;
}
bool nested_entry_observe(struct nested_entry_event *e,bool completed) {
    if(!e||!e->armed||e->observed||!completed)return false;
    e->observed=true;return true;
}
bool nested_entry_release(struct nested_entry_event *e) {
    if(!e||!e->armed||!e->observed)return false;
    *e=(struct nested_entry_event){0};return true;
}
