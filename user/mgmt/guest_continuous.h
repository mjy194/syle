#ifndef SYLE_GUEST_CONTINUOUS_H
#define SYLE_GUEST_CONTINUOUS_H
#include "../../include/syle/guest_control.h"
#include <stdbool.h>
/* Host-testable policy, not a scheduler. Every guest call remains bounded. */
struct mg_continuous_ops {
    void *ctx;
    int64_t (*call)(void *,uint64_t,uint64_t,uint64_t);
    bool (*output)(void *,uint8_t);
};
enum mg_continuous_result { MG_BUDGET_REACHED, MG_RUN_FAILED, MG_CLEANUP_FAILED };
static inline enum mg_continuous_result mg_continuous_run(
        const struct mg_continuous_ops *o,uint32_t slices,uint32_t drain_limit) {
    if (!o || !o->call || !o->output || !slices || !drain_limit)
        return MG_RUN_FAILED;
    int64_t h=o->call(o->ctx,SYLE_GUEST_CREATE,0,SYLE_GUEST_BOOT_PROFILE);
    if (h<=0) {
        return o->call(o->ctx,SYLE_GUEST_REAP,0,0)==0 ? MG_RUN_FAILED : MG_CLEANUP_FAILED;
    }
    enum mg_continuous_result result=MG_BUDGET_REACHED;
    for (uint32_t i=0;i<slices;i++) {
        if (o->call(o->ctx,i?SYLE_GUEST_STEP:SYLE_GUEST_START,(uint64_t)h,0)!=0) {
            result=MG_RUN_FAILED;break;
        }
        /* Never let a productive device starve the next execution slice. */
        for (uint32_t n=0;n<drain_limit;n++) {
            int64_t b=o->call(o->ctx,SYLE_GUEST_CONSOLE_RECV,(uint64_t)h,0);
            /* The existing UART service returns legacy -16 directly; the
               controller does not translate backend errors. Accept both. */
            if (b==-SYLE_GUEST_EBUSY || b==-16) break;
            if (b<0 || b>255 || !o->output(o->ctx,(uint8_t)b)) {
                result=MG_RUN_FAILED;break;
            }
        }
        if (result!=MG_BUDGET_REACHED) break;
    }
    /* Attempt DESTROY even if STOP fails; the controller retains quarantine
       if teardown cannot prove ownership safe to release. */
    int64_t stop=o->call(o->ctx,SYLE_GUEST_STOP,(uint64_t)h,0);
    int64_t destroy=o->call(o->ctx,SYLE_GUEST_DESTROY,(uint64_t)h,0);
    if (stop!=0 || destroy!=0) return MG_CLEANUP_FAILED;
    return result;
}
#endif
