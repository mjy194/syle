#include "../nested_ept.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static uint64_t tables[4][512] __attribute__((aligned(4096)));
struct fixture {
    uint8_t perms[5];
    int missing;
    bool bad_pointer,bad_hpa,high_hpa;
    unsigned reads,leaf_reads;
};
static bool resolve(void *context,uint64_t gpa,uint8_t need,uint64_t *hpa,
    const uint64_t **pointer,uint8_t *perm) {
    struct fixture *f=context;
    assert(!(gpa&0xfff));
    int index=gpa>=0x1000 && gpa<=0x4000?(int)(gpa/0x1000)-1:gpa==0x8000?4:-1;
    if(index<0 || index==f->missing) return false;
    f->reads++;
    *perm=f->perms[index];
    *hpa=UINT64_C(0x100000)+(uint64_t)index*4096;
    if(f->bad_hpa) *hpa|=1;
    if(f->high_hpa) *hpa|=UINT64_C(1)<<48;
    if(index<4) {
        assert(need==1);
        *pointer=tables[index]+(f->bad_pointer?1:0);
    } else {
        assert(need==1 || need==2 || need==4);
        f->leaf_reads++;
        *pointer=NULL; /* leaf never needs a pointer */
        *hpa=0x800000;
    }
    return true;
}
static struct fixture init(void) {
    memset(tables,0,sizeof tables);
    tables[0][0]=0x2007;
    tables[1][0]=0x3007;
    tables[2][0]=0x4007;
    tables[3][15]=0x8037; /* L2 f000 aliases L1 8000, WB/RWX */
    return (struct fixture){.perms={7,7,7,7,7},.missing=-1};
}
static struct nested_ept_result walk(struct fixture *f,uint8_t access) {
    return nested_ept_walk(0x101e,0xf123,access,resolve,f);
}
int main(void) {
    assert(nested_invept_check(1,0x1001e,0)==NESTED_INVEPT_VALID);
    assert(nested_invept_check(UINT64_C(0x100000001),0x1001e,0)==NESTED_INVEPT_INVALID);
    assert(nested_invept_check(1,0x1001e,1)==NESTED_INVEPT_INVALID);
    assert(nested_invept_check(1,0x1005e,0)==NESTED_INVEPT_INVALID);
    assert(nested_invept_check(1,0x1101e,0)==NESTED_INVEPT_UNSUPPORTED);

    struct fixture f=init();
    struct nested_ept_result r=walk(&f,1);
    assert(r.status==NESTED_EPT_OK && r.leaf_resolved);
    assert(r.l1_gpa==0x8123 && r.hpa==0x800123);
    assert(r.effective12==7 && r.effective02==7 && f.reads==5);
    /* Every EPT12 level contributes permissions, not just the leaf. */
    for(unsigned level=0;level<4;level++) {
        f=init();unsigned slot=level==3?15:0;
        tables[level][slot]&=~UINT64_C(6); /* R only */
        r=walk(&f,1);assert(r.status==NESTED_EPT_OK && r.effective12==1 && r.effective02==1);
        f.leaf_reads=0;r=walk(&f,2);
        assert(r.status==NESTED_EPT_L1_DENIED && r.effective12==1);
        assert(r.l1_gpa==0x8123 && r.leaf_resolved && f.leaf_reads==0);
        r=walk(&f,4);assert(r.status==NESTED_EPT_L1_DENIED);
    }
    f=init();tables[0][0]&=~UINT64_C(2);tables[1][0]&=~UINT64_C(4);
    r=walk(&f,1);assert(r.effective12==1 && r.status==NESTED_EPT_OK);
    f=init();f.perms[4]=5;r=walk(&f,2);
    assert(r.status==NESTED_EPT_L0_BACKING_DENIED && r.effective12==7 && r.effective02==5);
    assert(r.hpa==0x800123 && r.l1_gpa==0x8123);
    r=walk(&f,4);assert(r.status==NESTED_EPT_OK && r.effective02==5);
    /* Table EPT01 permissions gate reading tables, not L2 final execute. */
    f=init();for(unsigned i=0;i<4;i++) f.perms[i]=1;
    r=walk(&f,4);assert(r.status==NESTED_EPT_OK && r.effective02==7);

    for(unsigned level=0;level<4;level++) {
        unsigned slot=level==3?15:0;
        f=init();tables[level][slot]=(tables[level][slot]&~UINT64_C(7))|2;
        assert(walk(&f,1).status==NESTED_EPT_MALFORMED_UNSUPPORTED); /* W only */
        f=init();tables[level][slot]=(tables[level][slot]&~UINT64_C(7))|4;
        assert(walk(&f,4).status==NESTED_EPT_MALFORMED_UNSUPPORTED); /* X only */
        const unsigned bad_bits[]={6,7,8,9,10,11,48,63};
        for(unsigned i=0;i<sizeof bad_bits/sizeof *bad_bits;i++) {
            f=init();tables[level][slot]|=UINT64_C(1)<<bad_bits[i];
            assert(walk(&f,1).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        }
        f=init();tables[level][slot]=UINT64_C(0xfffffffffffffff8);
        r=walk(&f,1);assert(r.status==NESTED_EPT_L1_DENIED && r.effective12==0);
        assert(!r.leaf_resolved && f.leaf_reads==0); /* nonpresent ignores other bits */
    }
    f=init();tables[3][15]&=~UINT64_C(0x38); /* UC unsupported */
    assert(walk(&f,1).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    f=init();tables[0][0]|=0x30; /* memory-type bits reserved in nonleaf */
    assert(walk(&f,1).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    const uint64_t bad_eptp[]={0x101a,0x101f,0x105e,0x109e,0x100000000101eULL,0x800000000000101eULL};
    for(unsigned i=0;i<sizeof bad_eptp/sizeof *bad_eptp;i++) {
        f=init();
        assert(nested_ept_walk(bad_eptp[i],0xf000,1,resolve,&f).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
        assert(f.reads==0);
    }
    f=init();
    assert(nested_ept_walk(0x101e,UINT64_C(1)<<48,1,resolve,&f).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_walk(0x101e,0xf000,3,resolve,&f).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_walk(0x101e,0xf000,0,resolve,&f).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_walk(0x101e,0xf000,1,NULL,&f).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    for(int missing=0;missing<5;missing++) {
        f=init();f.missing=missing;
        assert(walk(&f,1).status==NESTED_EPT_L0_BACKING_DENIED);
    }
    for(unsigned i=0;i<4;i++) {
        f=init();f.perms[i]=0;
        assert(walk(&f,1).status==NESTED_EPT_L0_BACKING_DENIED);
    }
    f=init();f.bad_pointer=true;
    assert(walk(&f,1).status==NESTED_EPT_L0_BACKING_DENIED);
    f=init();f.bad_hpa=true;
    assert(walk(&f,1).status==NESTED_EPT_L0_BACKING_DENIED);
    f=init();f.high_hpa=true;
    assert(walk(&f,1).status==NESTED_EPT_L0_BACKING_DENIED);
    /* Exact page boundary is resolved independently; no borrowed pointer. */
    f=init();r=nested_ept_walk(0x101e,0xffff,1,resolve,&f);
    assert(r.status==NESTED_EPT_OK && r.hpa==0x800fff);
    r=nested_ept_walk(0x101e,0x10000,1,resolve,&f);
    assert(r.status==NESTED_EPT_L1_DENIED);
    puts("PASS nested EPT12 walk / EPT02 composition policy");
    /* Opt-in memory parsing does not imply hardware admission. */
    const struct nested_ept_policy memory_policy={48,true,true};
    for(unsigned wt=0;wt<8;wt++) for(unsigned mt=0;mt<8;mt++)
    for(unsigned ipat=0;ipat<2;ipat++) {
        f=init();
        for(unsigned d=0;d<3;d++)tables[d][0]|=0x300;
        tables[3][15]=0x8007|(mt<<3)|(ipat<<6)|0x300;
        r=nested_ept_walk_memory(0x1018|wt,0xf123,1,resolve,&f,&memory_policy);
        bool legal=(wt==0||wt==6)&&(mt==0||mt==1||mt==4||mt==5||mt==6);
        assert((r.status==NESTED_EPT_OK)==legal);
        if(legal)assert(r.memory_explicit&&r.leaf_type==mt&&r.ignore_pat==(bool)ipat&&r.walk_type==wt);
        assert(walk(&f,1).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    }
    f=init();r=walk(&f,1);assert(!r.memory_explicit);
    tables[0][0]|=0x40; /* IPAT is leaf-only */
    assert(nested_ept_walk_memory(0x101e,0xf123,1,resolve,&f,&memory_policy).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    f=init();assert(nested_ept_walk_memory(0x105e,0xf123,1,resolve,&f,&memory_policy).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    puts("PASS explicit EPT memory types and unchanged strict wrapper");
}
