#ifndef SYLE_NESTED_EVENTS_H
#define SYLE_NESTED_EVENTS_H
#include <stdint.h>
#include <stdbool.h>
/* Restricted #UD/#GP ownership policy, not a general event arbiter. All VMCS
 * values are supplied by the caller; no state or architectural RIP is changed. */
enum nested_event_action {
    NESTED_EVENT_REJECT,
    NESTED_EVENT_REFLECT_L1,
    NESTED_EVENT_REINJECT_L2
};
struct nested_event_input {
    uint32_t exit_reason,exit_info,exit_error,vectoring,entry_pending,blocking;
    uint64_t pending_debug,rflags;
    uint32_t bitmap12,bitmap_l0;
    uint32_t pf_mask12,pf_match12;
};
struct nested_event_injection {
    uint32_t info,error,length;
};
/* Supported protected-mode hardware exception attributes; other vectors are
 * unsupported, not claimed architecturally invalid. */
bool nested_hardware_exception(uint32_t info,uint32_t error,unsigned *vector);
bool nested_pf_intercept(uint32_t bitmap,uint32_t mask,uint32_t match,uint32_t error);
uint32_t nested_exception_bitmap(uint32_t bitmap_l0,uint32_t bitmap12);
/* On rejection *out is unchanged. For either accepted ownership outcome,
 * out describes the hardware event (UD without error, GP with its original
 * 16-bit error, no software length).
 * REFLECT_L1 must not itself inject into L2; the caller reflects to L1.
 * Invalid vectoring/entry info has no pending event, so its other bits are
 * ignored. Valid vectoring/entry info is always rejected in this subset. */
enum nested_event_action nested_event_route(const struct nested_event_input *in,
                                            struct nested_event_injection *out);

/* Entry-request lifecycle is separate from interception ownership. This is a
 * strict diagnostic subset, NOT Intel's full event-injection validator. */
enum nested_entry_origin { NESTED_ORIGIN_NONE, NESTED_ORIGIN_L0, NESTED_ORIGIN_L1 };
struct nested_entry_request {
    uint32_t info,error,length,blocking,vectoring;
    uint64_t pending_debug,rflags;
};
struct nested_entry_event { enum nested_entry_origin origin; bool armed,observed; };
bool nested_entry_validate(const struct nested_entry_request *request);
bool nested_entry_arm(struct nested_entry_event *event,enum nested_entry_origin origin,
                      const struct nested_entry_request *request);
/* Caller must prove handler completion and absence of intervening events.
 * Failure changes nothing: do not retry an ambiguous event delivery. */
bool nested_entry_observe(struct nested_entry_event *event,bool completed);
bool nested_entry_release(struct nested_entry_event *event);

#endif
