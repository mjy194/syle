#include "../nested_ept.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t tables[4][512] __attribute__((aligned(4096)));
struct source {uint64_t leaf_page;bool missing;uint8_t perm;};
static struct source init(unsigned level) {
    memset(tables,0,sizeof tables);
    tables[0][0]=0x2307; /* inherited A and D even on nonleaf */
    if(level==3) tables[1][1]=0x800003b7;
    else {
        tables[1][0]=0x3307;
        if(level==2) tables[2][1]=0x4003b7;
        else {tables[2][0]=0x4307;tables[3][15]=0x8337;}
    }
    return (struct source){.leaf_page=level==3?0x80003000:level==2?0x401000:0x8000,.perm=7};
}
static bool resolve(void *ctx,uint64_t page,uint8_t need,uint64_t *hpa,
    const uint64_t **ptr,uint8_t *perm) {
    struct source *s=ctx;
    if(page>=0x1000&&page<=0x4000) {
        unsigned i=(unsigned)(page/4096)-1;assert(need==1);
        *hpa=0x100000+i*4096;*ptr=tables[i];*perm=7;return true;
    }
    assert(page==s->leaf_page);
    if(s->missing) return false;
    *hpa=0x900000;*ptr=NULL;*perm=s->perm;return true;
}
int main(void) {
    const struct nested_ept_policy p={48,true,true};
    for(unsigned level=1;level<=3;level++) {
        struct source s=init(level);
        uint64_t gpa=level==3?0x40003123:level==2?0x201123:0xf123;
        uint64_t before[4][512];memcpy(before,tables,sizeof before);
        struct nested_ept_result r=nested_ept_walk_ad0_ignored(0x101e,gpa,1,resolve,&s,&p);
        assert(r.status==NESTED_EPT_OK&&r.hpa==0x900123&&r.l1_gpa==s.leaf_page+0x123);
        assert(r.effective12==7&&r.effective02==7);
        assert(memcmp(before,tables,sizeof before)==0);
        assert(nested_ept_walk_policy(0x101e,gpa,1,resolve,&s,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        assert(nested_ept_walk(0x101e,gpa,1,resolve,&s).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        assert(nested_ept_walk_ad0_ignored(0x105e,gpa,1,resolve,&s,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        assert(memcmp(before,tables,sizeof before)==0);
        tables[0][0]&=~UINT64_C(2);memcpy(before,tables,sizeof before);
        assert(nested_ept_walk_ad0_ignored(0x101e,gpa,2,resolve,&s,&p).status==NESTED_EPT_L1_DENIED);
        assert(memcmp(before,tables,sizeof before)==0);
        s=init(level);s.missing=true;memcpy(before,tables,sizeof before);
        assert(nested_ept_walk_ad0_ignored(0x101e,gpa,1,resolve,&s,&p).status==NESTED_EPT_L0_BACKING_DENIED);
        assert(memcmp(before,tables,sizeof before)==0);
        s=init(level);s.perm=1;
        assert(nested_ept_walk_ad0_ignored(0x101e,gpa,4,resolve,&s,&p).status==NESTED_EPT_L0_BACKING_DENIED);
        s=init(level);tables[0][0]|=0x40;
        assert(nested_ept_walk_ad0_ignored(0x101e,gpa,1,resolve,&s,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        s=init(level);tables[0][0]=UINT64_C(0xfffffffffffffff8);
        assert(nested_ept_walk_ad0_ignored(0x101e,gpa,1,resolve,&s,&p).status==NESTED_EPT_L1_DENIED);
    }
    /* Independently inherit each bit at every level, no permission effect. */
    for(unsigned depth=0;depth<4;depth++) for(unsigned bits=1;bits<4;bits++) {
        struct source s=init(1);
        for(unsigned i=0;i<4;i++) tables[i][i==3?15:0]&=~UINT64_C(0x300);
        tables[depth][depth==3?15:0]|=(uint64_t)bits<<8;
        uint64_t before[4][512];memcpy(before,tables,sizeof before);
        assert(nested_ept_walk_ad0_ignored(0x101e,0xf123,2,resolve,&s,&p).status==NESTED_EPT_OK);
        assert(memcmp(before,tables,sizeof before)==0);
    }
    puts("PASS explicit AD0 inherited A/D ignoring, unchanged backing and strict compatibility");
}
