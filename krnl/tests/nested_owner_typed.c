#include "../nested_owner_typed.h"
#include "../paging.h"
#include "../vmx_memory.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static _Alignas(4096) uint64_t ram[128][512];
static struct nested_owner_sparse s;
static struct nested_owner_typed typed;
static struct domain_typed_lease lease;
static struct dtl_token token;
static bool environment_ok;
static unsigned fatal_count;
static void fatal(void *ctx){(void)ctx;fatal_count++;}

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
static bool environment(void *ctx){(void)ctx;return environment_ok;}
static void init_mode(bool wt) {
    memset(ram,0,sizeof ram);memset(&s,0,sizeof s);memset(&typed,0,sizeof typed);
    memset(&token,0,sizeof token);environment_ok=true;fatal_count=0;
    memset(&fixture,0,sizeof fixture);flush_ok=true;
    for(unsigned i=0;i<63;i++)fixture.pages[i]=0x100000+i*4096;
    for(unsigned i=0;i<4;i++)fixture.tables[i]=0x140000+i*4096;
    for(unsigned i=0;i<3;i++)ptr(fixture.tables[i])[0]=fixture.tables[i+1]|7;
    for(unsigned n=1;n<64;n++)ptr(fixture.tables[3])[n]=fixture.pages[n-1]|0x70|
        (n>=16&&n<=19?1:3);
    for(unsigned i=0;i<3;i++)ptr(fixture.pages[15+i])[0]=((uint64_t)(17+i)*4096)|7;
    for(unsigned n=1;n<64;n++)ptr(fixture.pages[18])[n]=((uint64_t)n*4096)|0x73;
    for(unsigned i=0;i<32;i++)s.pages[i]=(struct nested_sparse_page){0x150000+i*4096,ram[80+i]};
    unsigned a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    assert(nested_owner_pool_init(&s.pool,b>>24,1,s.pages,s.slots,32,48));
    s.fixture=&fixture;s.initialized=true;s.selected12=0x1001e;
    struct nested_sparse_ops ops={alloc,release,flush};
    assert(nested_ept_sparse_init_mode(&s.tree,&ops,&s.pool,48,false));
    /* Isolate adapter from global CPU-mask protocol: fixture inventory uses
     * actual APIC ID (which may exceed the lease's 64-participant init profile).
     * Real dtl acquire/admit still validate token/owner/root/epoch membership. */
    lease=(struct domain_typed_lease){.phase=DTL_OPEN,.type=6,.epoch=1,.nonwb_type=wt?4:0};
    lease.inventory.host.target=fixture.pages[14];
    lease.inventory.roots=1;
    lease.inventory.root[0]=(struct dtl_root){.id=1,.generation=1,.owner=b>>24};
    typed=(struct nested_owner_typed){.lease=&lease,.token=&token,.epoch=1,
        .target_hpa=fixture.pages[14],.environment_guard=environment,.fatal=fatal,.allow_wt=wt};
    assert(nested_owner_typed_bind(&typed,&s));
    assert(dtl_acquire(&lease,b>>24,0,1,6,true,false,&token,&typed.nonce));
}
static void init(void) { init_mode(false); }
/* The transition protocol is tested independently: model its completed OPEN
 * state while retaining the real token membership, nonce and owner identity. */
static void current_type(unsigned type) {
    lease.type=type;token.type=type;
    ptr(fixture.tables[3])[15]=fixture.pages[14]|0x43|((uint64_t)type<<3);
    ptr(fixture.pages[18])[15]=0xf043|((uint64_t)type<<3);
}
int main(void) {
    init();
    assert(nested_owner_typed_miss(&typed,0xf000,1));
    struct nested_ept_result r;
    assert(nested_ept_sparse_lookup(&s.tree,0xf000,1,&r)==NESTED_SPARSE_HIT);
    assert(r.leaf_type==6&&r.ignore_pat);
    uint64_t nonce=typed.nonce;
    assert(nested_ept_sparse_reset(&s.tree));
    /* Isolate post-transition prepared state; full transition separately tested
     * by domain_typed_lease tests. Keep the actual token and membership list. */
    lease.phase=DTL_STAMP;lease.epoch=2;lease.destination=0;
    token.epoch=2;token.type=0;token.prepared=true;
    ptr(fixture.tables[3])[15]=fixture.pages[14]|0x43;
    ptr(fixture.pages[18])[15]=0xf043;
    assert(nested_owner_typed_refresh(&typed,2,nonce));
    assert(typed.nonce==nonce&&token.live);
    lease.type=0;lease.phase=DTL_OPEN;
    assert(nested_owner_typed_miss(&typed,0xf000,1));
    assert(nested_ept_sparse_lookup(&s.tree,0xf000,1,&r)==NESTED_SPARSE_HIT);
    assert(r.leaf_type==0&&r.ignore_pat&&r.hpa==fixture.pages[14]);

    init_mode(true);current_type(4);
    assert(nested_owner_typed_miss(&typed,0xf000,1));
    assert(nested_ept_sparse_lookup(&s.tree,0xf000,1,&r)==NESTED_SPARSE_HIT);
    assert(r.leaf_type==4&&r.ignore_pat&&r.hpa==fixture.pages[14]);
    assert(!typed.failed&&fatal_count==0&&typed.misses==1);

    /* A valid WT lease alone must not opt the default sparse adapter into WT. */
    init();lease.nonwb_type=4;current_type(4);
    assert(!nested_owner_typed_miss(&typed,0xf000,1));
    assert(typed.failed&&fatal_count==1&&lease.phase==DTL_OPEN&&token.live);
    assert(s.tree.root&&s.tree.leaves==0);

    /* WT admission must match both the 01 mapping and actual EPT12 leaf. */
    init_mode(true);current_type(4);
    ptr(fixture.pages[18])[15]=0xf043; /* EPT12 incorrectly requests UC. */
    assert(!nested_owner_typed_miss(&typed,0xf000,1));
    assert(typed.failed&&fatal_count==1&&s.tree.leaves==0&&token.live);
    init_mode(true);current_type(4);
    ptr(fixture.tables[3])[15]=fixture.pages[14]|0x43; /* 01 incorrectly UC. */
    assert(!nested_owner_typed_miss(&typed,0xf000,1));
    assert(typed.failed&&fatal_count==1&&s.tree.leaves==0&&token.live);

    init();ptr(fixture.tables[3])[10]=fixture.pages[9]|0x33;
    assert(!nested_owner_typed_miss(&typed,0xa000,1)&&typed.failed); /* historical heartbeat rewrite */
    init();assert(nested_owner_typed_miss(&typed,0xa000,1));
    init();environment_ok=false;
    assert(!nested_owner_typed_miss(&typed,0xf000,1));
    assert(typed.failed&&fatal_count==1&&lease.phase==DTL_OPEN&&s.tree.root&&token.live);
    init();typed.nonce++;
    assert(!nested_owner_typed_miss(&typed,0xf000,1)&&token.live);
    init();typed.epoch++;
    assert(!nested_owner_typed_miss(&typed,0xf000,1)&&token.live);
    init();ptr(fixture.tables[3])[15]=fixture.pages[14]|0x43;
    assert(!nested_owner_typed_miss(&typed,0xf000,1)); /* wrong 01 current type */
    init();ptr(fixture.pages[15])[0]=0xf007; /* target asked to supply table bytes */
    assert(!nested_owner_typed_miss(&typed,0xf000,1));
    assert(typed.failed&&fatal_count==1&&lease.phase==DTL_OPEN&&s.tree.root);
    init();assert(nested_owner_typed_miss(&typed,0xf000,1));
    environment_ok=false;
    assert(nested_ept_sparse_lookup(&s.tree,0xf000,1,&r)!=NESTED_SPARSE_HIT);
    init();flush_ok=false;
    assert(!nested_owner_typed_miss(&typed,0xf000,1));
    assert(s.pool.live&&s.tree.root&&token.live);
    puts("PASS typed owner real pool/tree + token, UC/WT exact fill, WT opt-in, refresh and fail-retain");
}
