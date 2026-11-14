#ifndef SYLE_GUEST_TERMINAL_H
#define SYLE_GUEST_TERMINAL_H
#include "guest_continuous.h"
#include "../../include/syle/host_console.h"
/* Text terminal transport, not a shell acceptance detector. */
enum mg_terminal_result { MG_TERMINAL_DETACHED, MG_TERMINAL_BUDGET,
    MG_TERMINAL_FAILED, MG_TERMINAL_CLEANUP_FAILED };
struct mg_terminal_report {uint32_t slices,sent,received;bool ready,prompt;int64_t guest_result,input_result,space_result,send_result;};
static inline enum mg_terminal_result mg_terminal_run(const struct mg_continuous_ops *o,
    uint32_t slices,uint32_t drain_limit,struct mg_terminal_report *report) {
    struct mg_terminal_report local={0};if(!report)report=&local;*report=(struct mg_terminal_report){0};
    if(!o||!o->call||!o->output||!slices||!drain_limit)return MG_TERMINAL_FAILED;
    int64_t h=o->call(o->ctx,SYLE_GUEST_CREATE,0,SYLE_GUEST_BOOT_PROFILE);
    if(h<=0)return o->call(o->ctx,SYLE_GUEST_REAP,0,0)==0?MG_TERMINAL_FAILED:MG_TERMINAL_CLEANUP_FAILED;
    enum mg_terminal_result result=MG_TERMINAL_BUDGET;
    static const char ready[]="SYLE_LINUX_USERSPACE_READY";
    unsigned ready_match=0;bool prompt_hash=false;
    for(uint32_t i=0;i<slices;i++) {
        report->slices=i+1;
        report->guest_result=o->call(o->ctx,i?SYLE_GUEST_STEP:SYLE_GUEST_START,(uint64_t)h,0);
        if(report->guest_result!=0){result=MG_TERMINAL_FAILED;break;}
        for(uint32_t n=0;n<drain_limit;n++) {
            int64_t b=o->call(o->ctx,SYLE_GUEST_CONSOLE_RECV,(uint64_t)h,0);
            if(b==-SYLE_GUEST_EBUSY||b==-16)break;
            if(b<0||b>255||!o->output(o->ctx,(uint8_t)b)){result=MG_TERMINAL_FAILED;break;}
            report->received++;
            if(!report->ready) {
                if((uint8_t)b==(uint8_t)ready[ready_match])ready_match++;
                else ready_match=(uint8_t)b==(uint8_t)ready[0]?1:0;
                if(ready_match==sizeof(ready)-1)report->ready=true;
            } else if(!report->prompt) {
                if(prompt_hash&&b==' ')report->prompt=true;
                prompt_hash=b=='#';
            }
        }
        if(result==MG_TERMINAL_FAILED)break;
        if(!report->prompt)continue;
        report->space_result=o->call(o->ctx,SYLE_GUEST_CONSOLE_SPACE,(uint64_t)h,0);
        if(report->space_result==-SYLE_GUEST_EBUSY)continue;
        if(report->space_result<0||report->space_result>16){result=MG_TERMINAL_FAILED;break;}
        if(!report->space_result)continue;
        /* No guest entry or other producer between SPACE and SEND. Single
         * BSP ABI makes this one-byte capacity stable. Host bytes remain
         * unread when the guest FIFO is full; no echo heuristics or retry. */
        report->input_result=o->call(o->ctx,SYLE_HOST_CONSOLE_RECV,0,0);
        if(report->input_result==SYLE_HOST_CONSOLE_EMPTY)continue;
        if(report->input_result<0||report->input_result>255){result=MG_TERMINAL_FAILED;break;}
        if(report->input_result==0x1d){result=MG_TERMINAL_DETACHED;break;}
        report->send_result=o->call(o->ctx,SYLE_GUEST_CONSOLE_SEND,(uint64_t)h,(uint8_t)report->input_result);
        if(report->send_result!=0){result=MG_TERMINAL_FAILED;break;}
        report->sent++;
    }
    int64_t stop=o->call(o->ctx,SYLE_GUEST_STOP,(uint64_t)h,0);
    int64_t destroy=o->call(o->ctx,SYLE_GUEST_DESTROY,(uint64_t)h,0);
    return stop||destroy?MG_TERMINAL_CLEANUP_FAILED:result;
}
#endif
