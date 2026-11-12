#include "../cache_ept_alias.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static _Alignas(4096) uint64_t tables[4][512];
static struct cache_ept_alias_plan plan,old;
static struct cache_ept_alias_view setup(void) {
    memset(tables,0,sizeof tables);
    struct cache_ept_alias_view v={.eptp=0x1001e};
    for(unsigned d=0;d<4;d++){v.table[d]=tables[d];v.hpa[d]=0x10000+d*4096;}
    for(unsigned d=0;d<3;d++)tables[d][0]=v.hpa[d+1]|7;
    tables[3][0]=0x800031;tables[3][63]=0x800031;
    tables[3][64]=0x800031;tables[3][511]=0x800031;
    tables[3][7]=0x900037; /* unrelated mapping must be retained exactly */
    return v;
}
static void bad(struct cache_ept_alias_view v,uint64_t target,bool uc) {
    memset(&plan,0xa5,sizeof plan);old=plan;
    assert(!cache_ept_alias_prepare(&plan,&v,target,uc));
    assert(!memcmp(&old,&plan,sizeof plan));
}
int main(void) {
    struct cache_ept_alias_view v=setup();
    assert(cache_ept_alias_prepare(&plan,&v,0x800000,true)&&plan.count==4);
    assert(!cache_ept_alias_publish(&plan));
    assert(cache_ept_alias_detach(&plan));
    assert(!tables[3][0]&&!tables[3][63]&&!tables[3][64]&&!tables[3][511]);
    assert(tables[3][7]==0x900037);
    assert(!cache_ept_alias_detach(&plan));
    assert(cache_ept_alias_publish(&plan));
    assert(tables[3][0]==0x800041&&tables[3][511]==0x800041);
    assert(!cache_ept_alias_publish(&plan));
    assert(cache_ept_alias_prepare(&plan,&v,0x800000,false));
    assert(cache_ept_alias_detach(&plan)&&cache_ept_alias_publish(&plan));
    assert(tables[3][0]==0x800031&&tables[3][7]==0x900037);
    for(unsigned d=0;d<4;d++) {
        v=setup();assert(cache_ept_alias_prepare(&plan,&v,0x800000,true));
        tables[d][100]=0x800031;old=plan;
        assert(!cache_ept_alias_detach(&plan));
        assert(tables[3][0]==0x800031&&!memcmp(&old,&plan,sizeof plan));
        v=setup();assert(cache_ept_alias_prepare(&plan,&v,0x800000,true));
        assert(cache_ept_alias_detach(&plan));tables[d][100]=0x800031;old=plan;
        assert(!cache_ept_alias_publish(&plan));
        assert(tables[3][0]==0&&!memcmp(&old,&plan,sizeof plan));
    }
    v=setup();assert(cache_ept_alias_prepare(&plan,&v,0x800000,true));
    assert(cache_ept_alias_detach(&plan));tables[3][63]=0x800031;
    assert(!cache_ept_alias_publish(&plan)&&tables[3][0]==0);
    for(unsigned d=0;d<3;d++) {
        v=setup();tables[d][1]=v.hpa[d+1]|7;bad(v,0x800000,true);
        v=setup();tables[d][0]|=0x80;bad(v,0x800000,true);
        v=setup();tables[d][0]^=0x1000;bad(v,0x800000,true);
        v=setup();tables[d][0]&=~UINT64_C(2);bad(v,0x800000,true);
    }
    const uint64_t bits[]={2,4,0x40,0x80,0x100,0x200,UINT64_C(1)<<63};
    for(unsigned i=0;i<sizeof bits/sizeof bits[0];i++) {
        v=setup();tables[3][63]^=bits[i];bad(v,0x800000,true);
    }
    v=setup();bad(v,0x800001,true);bad(v,0x10000,true);bad(v,UINT64_C(1)<<48,true);
    bad(v,0x700000,true);bad(v,0x800000,false);
    v.eptp|=0x40;bad(v,0x800000,true);
    v=setup();v.hpa[1]=v.hpa[0];bad(v,0x800000,true);
    v=setup();v.table[1]=v.table[0];bad(v,0x800000,true);
    v=setup();v.table[1]++;bad(v,0x800000,true);
    v=setup();v.hpa[2]++;bad(v,0x800000,true);
    assert(!cache_ept_alias_prepare(NULL,&v,0,true));
    assert(!cache_ept_alias_prepare(&plan,NULL,0,true));
    assert(!cache_ept_alias_detach(NULL)&&!cache_ept_alias_publish(NULL));
    puts("PASS complete fixed-shape EPT alias plan/detach/publish and rejection");
}
