#include "../nested_ept.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t tables[4][512] __attribute__((aligned(4096)));
struct source {
    uint64_t expected_page,leaf_hpa,table_hpa;
    uint8_t leaf_perm;
    bool missing;
    unsigned reads,leaf_reads;
};
static bool resolve(void *context,uint64_t gpa,uint8_t need,uint64_t *hpa,
    const uint64_t **view,uint8_t *perm) {
    struct source *s=context;s->reads++;
    assert(!(gpa&4095));
    if(gpa>=0x1000&&gpa<=0x4000) {
        assert(need==1);
        unsigned i=(unsigned)(gpa/4096)-1;
        *hpa=s->table_hpa+i*4096;*view=tables[i];*perm=7;return true;
    }
    s->leaf_reads++;
    assert(gpa==s->expected_page); /* must resolve the derived 4K page */
    if(s->missing) return false;
    *hpa=s->leaf_hpa;*view=NULL;*perm=s->leaf_perm;return true;
}
static struct source init(bool one_g) {
    memset(tables,0,sizeof tables);
    tables[0][0]=0x2007;
    if(one_g) tables[1][1]=UINT64_C(0x80000000)|0xb7;
    else {tables[1][0]=0x3007;tables[2][1]=0x400000|0xb7;}
    return (struct source){.expected_page=one_g?0x80003000:0x401000,
        .leaf_hpa=0x900000,.table_hpa=0x100000,.leaf_perm=7};
}
static struct nested_ept_result walk(struct source *s,bool one_g,uint8_t access,
    const struct nested_ept_policy *p) {
    return nested_ept_walk_policy(0x101e,one_g?0x40003123:0x201123,access,resolve,s,p);
}
int main(void) {
    struct nested_ept_policy p={48,true,true};
    for(unsigned mode=0;mode<2;mode++) {
        bool one_g=mode!=0;
        struct source s=init(one_g);
        struct nested_ept_result r=walk(&s,one_g,1,&p);
        assert(r.status==NESTED_EPT_OK&&r.hpa==0x900123&&r.l1_gpa==s.expected_page+0x123);
        assert(r.effective12==7&&r.effective02==7&&r.leaf_resolved&&s.leaf_reads==1);
        assert(s.reads==(one_g?3u:4u));
        /* Adjacent L1 4K page can map to unrelated HPA. No large HPA claim. */
        s.expected_page+=4096;s.leaf_hpa=0xb00000;
        r=nested_ept_walk_policy(0x101e,(one_g?0x40004123:0x202123),1,resolve,&s,&p);
        assert(r.status==NESTED_EPT_OK&&r.hpa==0xb00123&&r.l1_gpa==s.expected_page+0x123);
        /* Last byte of a large leaf, then the first byte of its adjacent
         * leaf: require a new entry and preserve the final 4K page offset. */
        s=init(one_g);
        uint64_t size=UINT64_C(1)<<(one_g?30:21);
        uint64_t base=one_g?UINT64_C(0x80000000):UINT64_C(0x400000);
        s.expected_page=base+size-4096;
        r=nested_ept_walk_policy(0x101e,2*size-1,1,resolve,&s,&p);
        assert(r.status==NESTED_EPT_OK&&r.l1_gpa==base+size-1&&r.hpa==s.leaf_hpa+4095);
        tables[one_g?1:2][2]=(base+2*size)|0xb7;
        s.expected_page=base+2*size;s.leaf_hpa=0xd00000;
        r=nested_ept_walk_policy(0x101e,2*size,1,resolve,&s,&p);
        assert(r.status==NESTED_EPT_OK&&r.l1_gpa==s.expected_page&&r.hpa==s.leaf_hpa);
        s=init(one_g);tables[0][0]&=~UINT64_C(2);
        r=walk(&s,one_g,2,&p);
        assert(r.status==NESTED_EPT_L1_DENIED&&r.effective12==5&&s.leaf_reads==0);
        assert(r.l1_gpa==s.expected_page+0x123);
        s=init(one_g);s.leaf_perm=1;r=walk(&s,one_g,4,&p);
        assert(r.status==NESTED_EPT_L0_BACKING_DENIED&&r.effective12==7&&r.effective02==1);
        s=init(one_g);s.missing=true;
        assert(walk(&s,one_g,1,&p).status==NESTED_EPT_L0_BACKING_DENIED);
        s=init(one_g);s.leaf_hpa=UINT64_C(1)<<48;
        assert(walk(&s,one_g,1,&p).status==NESTED_EPT_L0_BACKING_DENIED);
        s=init(one_g);s.table_hpa=UINT64_C(1)<<48;
        assert(walk(&s,one_g,1,&p).status==NESTED_EPT_L0_BACKING_DENIED);

        uint64_t *leaf=&tables[one_g?1:2][1];
        const unsigned bad_bits[]={6,8,9,10,11,48,63};
        for(unsigned i=0;i<sizeof bad_bits/sizeof *bad_bits;i++) {
            s=init(one_g);*leaf|=UINT64_C(1)<<bad_bits[i];
            assert(walk(&s,one_g,1,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        }
        /* Every reserved address-alignment bit is rejected. */
        for(unsigned bit=12;bit<(one_g?30u:21u);bit++) {
            s=init(one_g);*leaf|=UINT64_C(1)<<bit;
            assert(walk(&s,one_g,1,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        }
        s=init(one_g);*leaf&=~UINT64_C(0x38);
        assert(walk(&s,one_g,1,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED); /* non-WB */
        s=init(one_g);*leaf=(*leaf&~UINT64_C(7))|2;
        assert(walk(&s,one_g,1,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        s=init(one_g);*leaf=(*leaf&~UINT64_C(7))|4;
        assert(walk(&s,one_g,4,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        s=init(one_g);*leaf=UINT64_C(0xfffffffffffffff8);
        assert(walk(&s,one_g,1,&p).status==NESTED_EPT_L1_DENIED); /* junk ignored */
        s=init(one_g);
        struct nested_ept_policy disabled={48,!one_g,one_g};
        if(one_g) disabled.allow_1g=false;else disabled.allow_2m=false;
        assert(walk(&s,one_g,1,&disabled).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        assert(nested_ept_walk(0x101e,one_g?0x40003123:0x201123,1,resolve,&s).status==
               NESTED_EPT_MALFORMED_UNSUPPORTED); /* old strict wrapper */
    }
    struct source s=init(false);tables[0][0]=0xb7;
    assert(walk(&s,false,1,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED); /* PS/PML4 */
    s=init(false);tables[2][1]=0x4007;tables[3][1]=0x4010b7;
    assert(walk(&s,false,1,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED); /* PS/PT */
    s=init(false);tables[0][0]=UINT64_C(0xfffffffffffffff8);
    assert(walk(&s,false,1,&p).status==NESTED_EPT_L1_DENIED);

    struct nested_ept_policy p32={32,true,true};
    s=init(true);assert(walk(&s,true,1,&p32).status==NESTED_EPT_OK);
    s=init(true);tables[1][1]|=UINT64_C(1)<<32;
    assert(walk(&s,true,1,&p32).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    s=init(true);s.leaf_hpa=UINT64_C(1)<<32;
    assert(walk(&s,true,1,&p32).status==NESTED_EPT_L0_BACKING_DENIED);
    s=init(true);
    assert(nested_ept_walk_policy(0x10000101eULL,0x40003123,1,resolve,&s,&p32).status==
           NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_walk_policy(0x101e,UINT64_C(1)<<32,1,resolve,&s,&p32).status==
           NESTED_EPT_MALFORMED_UNSUPPORTED);
    struct nested_ept_policy invalid={11,true,true};
    assert(walk(&s,true,1,&invalid).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    invalid.maxphys=49;
    assert(walk(&s,true,1,&invalid).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(walk(&s,true,1,NULL).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    s=init(false);
    assert(nested_ept_walk_policy(0x105e,0x201123,1,resolve,&s,&p).status==
           NESTED_EPT_MALFORMED_UNSUPPORTED); /* EPTP A/D */
    assert(walk(&s,false,3,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED); /* unchanged access API */
    puts("PASS policy EPT12 2M/1G leaf walk and 4K backing composition");
}
