#include "../nested_ept_atomic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint64_t tables[4][512];
static struct ad_entry_stamp stamp={1,2,3,4,5};
struct state {unsigned guards,loads,cas,deny_guard;bool failcas,clear_dirty,structural,change_hpa,fail_leaf;};
static bool guard(void *c,struct ad_entry_stamp s) {
    struct state *x=c;
    assert(!memcmp(&s,&stamp,sizeof s));
    return ++x->guards!=x->deny_guard;
}
static bool table(void *c,uint64_t gpa,uint64_t *hpa,uint64_t **page) {
    struct state *x=c;
    if(gpa<0x1000||gpa>0x4000||gpa%4096)return false;
    unsigned i=gpa/4096-1;*hpa=0x100000+i*4096;*page=tables[i];
    if(x->change_hpa && x->loads>=4)*hpa+=0x10000;
    return true;
}
static bool leaf(void *c,uint64_t gpa,uint64_t *hpa,uint8_t *perm) {
    struct state *x=c;assert(gpa==0x8000);
    *hpa=0x200000;*perm=3;return !x->fail_leaf;
}
static uint64_t load(void *c,uint64_t *p) {
    struct state *x=c;x->loads++;
    /* Concurrent clear after walk but before update: never resurrect old D. */
    if(x->clear_dirty&&x->loads==5)__atomic_fetch_and(&tables[3][15],~UINT64_C(0x200),__ATOMIC_ACQ_REL);
    return __atomic_load_n(p,__ATOMIC_ACQUIRE);
}
static bool cas(void *c,uint64_t *p,uint64_t old,uint64_t next) {
    struct state *x=c;x->cas++;
    if(x->structural){__atomic_fetch_xor(p,0x1000,__ATOMIC_ACQ_REL);return false;}
    if(x->failcas)return false;
    if(x->cas==1) { /* competing writer adds A; first CAS loses */
        __atomic_fetch_or(p,0x100,__ATOMIC_ACQ_REL);return false;
    }
    return __atomic_compare_exchange_n(p,&old,next,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static struct nested_ept_atomic_ops ops={guard,table,leaf,load,cas};
static void init(void) {
    memset(tables,0,sizeof tables);
    tables[0][0]=0x2007;tables[1][0]=0x3007;tables[2][0]=0x4007;
    tables[3][15]=0x8033;
}
static struct nested_ept_atomic_result walk(struct state *s,unsigned need) {
    struct nested_ept_atomic_trace trace;
    struct nested_ept_atomic_result r=nested_ept_atomic_walk(0x105e,0xf008,need,48,&ops,s,stamp,&trace);
    assert(!memcmp(&trace.stamp,&stamp,sizeof stamp));
    return r;
}
int main(void) {
    init();struct state s={0};
    struct nested_ept_atomic_result r=walk(&s,2);
    assert(r.status==NEA_DONE && r.translation.status==NESTED_EPT_OK);
    assert(r.translation.hpa==0x200008 && r.changed);
    assert(tables[0][0]==0x2107 && tables[3][15]==0x8333);
    init();s=(struct state){.clear_dirty=true};tables[3][15]|=0x200;
    r=walk(&s,1);assert(r.status==NEA_DONE && tables[3][15]==0x8133);
    init();s=(struct state){0};tables[3][15]=0xdead000;
    r=walk(&s,1);assert(r.status==NEA_DONE && r.translation.status==NESTED_EPT_L1_DENIED);
    assert(r.translation.effective12==0);
    assert(tables[0][0]==0x2107 && tables[2][0]==0x4107 && tables[3][15]==0xdead000);
    init();s=(struct state){0};tables[3][15]=0x8031;
    r=walk(&s,2);assert(r.status==NEA_DONE && r.translation.status==NESTED_EPT_L1_DENIED);
    assert(tables[3][15]==0x8131);
    init();s=(struct state){.fail_leaf=true};
    r=walk(&s,1);assert(r.status==NEA_BACKING && !r.changed && tables[0][0]==0x2007);
    init();s=(struct state){.fail_leaf=true};tables[3][15]=0x8031;
    r=walk(&s,2);assert(r.status==NEA_BACKING && !r.changed);
    init();s=(struct state){.change_hpa=true};
    r=walk(&s,1);assert(r.status==NEA_BACKING && !r.changed);
    init();s=(struct state){.failcas=true};
    r=walk(&s,1);assert(r.status==NEA_RETRY && !r.changed && s.cas==64);
    init();s=(struct state){.structural=true};
    r=walk(&s,1);assert(r.status==NEA_REWALK && !r.changed);
    init();s=(struct state){0};tables[2][0]|=0x80;
    r=walk(&s,1);assert(r.status==NEA_UNSUPPORTED && !r.changed);
    /* Test every guard failure, including after completed CAS (partial effects). */
    init();s=(struct state){0};r=walk(&s,2);unsigned guards=s.guards;
    bool partial=false;
    for(unsigned n=1;n<=guards;n++) {
        init();s=(struct state){.deny_guard=n};
        r=walk(&s,2);assert(r.status==NEA_STALE);partial|=r.changed;
    }
    assert(partial);
    /* Production backend must work independently of injected operations. */
    init();s=(struct state){0};struct nested_ept_atomic_ops real=ops;
    real.load=NULL;real.cas=NULL;
    struct nested_ept_atomic_trace trace;
    r=nested_ept_atomic_walk(0x105e,0xf000,2,48,&real,&s,stamp,&trace);
    assert(r.status==NEA_DONE && tables[3][15]==0x8333);
    assert(trace.count==4 && trace.entry[3].pointer==&tables[3][15]);
    assert(trace.entry[3].entry_gpa==0x4078 && trace.entry[3].table_hpa==0x103000);
    r=nested_ept_atomic_walk(0x101e,0xf000,2,48,&real,&s,stamp,&trace);
    assert(r.status==NEA_INVALID);
    puts("PASS atomic EPT A/D merge, no stale D replay, prefix faults, guards, conflicts and identity");
}
