#ifndef SYLE_GUEST_SHELL_PROBE_H
#define SYLE_GUEST_SHELL_PROBE_H
#include "guest_continuous.h"
/* Fixed command acceptance probe, not a host keyboard or general terminal. */
#ifndef MG_SHELL_READY
#define MG_SHELL_READY "SYLE_LINUX_USERSPACE_READY"
#endif
#ifndef MG_SHELL_COMMAND
#define MG_SHELL_COMMAND "echo SYLE_\"\"SHELL_OK\n"
#endif
#ifndef MG_SHELL_REPLY
#define MG_SHELL_REPLY "SYLE_SHELL_OK"
#endif
enum mg_shell_result { MG_SHELL_VERIFIED, MG_SHELL_TIMEOUT,
    MG_SHELL_FAILED, MG_SHELL_CLEANUP_FAILED };
struct mg_shell_report {
    uint32_t slices, sent;
    bool ready, prompt, echo_wait, reply, echo_mismatch;
    uint8_t expected_echo, observed_echo;
    int64_t guest_result, send_result;
};
struct mg_shell_stream {
    unsigned ready_match, prompt_match, line_len;
    bool ready, prompt, line_overflow;
    char line[64];
};
static inline bool mg_shell_byte(struct mg_shell_stream *s,uint8_t b,bool armed) {
    static const char ready[]=MG_SHELL_READY,reply[]=MG_SHELL_REPLY;
    if(!s->ready) {
        if(b==(uint8_t)ready[s->ready_match])s->ready_match++;
        else s->ready_match=b==(uint8_t)ready[0]?1:0;
        if(s->ready_match==sizeof(ready)-1)s->ready=true;
    } else if(!s->prompt) {
        if(s->prompt_match&&b==' ')s->prompt=true;
        s->prompt_match=b=='#';
    }
    if(b=='\r')return false;
    if(b=='\n') {
        bool match=armed&&!s->line_overflow&&s->line_len==sizeof(reply)-1;
        for(unsigned i=0;match&&i<sizeof(reply)-1;i++)if(s->line[i]!=reply[i])match=false;
        s->line_len=0;s->line_overflow=false;return match;
    }
    if(s->line_len<sizeof(s->line))s->line[s->line_len++]=(char)b;
    else s->line_overflow=true;
    return false;
}
typedef bool (*mg_shell_observer)(void *,const struct mg_shell_report *);
static inline bool mg_shell_notify(const struct mg_shell_report *r,
    struct mg_shell_report *last,mg_shell_observer observer,void *ctx) {
    bool changed=r->ready!=last->ready||r->prompt!=last->prompt||r->reply!=last->reply||
        r->echo_mismatch!=last->echo_mismatch||
        ((r->sent==1||r->sent==sizeof(MG_SHELL_COMMAND)-1)&&r->sent!=last->sent);
    *last=*r;
    return !changed||!observer||observer(ctx,r);
}
static inline enum mg_shell_result mg_shell_probe_run_observed(
    const struct mg_continuous_ops *o,uint32_t slices,uint32_t drain_limit,
    uint32_t post_reply_slices,struct mg_shell_report *report,
    mg_shell_observer observer,void *observer_ctx) {
    struct mg_shell_report local={0},last={0};
    if(!report)report=&local;
    *report=(struct mg_shell_report){0};
    if(!o||!o->call||!o->output||!slices||!drain_limit||!post_reply_slices)
        return MG_SHELL_FAILED;
    int64_t h=o->call(o->ctx,SYLE_GUEST_CREATE,0,SYLE_GUEST_BOOT_PROFILE);
    if(h<=0)return o->call(o->ctx,SYLE_GUEST_REAP,0,0)==0?MG_SHELL_FAILED:MG_SHELL_CLEANUP_FAILED;
    static const char command[]=MG_SHELL_COMMAND;
    struct mg_shell_stream stream={0};
    unsigned sent=0,post=0;bool echo_wait=false,reply=false;
    enum mg_shell_result result=MG_SHELL_TIMEOUT;
    for(uint32_t i=0;i<slices;i++) {
        report->slices=i+1;
        report->guest_result=o->call(o->ctx,i?SYLE_GUEST_STEP:SYLE_GUEST_START,(uint64_t)h,0);
        if(report->guest_result!=0) {
            result=MG_SHELL_FAILED;break;
        }
        bool already_replied=reply;
        for(uint32_t n=0;n<drain_limit;n++) {
            int64_t b=o->call(o->ctx,SYLE_GUEST_CONSOLE_RECV,(uint64_t)h,0);
            if(b==-SYLE_GUEST_EBUSY||b==-16)break;
            if(b<0||b>255||!o->output(o->ctx,(uint8_t)b)) {result=MG_SHELL_FAILED;break;}
            /* Never search asynchronous output for a matching byte. During
             * transmission the next byte must be exactly the pending echo;
             * unsolicited bytes are an ambiguous terminal stream, fail closed. */
            if(sent&&sent<sizeof(command)-1) {
                if(!echo_wait||(uint8_t)b!=(uint8_t)command[sent-1]) {
                    report->echo_mismatch=true;
                    report->expected_echo=echo_wait?(uint8_t)command[sent-1]:0;
                    report->observed_echo=(uint8_t)b;
                    result=MG_SHELL_FAILED;break;
                }
                echo_wait=false;
            }
            if(mg_shell_byte(&stream,(uint8_t)b,sent==sizeof(command)-1))reply=true;
        }
        report->ready=stream.ready;report->prompt=stream.prompt;
        report->echo_wait=echo_wait;report->reply=reply;
        if(!mg_shell_notify(report,&last,observer,observer_ctx))result=MG_SHELL_FAILED;
        if(result==MG_SHELL_FAILED)break;
        if(already_replied&&++post>=post_reply_slices){result=MG_SHELL_VERIFIED;break;}
        if(stream.prompt&&!echo_wait&&sent<sizeof(command)-1) {
            /* At most one outstanding RX byte. Never retry ambiguous SEND
             * errors: the backend may have side effects even on rejection. */
            report->send_result=o->call(o->ctx,SYLE_GUEST_CONSOLE_SEND,(uint64_t)h,(uint8_t)command[sent]);
            if(report->send_result!=0){result=MG_SHELL_FAILED;break;}
            echo_wait=command[sent]!='\n';sent++;
            report->sent=sent;report->echo_wait=echo_wait;
            if(!mg_shell_notify(report,&last,observer,observer_ctx)){result=MG_SHELL_FAILED;break;}
        }
    }
    int64_t stop=o->call(o->ctx,SYLE_GUEST_STOP,(uint64_t)h,0);
    int64_t destroy=o->call(o->ctx,SYLE_GUEST_DESTROY,(uint64_t)h,0);
    return stop||destroy?MG_SHELL_CLEANUP_FAILED:result;
}
static inline enum mg_shell_result mg_shell_probe_run_report(
    const struct mg_continuous_ops *o,uint32_t slices,uint32_t drain_limit,
    uint32_t post_reply_slices,struct mg_shell_report *report) {
    return mg_shell_probe_run_observed(o,slices,drain_limit,post_reply_slices,report,0,0);
}
static inline enum mg_shell_result mg_shell_probe_run(
    const struct mg_continuous_ops *o,uint32_t slices,uint32_t drain_limit,
    uint32_t post_reply_slices) {
    return mg_shell_probe_run_report(o,slices,drain_limit,post_reply_slices,0);
}
#endif
