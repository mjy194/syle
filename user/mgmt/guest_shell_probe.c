#include "guest.h"
#include "guest_shell_probe.h"
static int64_t call(void *p,uint64_t op,uint64_t h,uint64_t a){(void)p;return syle_guest_call(op,h,a);}
static bool output(void *p,uint8_t b){(void)p;return syle_guest_call(1,(uintptr_t)&b,1)==1;}
static bool report_number(const char *label,uint64_t value) {
    uint64_t n=0;while(label[n])n++;
    bool ok=syle_guest_call(1,(uintptr_t)label,n)==(int64_t)n;
    char hex[19];hex[0]='0';hex[1]='x';
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<16;i++)hex[i+2]=digits[(value>>(60-4*i))&15];
    hex[18]='\n';
    return syle_guest_call(1,(uintptr_t)hex,sizeof(hex))==(int64_t)sizeof(hex)&&ok;
}
static bool observed(void *ctx,const struct mg_shell_report *r) {
    (void)ctx;
    bool ok=report_number("\nSYLE.PROBE phase slices=",r->slices);
    ok=report_number(" ready=",r->ready)&&ok;
    ok=report_number(" prompt=",r->prompt)&&ok;
    ok=report_number(" sent=",r->sent)&&ok;
    ok=report_number(" echo_wait=",r->echo_wait)&&ok;
    ok=report_number(" reply=",r->reply)&&ok;
    ok=report_number(" echo_mismatch=",r->echo_mismatch)&&ok;
    ok=report_number(" expected_echo=",r->expected_echo)&&ok;
    ok=report_number(" observed_echo=",r->observed_echo)&&ok;
    return ok;
}
void mg_shell_probe_main(void) {
    const struct mg_continuous_ops ops={0,call,output};
    struct mg_shell_report report;
    enum mg_shell_result r=mg_shell_probe_run_observed(&ops,1u<<18,32,256,&report,observed,0);
    bool diagnostics=report_number("\nSYLE.PROBE result=",r);
    diagnostics=report_number(" slices=",report.slices)&&diagnostics;
    diagnostics=report_number(" ready=",report.ready)&&diagnostics;
    diagnostics=report_number(" prompt=",report.prompt)&&diagnostics;
    diagnostics=report_number(" sent=",report.sent)&&diagnostics;
    diagnostics=report_number(" echo_wait=",report.echo_wait)&&diagnostics;
    diagnostics=report_number(" reply=",report.reply)&&diagnostics;
    diagnostics=report_number(" guest_result=",(uint64_t)report.guest_result)&&diagnostics;
    diagnostics=report_number(" send_result=",(uint64_t)report.send_result)&&diagnostics;
    static const char yes[]="\nSYLE.MGMT shell command response verified; not a general terminal\n";
    static const char no[]="\nSYLE.MGMT shell probe failed, timed out, or cleanup failed\n";
    const char *m=r==MG_SHELL_VERIFIED?yes:no;
    uint64_t n=r==MG_SHELL_VERIFIED?sizeof(yes)-1:sizeof(no)-1;
    bool ok=syle_guest_call(1,(uintptr_t)m,n)==(int64_t)n;
    syle_guest_call(3,r!=MG_SHELL_VERIFIED||!ok||!diagnostics,0);__builtin_trap();
}
