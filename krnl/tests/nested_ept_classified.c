#include "../nested_ept.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t tables[4][512] __attribute__((aligned(4096)));
static unsigned reads;
static bool missing;static uint8_t backing;
static bool resolve(void *ctx,uint64_t pa,uint8_t need,uint64_t *hpa,const uint64_t **view,uint8_t *perm)
{
    (void)ctx;(void)need;reads++;
    if(pa>=0x1000&&pa<=0x4000){*hpa=pa+0x100000;*view=tables[pa/4096-1];*perm=7;return true;}
    if(missing)return false;
    *hpa=0x900000;*view=0;*perm=backing;return true;
}
static void init(void)
{
    memset(tables,0,sizeof tables);reads=0;missing=false;backing=7;
    tables[0][0]=0x2007;tables[1][0]=0x3007;tables[2][0]=0x4007;tables[3][0]=0x8037;
}
static struct nested_ept_result walk(uint8_t access)
{
    const struct nested_ept_policy p={48,true,true};
    return nested_ept_walk_classified(0x101e,0x123,access,resolve,0,&p);
}
int main(void)
{
    assert(nested_misconfig_repair_pending(0,1));
    assert(nested_misconfig_repair_pending(1,2)); /* failed hardware candidate regression */
    assert(nested_misconfig_repair_pending(2,3));
    assert(!nested_misconfig_repair_pending(1,3));
    assert(!nested_misconfig_repair_pending(3,4));
    assert(!nested_misconfig_repair_pending(0,0));
    assert(!nested_misconfig_repair_pending(~0u,0));

    init();struct nested_ept_result r=walk(1);assert(r.status==NESTED_EPT_OK&&r.hpa==0x900123);
    for(unsigned level=0;level<4;level++) {
        init();tables[level][0]=UINT64_MAX&~7ull;assert(walk(1).status==NESTED_EPT_L1_DENIED);assert(reads==level+1);
        init();tables[level][0]=(tables[level][0]&~7ull)|2;assert(walk(1).status==NESTED_EPT_MISCONFIG);
        init();tables[level][0]|=1ull<<48;assert(walk(1).status==NESTED_EPT_MISCONFIG);
        init();tables[level][0]|=0x300;assert(walk(1).status==NESTED_EPT_OK);
        init();tables[level][0]|=1ull<<52;assert(walk(1).status==NESTED_EPT_UNSUPPORTED);
        init();tables[level][0]=(tables[level][0]&~7ull)|4;assert(walk(4).status==NESTED_EPT_UNSUPPORTED);
    }
    for(unsigned level=0;level<3;level++)for(unsigned bit=3;bit<7;bit++) {
        init();tables[level][0]|=1ull<<bit;assert(walk(1).status==NESTED_EPT_MISCONFIG);
    }
    init();tables[0][0]|=0x80;assert(walk(1).status==NESTED_EPT_MISCONFIG);
    init();tables[3][0]|=0x80;assert(walk(1).status==NESTED_EPT_OK);
    for(unsigned mt=0;mt<8;mt++) {
        init();tables[3][0]=(tables[3][0]&~0x38ull)|(mt<<3);
        enum nested_ept_status expected=mt==6?NESTED_EPT_OK:
            (mt==2||mt==3||mt==7)?NESTED_EPT_MISCONFIG:NESTED_EPT_UNSUPPORTED;
        assert(walk(1).status==expected);
    }
    init();tables[3][0]|=0x40;assert(walk(1).status==NESTED_EPT_UNSUPPORTED);
    for(unsigned level=1;level<3;level++) {
        init();tables[level][0]=0xb7;assert(walk(1).status==NESTED_EPT_OK);
        tables[level][0]|=0x1000;assert(walk(1).status==NESTED_EPT_MISCONFIG);
        tables[level][0]=0xb7;
        const struct nested_ept_policy p={48,false,false};
        assert(nested_ept_walk_classified(0x101e,0x123,1,resolve,0,&p).status==NESTED_EPT_UNSUPPORTED);
    }
    init();tables[3][0]&=~2ull;assert(walk(2).status==NESTED_EPT_L1_DENIED);
    init();backing=1;assert(walk(2).status==NESTED_EPT_L0_BACKING_DENIED);
    init();missing=true;assert(walk(1).status==NESTED_EPT_L0_BACKING_DENIED);
    init();tables[3][0]|=0x80;
    assert(nested_ept_walk(0x101e,0x123,1,resolve,0).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    init();tables[3][0]=0x8002;
    assert(nested_ept_walk(0x101e,0x123,1,resolve,0).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    const struct nested_ept_policy p={32,true,true};
    init();tables[3][0]|=1ull<<32;
    assert(nested_ept_walk_classified(0x101e,0x123,1,resolve,0,&p).status==NESTED_EPT_MISCONFIG);
    assert(nested_ept_walk_classified(0x105e,0x123,1,resolve,0,&p).status==NESTED_EPT_UNSUPPORTED);
    assert(nested_ept_walk_classified(0x101e,1ull<<32,1,resolve,0,&p).status==NESTED_EPT_UNSUPPORTED);
    puts("nested_ept_classified: PASS");
}
