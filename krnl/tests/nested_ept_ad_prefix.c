#include "../nested_ept_ad.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t tables[4][512] __attribute__((aligned(4096)));
struct source {int read_fail,write_fail,readonly,wrong_hpa;unsigned writes;};
static struct source init(void) {
    memset(tables,0,sizeof tables);
    tables[0][0]=0x2007;tables[1][0]=0x3007;
    tables[2][0]=0x4007;tables[3][15]=0x8037;
    return (struct source){.read_fail=-1,.write_fail=-1,.readonly=-1,.wrong_hpa=-1};
}
static uint64_t *at(unsigned depth) {return &tables[depth][depth==3?15:0];}
static bool read_page(void *ctx,uint64_t page,uint8_t need,uint64_t *hpa,
    const uint64_t **ptr,uint8_t *perm) {
    struct source *s=ctx;
    int index=page>=0x1000&&page<=0x4000?(int)(page/4096)-1:4;
    if(index==s->read_fail) return false;
    if(index<4) {assert(need==1);*hpa=0x100000+(unsigned)index*4096;*ptr=tables[index];*perm=7;}
    else {assert(page==0x8000);*hpa=0x900000;*ptr=NULL;*perm=7;}
    return true;
}
static bool write_page(void *ctx,uint64_t page,uint8_t need,uint64_t *hpa,
    uint64_t **ptr,uint8_t *perm) {
    struct source *s=ctx;assert(need==3&&page>=0x1000&&page<=0x4000);
    int index=(int)(page/4096)-1;s->writes++;
    if(index==s->write_fail) return false;
    *hpa=index==s->wrong_hpa?0x900000:0x100000+(unsigned)index*4096;
    *ptr=tables[index];*perm=index==s->readonly?1:7;return true;
}
static struct nested_ept_result walk(struct source *s,uint8_t access,struct nested_ept_ad_trace *t,bool prefix) {
    const struct nested_ept_policy p={48,true,true};
    return prefix?nested_ept_ad_walk_prefix(0x105e,0xf123,access,read_page,s,&p,t):
        nested_ept_ad_walk(0x105e,0xf123,access,read_page,s,&p,t);
}
int main(void) {
    struct source s;struct nested_ept_ad_trace t;
    for(unsigned terminal=0;terminal<4;terminal++) {
        s=init();*at(terminal)=UINT64_C(0xfffffffffffffff8);
        uint64_t nonpresent=*at(terminal);
        assert(walk(&s,2,&t,true).status==NESTED_EPT_L1_DENIED);
        assert(t.count==terminal&&t.leaf_level==0&&t.maxphys==48);
        assert(nested_ept_ad_update_prefix(&t,write_page,&s));
        assert(s.writes==terminal&&*at(terminal)==nonpresent);
        for(unsigned i=0;i<terminal;i++) assert((*at(i)&0x300)==0x100);
        for(unsigned i=terminal+1;i<4;i++) assert((*at(i)&0x300)==0);
        assert(!nested_ept_ad_update(&t,false,write_page,&s)); /* old updater requires full */
        t.count=999;assert(walk(&s,2,&t,false).status==NESTED_EPT_L1_DENIED&&t.count==0);
        for(unsigned fail=0;fail<terminal;fail++) for(unsigned mode=0;mode<4;mode++) {
            s=init();*at(terminal)=0;
            assert(walk(&s,2,&t,true).status==NESTED_EPT_L1_DENIED);
            if(mode==0) s.write_fail=(int)fail;
            if(mode==1) s.readonly=(int)fail;
            if(mode==2) s.wrong_hpa=(int)fail;
            if(mode==3) *at(fail)^=0x1000;
            uint64_t before[4][512];memcpy(before,tables,sizeof before);
            assert(!nested_ept_ad_update_prefix(&t,write_page,&s));
            assert(memcmp(before,tables,sizeof before)==0);
        }
    }
    /* Legal full permission denial marks A along path, never sets D. */
    for(unsigned ro=0;ro<4;ro++) {
        s=init();*at(ro)&=~UINT64_C(2);
        assert(walk(&s,2,&t,true).status==NESTED_EPT_L1_DENIED);
        assert(t.count==4&&t.leaf_level==1&&t.entries[3].leaf);
        assert(nested_ept_ad_update_prefix(&t,write_page,&s));
        for(unsigned i=0;i<4;i++) assert((*at(i)&0x300)==0x100);
        assert(walk(&s,2,&t,false).status==NESTED_EPT_L1_DENIED&&t.count==0);
    }
    /* Preserve current D, but prefix mode must not newly restore cached D. */
    s=init();*at(0)&=~UINT64_C(2);*at(3)|=0x200;
    assert(walk(&s,2,&t,true).status==NESTED_EPT_L1_DENIED);
    assert(nested_ept_ad_update_prefix(&t,write_page,&s)&&(*at(3)&0x200));
    *at(3)&=~UINT64_C(0x200);
    assert(nested_ept_ad_update_prefix(&t,write_page,&s)&&!(*at(3)&0x200));
    /* Any L0 walker backing failure drops the whole trace. */
    for(unsigned fail=0;fail<5;fail++) {
        s=init();s.read_fail=(int)fail;t.count=999;
        uint64_t before[4][512];memcpy(before,tables,sizeof before);
        assert(walk(&s,1,&t,true).status==NESTED_EPT_L0_BACKING_DENIED&&t.count==0);
        assert(memcmp(before,tables,sizeof before)==0);
    }
    for(unsigned malformed=0;malformed<4;malformed++) {
        s=init();*at(malformed)|=1u<<11;t.count=999;
        assert(walk(&s,1,&t,true).status==NESTED_EPT_MALFORMED_UNSUPPORTED&&t.count==0);
    }
    /* Successful walk still yields the same full trace. */
    s=init();assert(walk(&s,1,&t,true).status==NESTED_EPT_OK&&t.count==4&&t.leaf_level==1);
    assert(nested_ept_ad_update_prefix(&t,write_page,&s));
    assert(!(*at(3)&0x200));
    /* Full large-leaf denial preserves the actual leaf level and permissions. */
    const struct nested_ept_policy p={48,true,true};
    s=init();tables[1][0]=0x400000b5;
    assert(nested_ept_ad_walk_prefix(0x105e,0xf123,2,read_page,&s,&p,&t).status==NESTED_EPT_L1_DENIED);
    assert(t.count==2&&t.leaf_level==3&&nested_ept_ad_update_prefix(&t,write_page,&s));
    assert((tables[1][0]&0x300)==0x100);
    s=init();tables[2][0]=0x4000b5;
    assert(nested_ept_ad_walk_prefix(0x105e,0xf123,2,read_page,&s,&p,&t).status==NESTED_EPT_L1_DENIED);
    assert(t.count==3&&t.leaf_level==2&&nested_ept_ad_update_prefix(&t,write_page,&s));
    assert((tables[2][0]&0x300)==0x100);
    /* Forged malformed prefixes are refused without memory writes. */
    s=init();*at(2)=0;assert(walk(&s,1,&t,true).status==NESTED_EPT_L1_DENIED);
    struct nested_ept_ad_trace saved=t;
    for(unsigned bad=0;bad<4;bad++) {
        t=saved;
        if(bad==0) t.entries[1].leaf=true;
        if(bad==1) t.count=4; /* partial nonleaf prefix cannot reach level0 */
        if(bad==2) t.maxphys=0;
        if(bad==3) t.entries[0].raw^=0x1000;
        uint64_t before[4][512];memcpy(before,tables,sizeof before);
        assert(!nested_ept_ad_update_prefix(&t,write_page,&s));
        assert(memcmp(before,tables,sizeof before)==0);
    }
    puts("PASS denied EPT12 present-prefix A policy and transactional validation");
}
