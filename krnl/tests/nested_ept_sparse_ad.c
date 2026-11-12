/* Reuse the standalone allocator/test helpers, without changing AD0 tests. */
#define main sparse_baseline_main
#include "nested_ept_sparse.c"
#undef main
static bool ad_flush(void *ctx,uint64_t eptp) {
    struct allocator *a=ctx;
    assert((eptp&4095)==0x5e);
    a->flushes++;return a->flush_ok;
}
static uint64_t *path_slot(struct nested_ept_sparse *t,uint64_t gpa,unsigned level) {
    uint64_t hpa=t->eptp&~UINT64_C(4095);
    for(unsigned d=0;d<4;d++) {
        uint64_t *p=&((uint64_t*)memory[(hpa-0x100000)/4096])[(gpa>>(39-9*d))&511];
        if(d==level)return p;
        hpa=*p&~UINT64_C(4095);
    }
    assert(0);return NULL;
}
int main(void) {
    struct nested_sparse_ops ad_ops=ops;ad_ops.flush=ad_flush;
    struct allocator a=fresh();struct nested_ept_sparse t;
    struct nested_ept_result r=resolved(7,7),out;
    bool tracked=false;
    assert(nested_ept_sparse_init_mode(&t,&ad_ops,&a,48,true));
    assert((t.eptp&4095)==0x5e);
    assert(nested_ept_sparse_fill_tracked(&t,0x1000,&r,true)==NESTED_SPARSE_OK);
    assert(nested_ept_sparse_query_tracking(&t,0x1123,&tracked)&&tracked);
    uint64_t *leaf=path_slot(&t,0x1000,3);
    assert((*leaf&7)==5); /* real RWX, installed RX */
    for(unsigned d=0;d<3;d++) *path_slot(&t,0x1000,d)|=0x100;
    *leaf|=0x300; /* Simulate allowed hardware mutations. */
    assert(nested_ept_sparse_lookup(&t,0x1000,2,&out)==NESTED_SPARSE_HIT);
    assert(out.status==NESTED_EPT_OK&&out.effective02==7);
    assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_OK); /* A-marked shared path */
    unsigned live=a.live,flushes=a.flushes;
    assert(nested_ept_sparse_promote_write(&t,0x1001));
    assert(a.live==live&&a.flushes==flushes+1&&(*leaf&0x307)==0x307);
    assert(nested_ept_sparse_query_tracking(&t,0x1000,&tracked)&&!tracked);
    assert(!nested_ept_sparse_promote_write(&t,0x1000));
    /* Permission/address and nonleaf D are never ignored as hardware A/D. */
    for(unsigned d=0;d<4;d++) {
        uint64_t *slot=path_slot(&t,0x1000,d),saved=*slot;
        const uint64_t flips[]={2,0x1000,0x400};
        for(unsigned i=0;i<3;i++) {
            *slot=saved^flips[i];tracked=true;
            assert(!nested_ept_sparse_query_tracking(&t,0x1000,&tracked)&&tracked);
            assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP);
            *slot=saved;
        }
        if(d<3) {
            *slot=saved|0x200;
            assert(!nested_ept_sparse_query_tracking(&t,0x1000,&tracked));
            *slot=saved;
        }
    }
    assert(nested_ept_sparse_reset(&t));
    assert(a.live==2&&(t.eptp&4095)==0x5e);
    assert(nested_ept_sparse_fill_tracked(&t,0x1000,&r,true)==NESTED_SPARSE_OK);
    a.flush_ok=false;live=a.live;
    assert(!nested_ept_sparse_promote_write(&t,0x1000)&&t.poisoned&&a.live==live);
    tracked=true;
    assert(!nested_ept_sparse_query_tracking(&t,0x1000,&tracked)&&tracked);
    assert(!nested_ept_sparse_reset(&t));
    assert(!nested_ept_sparse_destroy(&t)&&a.live==live);
    a.flush_ok=true;assert(nested_ept_sparse_destroy(&t)&&!a.live);
    a=fresh();assert(nested_ept_sparse_init(&t,&ops,&a,48));
    assert(nested_ept_sparse_fill_tracked(&t,0x1000,&r,true)==NESTED_SPARSE_INVALID);
    assert(nested_ept_sparse_fill(&t,0x1000,&r)==NESTED_SPARSE_OK);
    *path_slot(&t,0x1000,3)|=0x100;
    assert(nested_ept_sparse_lookup(&t,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP);
    *path_slot(&t,0x1000,3)&=~UINT64_C(0x100);
    *path_slot(&t,0x1000,0)|=0x100;
    assert(nested_ept_sparse_fill(&t,0x2000,&r)==NESTED_SPARSE_INVALID);
    assert(nested_ept_sparse_destroy(&t)&&!a.live);
    a=fresh();assert(nested_ept_sparse_init_mode(&t,&ad_ops,&a,48,true));
    r=resolved(1,1);
    assert(nested_ept_sparse_fill_tracked(&t,0x1000,&r,true)==NESTED_SPARSE_INVALID);
    assert(nested_ept_sparse_destroy(&t)&&!a.live);
    puts("PASS sparse hardware A/D mode and first-write promotion");
}
