#include "../nested_owner_ad.h"
#include "../paging.h"
#include "../vmx_memory.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static _Alignas(4096) uint64_t ram[128][512];
static struct nested_owner_sparse s;
static struct nested_owner_ad ad;
static struct ad_updater_gate gate;
static struct ept_fixture fixture;
static bool flush_ok;
void *paging_phys_to_virt(uint64_t p) {
    if(p<0x100000||p>=0x180000||(p&4095))return NULL;
    return ram[(p-0x100000)/4096];
}
int vmx_memory_region_wb(uint64_t p){return paging_phys_to_virt(p)!=NULL;}
static bool alloc(void *ctx,struct nested_sparse_page *p) {
    return nested_owner_pool_alloc(ctx,p);
}
static void release(void *ctx,struct nested_sparse_page p) {nested_owner_pool_free(ctx,p);}
static bool flush(void *ctx,uint64_t eptp){(void)ctx;(void)eptp;return flush_ok;}
static uint64_t *ptr(uint64_t p){return paging_phys_to_virt(p);}
static void init(void) {
    memset(ram,0,sizeof ram);memset(&s,0,sizeof s);memset(&ad,0,sizeof ad);
    memset(&fixture,0,sizeof fixture);flush_ok=true;
    for(unsigned i=0;i<63;i++)fixture.pages[i]=0x100000+i*4096;
    for(unsigned i=0;i<4;i++)fixture.tables[i]=0x140000+i*4096;
    for(unsigned i=0;i<3;i++)ptr(fixture.tables[i])[0]=fixture.tables[i+1]|7;
    for(unsigned n=1;n<64;n++)ptr(fixture.tables[3])[n]=fixture.pages[n-1]|0x30|
        (n>=16&&n<=19?1:3);
    for(unsigned i=0;i<3;i++)ptr(fixture.pages[15+i])[0]=((uint64_t)(17+i)*4096)|7;
    for(unsigned n=1;n<64;n++)ptr(fixture.pages[18])[n]=((uint64_t)n*4096)|0x33;
    for(unsigned i=0;i<32;i++)s.pages[i]=(struct nested_sparse_page){0x150000+i*4096,ram[80+i]};
    unsigned a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    assert(nested_owner_pool_init(&s.pool,b>>24,1,s.pages,s.slots,32,48));
    s.fixture=&fixture;s.initialized=true;s.selected12=0x1005e;
    struct nested_sparse_ops ops={alloc,release,flush};
    assert(nested_ept_sparse_init_mode(&s.tree,&ops,&s.pool,48,true));
    ad.gate=&gate;ad.stamp=(struct ad_entry_stamp){1,2,3,4,5};ad.authority=true;
    assert(ad_updater_gate_init(&gate,ad.stamp));
    assert(nested_owner_ad_bind(&ad,&s));
}
int main(void) {
    init();
    assert(nested_owner_ad_fault(&ad,0xf000,1));
    bool tracked=false;
    assert(nested_ept_sparse_query_tracking(&s.tree,0xf000,&tracked)&&tracked);
    assert((ptr(fixture.pages[18])[15]&0x300)==0x100);
    assert(nested_owner_ad_fault(&ad,0xf008,2));
    assert((ptr(fixture.pages[18])[15]&0x300)==0x300);
    assert(ad.readwalks==1&&ad.promotions==1&&ad.target_promotions==1);
    assert(!gate.references&&!ad.token.live);
    assert(nested_ept_sparse_query_tracking(&s.tree,0xf000,&tracked)&&!tracked);
    assert(nested_ept_sparse_destroy(&s.tree));
    assert(ad_updater_gate_close(&gate,ad.stamp));
    assert(nested_owner_ad_release_records(&ad,true));
    assert(!s.pool.live);

    init();ptr(fixture.pages[18])[15]|=0x200;
    assert(nested_owner_ad_fault(&ad,0xf000,1));
    assert(nested_ept_sparse_query_tracking(&s.tree,0xf000,&tracked)&&tracked);
    /* A read trace may remember D; clearing D does not excuse the owner write. */
    __atomic_fetch_and(&ptr(fixture.pages[18])[15],~UINT64_C(0x200),__ATOMIC_ACQ_REL);
    assert(nested_owner_ad_fault(&ad,0xf000,2));
    assert(ptr(fixture.pages[18])[15]&0x200);

    init();assert(nested_owner_ad_fault(&ad,0xf000,1));
    uint64_t *leaf=&ptr(fixture.pages[18])[15];
    *leaf=0xe033;
    assert(!nested_owner_ad_fault(&ad,0xf000,2));
    assert(*leaf==0xe033); /* cached identity rejected BEFORE CAS into new mapping */
    assert(ad.failed&&gate.fault&&!gate.references&&ad.records);
    assert(!nested_owner_ad_release_records(&ad,true));

    init();assert(nested_owner_ad_fault(&ad,0xf000,1));
    flush_ok=false;
    assert(!nested_owner_ad_fault(&ad,0xf000,2));
    assert(ad.failed&&s.tree.poisoned&&ad.records&&s.pool.live);
    assert(!gate.references);
    init();assert(nested_owner_ad_fault(&ad,0xf000,1));
    assert(nested_owner_ad_fault(&ad,0xf000,2));
    struct ad_entry_stamp old=ad.stamp,next=old,opened;next.epoch++;
    assert(!nested_owner_ad_local_reset(&ad,true)); /* open gate */
    assert(!nested_owner_ad_advance_stamp(&ad,old,next)); /* no reset */
    uint64_t eptp=s.tree.eptp,gen=s.tree.generation;
    assert(ad_updater_gate_close(&gate,old));
    assert(!nested_owner_ad_local_reset(&ad,false));
    assert(nested_owner_ad_local_reset(&ad,true));
    assert(ad.reset_pending&&!ad.records&&s.tree.eptp==eptp&&s.tree.generation==gen+1);
    assert(ad.target_promotions==1&&ad.readwalks==1);
    assert(!nested_owner_ad_local_reset(&ad,true));
    for(unsigned field=0;field<5;field++) {
        struct ad_entry_stamp bad=next;
        if(field==0)bad.domain_id++;
        if(field==1)bad.incarnation++;
        if(field==2)bad.context_id++;
        if(field==3)bad.epoch++;
        if(field==4)bad.lifetime++;
        assert(!nested_owner_ad_advance_stamp(&ad,old,bad));
        assert(ad.stamp.epoch==old.epoch&&ad.reset_pending);
    }
    assert(nested_owner_ad_advance_stamp(&ad,old,next));
    assert(!nested_owner_ad_advance_stamp(&ad,old,next));
    struct ad_updater_token stale={0};uint64_t nonce;
    assert(!ad_updater_gate_acquire(&gate,next,&stale,&nonce)); /* still closed */
    __atomic_fetch_and(&ptr(fixture.pages[18])[15],~UINT64_C(0x300),__ATOMIC_ACQ_REL);
    assert(ad_updater_gate_reopen(&gate,old,true,&opened));
    assert(opened.epoch==next.epoch);
    assert(!ad_updater_gate_acquire(&gate,old,&stale,&nonce));
    assert(nested_owner_ad_fault(&ad,0xf000,1));
    assert(nested_owner_ad_fault(&ad,0xf000,2));
    assert(ad.target_promotions==2&&ad.readwalks==2);
    assert(nested_ept_sparse_destroy(&s.tree));
    assert(nested_owner_ad_release_records(&ad,true));
    assert(!s.pool.live);

    init();assert(nested_owner_ad_fault(&ad,0xf000,1));
    assert(ad_updater_gate_close(&gate,ad.stamp));
    struct nested_owner_ad_record *saved_records=ad.records;
    size_t saved_live=s.pool.live;
    flush_ok=false;
    assert(!nested_owner_ad_local_reset(&ad,true));
    assert(ad.failed&&s.tree.poisoned&&ad.records==saved_records&&s.pool.live==saved_live);
    assert(!nested_owner_ad_advance_stamp(&ad,ad.stamp,ad.stamp));

    init();assert(nested_owner_ad_fault(&ad,0xf000,1));
    struct ad_updater_token busy={0};uint64_t ticket;
    assert(ad_updater_gate_acquire(&gate,ad.stamp,&busy,&ticket));
    assert(ad_updater_gate_close(&gate,ad.stamp));
    assert(!nested_owner_ad_local_reset(&ad,true)); /* live updater */
    assert(ad_updater_gate_release(&gate,&busy,ad.stamp,ticket));
    assert(nested_owner_ad_local_reset(&ad,true));
    puts("PASS owner atomic AD tracked write, identity-before-CAS, retained failures, successful cleanup");
}
