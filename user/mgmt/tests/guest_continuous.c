#include "../guest_continuous.h"
#include <assert.h>
#include <stdio.h>
struct mock { unsigned start,step,recv,out,stop,destroy,reap; int fault; };
static int64_t call(void *p,uint64_t op,uint64_t h,uint64_t a) {
    struct mock *m=p;
    if (op==SYLE_GUEST_CREATE) {assert(h==0&&a==1);return m->fault==1?-7:42;}
    if (op==SYLE_GUEST_REAP) {assert(h==0&&a==0);m->reap++;return 0;}
    assert(h==42&&a==0);
    switch(op) {
    case SYLE_GUEST_START:m->start++;return m->fault==2?-7:0;
    case SYLE_GUEST_STEP:m->step++;return m->fault==3?-7:0;
    case SYLE_GUEST_CONSOLE_RECV:m->recv++;return m->fault==4?-6:(m->recv%3==0?(m->fault==8?-16:-SYLE_GUEST_EBUSY):'a');
    case SYLE_GUEST_STOP:m->stop++;return m->fault==6?-7:0;
    case SYLE_GUEST_DESTROY:m->destroy++;return m->fault==7?-7:0;
    default:assert(0);return -1;
    }
}
static bool output(void *p,uint8_t b) {struct mock *m=p;assert(b=='a');m->out++;return m->fault!=5;}
int main(void) {
    for(int f=0;f<=8;f++) {
        struct mock m={.fault=f};struct mg_continuous_ops ops={&m,call,output};
        enum mg_continuous_result r=mg_continuous_run(&ops,3,64);
        assert(r==((f==6||f==7)?MG_CLEANUP_FAILED:(f&&f!=8)?MG_RUN_FAILED:MG_BUDGET_REACHED));
        if(f==1) {assert(m.reap==1&&!m.stop&&!m.destroy);continue;}
        assert(m.stop==1&&m.destroy==1&&m.start==1);
        if(!f||f==8) assert(m.step==2&&m.out==6&&m.recv==9);
    }
    struct mock m={0};struct mg_continuous_ops o={&m,call,output};
    assert(mg_continuous_run(&o,2,1)==MG_BUDGET_REACHED);
    assert(m.recv==2&&m.out==2&&m.step==1);
    assert(mg_continuous_run(&o,0,1)==MG_RUN_FAILED);
    puts("continuous frontend policy PASS (host only)");
}
