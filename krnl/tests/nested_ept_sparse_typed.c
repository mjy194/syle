#include "../nested_ept_sparse.h"
#include "../nested_memory_type.h"
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
    struct allocator *a=context;assert((eptp&4095)==0x1e||(eptp&4095)==0x5e);
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
struct lease {uint64_t hpa,pinned_epoch,current_epoch;uint8_t type;bool live;unsigned calls;};
static bool admit(void *ctx,const struct nested_ept_result *r) {
    struct lease *l=ctx;l->calls++;
    return l->live&&l->hpa==r->hpa&&l->pinned_epoch==l->current_epoch&&l->type==r->leaf_type;
}
struct pat_lease {struct lease pin;uint64_t pat;};
static bool admit_pat(void *ctx,const struct nested_ept_result *r) {
    struct pat_lease *p=ctx;
    return admit(&p->pin,r)&&nested_memory_backing_compatible(r->leaf_type,
        r->ignore_pat,false,false,p->pat,p->pin.type);
}
int main(void) {
    struct allocator a=fresh();struct nested_ept_sparse t;
    struct nested_ept_result r=resolved(3,3),out={.hpa=123};
    r.memory_explicit=true;r.ignore_pat=true;r.walk_type=6;r.leaf_type=0;
    assert(nested_ept_sparse_init(&t,&ops,&a,48));
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);
    struct lease l={.hpa=r.hpa,.pinned_epoch=1,.current_epoch=1,.type=0,.live=true};
    assert(nested_ept_sparse_set_typed_admission(&t,admit,&l));
    assert(!nested_ept_sparse_set_typed_admission(&t,admit,&l));
    unsigned calls=a.calls;
    l.live=false;assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);assert(a.calls==calls);
    l.live=true;l.hpa+=4096;assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);assert(a.calls==calls);l.hpa-=4096;
    r.memory_explicit=false;assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);r.memory_explicit=true;
    r.ignore_pat=false;assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);r.ignore_pat=true;
    r.walk_type=0;assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);r.walk_type=6;
    r.leaf_type=4;assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);r.leaf_type=0;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    assert(leaf_hw(&t,0x1000)==(r.hpa|0x43));
    assert(nested_ept_sparse_lookup(&t,0x1123,3,&out)==NESTED_SPARSE_HIT&&out.leaf_type==0&&out.hpa==r.hpa+0x123);
    bool tracked=true;assert(nested_ept_sparse_query_tracking(&t,0x1000,&tracked)&&!tracked);
    l.current_epoch++;
    out.hpa=123;assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP&&out.hpa==123);
    tracked=true;assert(!nested_ept_sparse_query_tracking(&t,0x1000,&tracked)&&tracked);
    assert(!nested_ept_sparse_promote_write(&t,0x1000));
    assert(nested_ept_sparse_reset(&t));l.pinned_epoch=l.current_epoch;
    assert(!nested_ept_sparse_set_typed_admission(&t,admit,&l));
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_HIT);
    assert(nested_ept_sparse_destroy(&t));assert(!a.live);
    /* Default strict WB mode and encoding remain unchanged. */
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    r.leaf_type=6;assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    assert(leaf_hw(&t,0x1000)==(r.hpa|0x73));
    assert(!nested_ept_sparse_set_typed_admission(&t,admit,&l));
    assert(nested_ept_sparse_destroy(&t));
    /* A/D UC admission is disabled, even without trap_write. */
    a=fresh();assert(nested_ept_sparse_init_mode(&t,&ops,&a,48,true));
    assert(nested_ept_sparse_set_typed_admission(&t,admit,&l));
    r.leaf_type=0;l.type=0;calls=a.calls;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);
    assert(nested_ept_sparse_fill_tracked(&t,0x1000,&r,true)==NESTED_SPARSE_INVALID&&a.calls==calls);
    /* Typed WB also invokes callback, including cached promotion. */
    r.leaf_type=6;l.type=6;
    assert(nested_ept_sparse_fill_tracked(&t,0x1000,&r,true)==NESTED_SPARSE_OK);
    l.live=false;assert(!nested_ept_sparse_promote_write(&t,0x1000));
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP);
    l.live=true;assert(nested_ept_sparse_promote_write(&t,0x1000));
    assert(nested_ept_sparse_destroy(&t)&&!a.live);
    /* PAT-sensitive admission requires explicit new opt-in; the original
     * setter still rejects IPAT0 even when the callback would allow it. */
    struct pat_lease pl={.pin={.hpa=0x800000,.pinned_epoch=3,.current_epoch=3,
        .type=0,.live=true},.pat=UINT64_C(0x0007040600070406)};
    r=resolved(3,3);r.memory_explicit=true;r.walk_type=6;r.leaf_type=0;r.ignore_pat=false;
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    assert(nested_ept_sparse_set_typed_admission(&t,admit_pat,&pl));
    calls=a.calls;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID&&a.calls==calls);
    assert(nested_ept_sparse_destroy(&t));
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    assert(nested_ept_sparse_set_typed_admission_pat(&t,admit_pat,&pl));
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    assert(leaf_hw(&t,0x1000)==(r.hpa|3)); /* UC RW; IPAT bit6 must stay zero */
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_HIT);
    assert(out.leaf_type==0&&!out.ignore_pat);
    const uint64_t good_pat=pl.pat,old_leaf=leaf_hw(&t,0x1000);
    pl.pat=(pl.pat&~(UINT64_C(255)<<56))|(UINT64_C(1)<<56); /* slot7 WC */
    out.hpa=123;calls=a.calls;
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP&&out.hpa==123);
    assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_INVALID&&a.calls==calls);
    assert(leaf_hw(&t,0x1000)==old_leaf&&leaf_hw(&t,0x2000)==0);
    pl.pat=good_pat;
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_HIT&&!out.ignore_pat);
    assert(leaf_hw(&t,0x1000)==old_leaf);
    assert(nested_ept_sparse_destroy(&t)&&!a.live);
    /* WT requires its own explicit admission opt-in, not the UC/WB setters. */
    r=resolved(3,3);r.memory_explicit=true;r.walk_type=6;r.leaf_type=4;r.ignore_pat=true;
    l=(struct lease){.hpa=r.hpa,.pinned_epoch=4,.current_epoch=4,.type=4,.live=true};
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    assert(nested_ept_sparse_set_typed_admission(&t,admit,&l));
    calls=a.calls;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID&&a.calls==calls);
    assert(nested_ept_sparse_destroy(&t));
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    assert(nested_ept_sparse_set_typed_admission_pat(&t,admit,&l));
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID);
    assert(nested_ept_sparse_destroy(&t));
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    assert(nested_ept_sparse_set_typed_admission_wt(&t,admit,&l));
    calls=a.calls;l.type=0;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID&&a.calls==calls);
    l.type=4;l.hpa+=4096;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID&&a.calls==calls);
    l.hpa-=4096;r.ignore_pat=false;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_INVALID&&a.calls==calls);
    r.ignore_pat=true;
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    assert(leaf_hw(&t,0x1000)==(r.hpa|0x63)); /* WT4<<3 + IPAT + RW */
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_HIT&&out.leaf_type==4&&out.ignore_pat);
    l.type=0;out.hpa=123;
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP&&out.hpa==123);
    assert(leaf_hw(&t,0x1000)==(r.hpa|0x63));
    l.type=4;assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_HIT);
    assert(nested_ept_sparse_destroy(&t)&&!a.live);
    puts("nested_ept_sparse_typed: PASS");
}
