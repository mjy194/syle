#include "../nested_ept_sparse.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define POOL 128
static _Alignas(4096) unsigned char memory[POOL][4096];
struct allocator {bool used[POOL],flush_ok;unsigned live,calls,fail_at,flushes;bool high_hpa,bad_view;};
static bool allocate(void *context,struct nested_sparse_page *p) {
    struct allocator *a=context;
    if(a->calls++==a->fail_at) return false;
    for(unsigned i=0;i<POOL;i++) if(!a->used[i]) {
        a->used[i]=true;a->live++;
        *p=(struct nested_sparse_page){.hpa=0x100000+(uint64_t)i*4096,
            .view=memory[i]+(a->bad_view?1:0)};
        if(a->high_hpa) p->hpa|=UINT64_C(1)<<48;
        return true;
    }
    return false;
}
static void release(void *context,struct nested_sparse_page p) {
    struct allocator *a=context;
    uint64_t hpa=p.hpa&~(UINT64_C(1)<<48);
    assert(hpa>=0x100000&&!(hpa&4095));
    unsigned i=(unsigned)((hpa-0x100000)/4096);
    assert(i<POOL&&a->used[i]);a->used[i]=false;a->live--;
}
static bool flush(void *context,uint64_t eptp) {
    struct allocator *a=context;assert((eptp&4095)==0x1e);
    a->flushes++;return a->flush_ok;
}
static const struct nested_sparse_ops ops={allocate,release,flush};
static struct allocator fresh(void) {
    memset(memory,0,sizeof memory);
    return (struct allocator){.flush_ok=true,.fail_at=~0u};
}
static struct nested_ept_result resolved(uint8_t p12,uint8_t p02) {
    return (struct nested_ept_result){.status=NESTED_EPT_OK,.l1_gpa=0x8000,.hpa=0x800000,
        .effective12=p12,.effective02=p02,.leaf_resolved=true};
}
static uint64_t leaf_hw(const struct nested_ept_sparse *t,uint64_t gpa) {
    uint64_t hpa=t->eptp&~UINT64_C(4095);
    for(unsigned depth=0;depth<4;depth++) {
        unsigned i=(unsigned)((hpa-0x100000)/4096);
        assert(i<POOL);
        uint64_t e=((uint64_t *)memory[i])[(gpa>>(39-9*depth))&511];
        if(depth==3 || !e) return e;
        assert((e&4095)==7);hpa=e&~UINT64_C(4095);
    }
    assert(0);return 0;
}
static uint64_t checksum(const struct allocator *a,const bool *old_used) {
    uint64_t x=0;
    for(unsigned i=0;i<POOL;i++) if(old_used[i]) {
        assert(a->used[i]);
        for(unsigned j=0;j<4096;j++) x=(x*131)^memory[i][j];
    }
    return x;
}
int main(void) {
    struct allocator a=fresh();struct nested_ept_sparse t;
    assert(!nested_ept_sparse_init(&t,&ops,&a,49));
    assert(!nested_ept_sparse_init(&t,&ops,&a,11));
    assert(!nested_ept_sparse_init(&t,NULL,&a,48));
    for(unsigned fail=0;fail<2;fail++) {
        a=fresh();a.fail_at=fail;
        assert(!nested_ept_sparse_init(&t,&ops,&a,48));assert(a.live==0);
    }
    a=fresh();a.high_hpa=true;
    assert(!nested_ept_sparse_init(&t,&ops,&a,48)&&a.live==0);
    a=fresh();a.bad_view=true;
    assert(!nested_ept_sparse_init(&t,&ops,&a,48)&&a.live==0);
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    uint64_t root=t.eptp;assert(a.live==2&&t.generation==1);
    struct nested_ept_result r=resolved(7,7),out={.hpa=123};
    const uint64_t addresses[]={0x1000,0x201000,0x40001000,0x8000001000};
    for(unsigned i=0;i<4;i++) {
        assert(nested_ept_sparse_fill(&t,addresses[i],&r)==NESTED_SPARSE_OK);
        assert(leaf_hw(&t,addresses[i])==0x800037);
        assert(nested_ept_sparse_lookup(&t,addresses[i]+123,7,&out)==NESTED_SPARSE_HIT);
        assert(out.hpa==0x80007b&&out.l1_gpa==0x807b&&out.status==NESTED_EPT_OK);
    }
    assert(a.live==24&&t.fill_count==4&&t.generation==1);
    /* Cache hit must agree with the real owned EPT02 path and leaf. */
    uint64_t path_hpa=t.eptp&~UINT64_C(4095);
    for(unsigned depth=0;depth<3;depth++)
        path_hpa=((uint64_t *)memory[(path_hpa-0x100000)/4096])[0]&~UINT64_C(4095);
    uint64_t *entry=&((uint64_t *)memory[(path_hpa-0x100000)/4096])[1];
    uint64_t saved_entry=*entry;
    *entry^=2;out.hpa=123;
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP&&out.hpa==123);
    *entry=saved_entry;
    uint64_t *root_entry=(uint64_t *)memory[((t.eptp&~UINT64_C(4095))-0x100000)/4096];
    saved_entry=root_entry[0];root_entry[0]=0xdead0007;
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP);
    root_entry[0]=saved_entry;
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_HIT);
    unsigned live=a.live,fl=a.flushes;
    r.hpa=0x900000;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_EXISTS);
    assert(a.live==live&&a.flushes==fl&&leaf_hw(&t,0x1000)==0x800037);
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_HIT&&out.hpa==0x800000);
    assert(nested_ept_sparse_lookup(&t,0x7000,1,&out)==NESTED_SPARSE_MISS);
    assert(nested_ept_sparse_lookup(&t,UINT64_C(1)<<48,1,&out)==NESTED_SPARSE_BAD_LOOKUP);
    assert(nested_ept_sparse_lookup(&t,0x1000,8,&out)==NESTED_SPARSE_BAD_LOOKUP);
    assert(nested_ept_sparse_lookup(&t,0x1000,0,&out)==NESTED_SPARSE_BAD_LOOKUP);
    assert(nested_ept_sparse_reset(&t));
    assert(t.eptp==root&&t.generation==2&&t.fill_count==0&&a.live==2);
    for(unsigned i=0;i<4;i++) {
        assert(leaf_hw(&t,addresses[i])==0);
        assert(nested_ept_sparse_lookup(&t,addresses[i],1,&out)==NESTED_SPARSE_MISS);
    }
    r=resolved(5,1);assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    assert(nested_ept_sparse_lookup(&t,0x1000,3,&out)==NESTED_SPARSE_HIT&&out.status==NESTED_EPT_L1_DENIED);
    assert(nested_ept_sparse_lookup(&t,0x1000,5,&out)==NESTED_SPARSE_HIT&&out.status==NESTED_EPT_L0_BACKING_DENIED);
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_HIT&&out.status==NESTED_EPT_OK);
    for(unsigned i=0;i<8;i++) {
        r=resolved(7,7);
        if(i==0) r.hpa|=1;
        if(i==1) r.hpa=UINT64_C(1)<<48;
        if(i==2) r.effective02=2;
        if(i==3) r.effective12=4;
        if(i==4) r.effective12=1;
        if(i==5) r.leaf_resolved=false;
        if(i==6) r.status=NESTED_EPT_L0_BACKING_DENIED;
        if(i==7) r.l1_gpa=UINT64_C(1)<<48;
        live=a.live;assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_INVALID);
        assert(a.live==live);
    }
    r=resolved(7,7);
    assert(nested_ept_sparse_fill(&t,0x2001,&r)==NESTED_SPARSE_INVALID);
    assert(nested_ept_sparse_fill(&t,UINT64_C(1)<<48,&r)==NESTED_SPARSE_INVALID);
    assert(nested_ept_sparse_destroy(&t)&&a.live==0);

    /* Fail each allocation on an entirely new PML4 branch: old tables AND
     * metadata bytes remain unchanged, all unpublished pages are freed. */
    for(unsigned fail=0;fail<7;fail++) {
        a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
        assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
        bool old_used[POOL];memcpy(old_used,a.used,sizeof old_used);
        uint64_t sum=checksum(&a,old_used);live=a.live;fl=a.flushes;
        a.fail_at=a.calls+fail;
        assert(nested_ept_sparse_fill(&t,0x8000001000,&r)==NESTED_SPARSE_OOM);
        assert(a.live==live&&a.flushes==fl&&t.fill_count==1&&t.generation==1&&!t.poisoned);
        assert(checksum(&a,old_used)==sum&&leaf_hw(&t,0x8000001000)==0);
        assert(nested_ept_sparse_destroy(&t)&&a.live==0);
    }
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,32));
    assert(nested_ept_sparse_fill(&t,0xfffff000,&r)==NESTED_SPARSE_OK);
    assert(nested_ept_sparse_fill(&t,UINT64_C(1)<<32,&r)==NESTED_SPARSE_INVALID);
    r.hpa=UINT64_C(1)<<32;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);
    assert(nested_ept_sparse_destroy(&t)&&a.live==0);

    a=fresh();r=resolved(7,7);assert(nested_ept_sparse_init(&t,&ops,&a,48));
    a.flush_ok=false;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_FLUSH_FAILED);
    live=a.live;assert(t.poisoned&&t.generation==1&&t.fill_count==0&&live==9);
    assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_POISONED);
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_LOOKUP_POISONED);
    assert(!nested_ept_sparse_reset(&t));
    assert(!nested_ept_sparse_destroy(&t)&&a.live==live&&t.generation==1);
    a.flush_ok=true;assert(nested_ept_sparse_destroy(&t)&&a.live==0);

    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    live=a.live;a.flush_ok=false;
    assert(!nested_ept_sparse_reset(&t)&&t.poisoned&&a.live==live&&t.generation==1);
    assert(leaf_hw(&t,0x1000)==0); /* root cleared, ownership deliberately held */
    a.flush_ok=true;assert(!nested_ept_sparse_reset(&t));
    assert(nested_ept_sparse_destroy(&t)&&a.live==0);
    a=fresh();a.flush_ok=false;
    assert(!nested_ept_sparse_init(&t,&ops,&a,48));
    assert(a.live==2&&t.root&&t.poisoned&&t.generation==0);
    assert(!nested_ept_sparse_reset(&t));
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_POISONED);
    assert(!nested_ept_sparse_destroy(&t)&&a.live==2);
    a.flush_ok=true;assert(nested_ept_sparse_destroy(&t)&&a.live==0);
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    r=resolved(7,7);assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    live=a.live;fl=a.flushes;a.fail_at=a.calls; /* existing PT, leaf metadata only */
    assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_OOM);
    assert(a.live==live&&a.flushes==fl&&t.generation==1&&t.fill_count==1);
    a.fail_at=~0u;
    t.fill_count=UINT64_MAX;
    assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_INVALID);
    assert(a.live==live&&a.flushes==fl&&leaf_hw(&t,0x1000)==0x800037);
    t.fill_count=1;t.generation=UINT64_MAX;
    assert(!nested_ept_sparse_reset(&t));
    assert(a.live==live&&a.flushes==fl&&leaf_hw(&t,0x1000)==0x800037);
    assert(nested_ept_sparse_destroy(&t)&&a.live==0);
    /* Regression: read-walk L0 backing failure stays fatal even when
     * the actual write also lacks an EPT12 permission. */
    r=resolved(1,0);r.status=NESTED_EPT_L0_BACKING_DENIED;
    out=nested_sparse_walk_access(r,2);
    assert(out.status==NESTED_EPT_L0_BACKING_DENIED);
    r.status=NESTED_EPT_MALFORMED_UNSUPPORTED;
    assert(nested_sparse_walk_access(r,2).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    r=resolved(1,1);
    assert(nested_sparse_walk_access(r,2).status==NESTED_EPT_L1_DENIED);
    r=resolved(3,1);
    assert(nested_sparse_walk_access(r,3).status==NESTED_EPT_L0_BACKING_DENIED);
    r=resolved(3,3);
    assert(nested_sparse_walk_access(r,3).status==NESTED_EPT_OK);
    assert(nested_sparse_walk_access(r,0).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    puts("PASS sparse EPT tree, cached permissions, OOM rollback and poison ownership");
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    r=resolved(7,7);r.memory_explicit=true;r.leaf_type=6;r.walk_type=6;r.ignore_pat=true;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    assert(leaf_hw(&t,0x1000)==0x800077);
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_HIT&&out.memory_explicit&&out.ignore_pat);
    uint64_t memory_path=t.eptp&~UINT64_C(4095);
    for(unsigned d=0;d<3;d++)memory_path=((uint64_t*)memory[(memory_path-0x100000)/4096])[0]&~UINT64_C(4095);
    uint64_t *memory_leaf=&((uint64_t*)memory[(memory_path-0x100000)/4096])[1];
    *memory_leaf^=0x40;
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP);
    *memory_leaf^=0x40;
    for(unsigned mt=0;mt<8;mt++) if(mt!=6) {
        r.leaf_type=mt;assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_INVALID);
    }
    r.leaf_type=6;r.walk_type=0;
    assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_INVALID);
    r.walk_type=6;r.ignore_pat=false;
    assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_INVALID);
    assert(nested_ept_sparse_destroy(&t)&&!a.live);
    puts("PASS explicit safe WB/IPAT sparse admission and tamper checking");
    return 0;
}
