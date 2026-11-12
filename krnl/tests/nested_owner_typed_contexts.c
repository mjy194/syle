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
static void base_init(bool wt) {
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

static struct nested_owner_typed ctx[2];
static struct dtl_token tokens[2];
static void init_contexts(void){
    base_init(false);
    assert(nested_ept_sparse_destroy(&s.tree));
    memset(ctx,0,sizeof ctx);memset(tokens,0,sizeof tokens);
    s.multiple=true;s.evict=false;
    struct nested_sparse_ops ops={alloc,release,flush};
    assert(nested_ept_contexts_init_budget(&s.contexts,&ops,&s.pool,48,2,false));
    /* Independent EPT12 root at GPA 14000, same registered data backing. */
    for(unsigned l=0;l<3;l++)ptr(fixture.pages[19+l])[0]=((uint64_t)(21+l)*4096)|7;
    for(unsigned n=1;n<64;n++)ptr(fixture.pages[22])[n]=((uint64_t)n*4096)|0x73;
    for(unsigned n=20;n<=23;n++)ptr(fixture.tables[3])[n]=fixture.pages[n-1]|0x71;
    lease.tokens=NULL;lease.pins=0;lease.nonce=0;lease.inventory.roots=2;
    for(unsigned i=0;i<2;i++){
        lease.inventory.root[i]=(struct dtl_root){.id=1+i,.generation=1,.owner=s.pool.owner};
        ctx[i]=(struct nested_owner_typed){.lease=&lease,.token=&tokens[i],.epoch=1,
            .root_index=i,.target_hpa=fixture.pages[14],.environment_guard=environment,.fatal=fatal};
        assert(nested_ept_contexts_select(&s.contexts,i?0x1401e:0x1001e)==NESTED_CONTEXT_OK);
        assert(nested_owner_typed_bind_context(&ctx[i],&s,s.contexts.active));
        assert(dtl_acquire(&lease,s.pool.owner,i,1,6,true,false,&tokens[i],&ctx[i].nonce));
    }
}
static void select_context(unsigned n){
    s.selected12=n?0x1401e:0x1001e;
    assert(nested_ept_contexts_select(&s.contexts,s.selected12)==NESTED_CONTEXT_OK);
}
int main(void){
    init_contexts();
    assert(ctx[0].tree!=ctx[1].tree&&ctx[0].eptp12==0x1001e&&ctx[1].eptp12==0x1401e);
    for(unsigned i=0;i<2;i++){
        select_context(i);assert(nested_owner_typed_miss(&ctx[i],0xf000,1));
        struct nested_ept_result r;
        assert(nested_ept_sparse_lookup(ctx[i].tree,0xf000,1,&r)==NESTED_SPARSE_HIT);
        assert(r.hpa==fixture.pages[14]&&r.leaf_type==6&&r.ignore_pat);
    }
    /* A single reset operation must remove both active AND inactive leaves. */
    assert(nested_ept_contexts_invalidate(&s.contexts,2,0)==NESTED_CONTEXT_OK);
    for(unsigned i=0;i<2;i++)assert(!ctx[i].tree->leaves&&ctx[i].tree->generation==2);
    lease.phase=DTL_STAMP;lease.epoch=2;lease.destination=0;
    ptr(fixture.tables[3])[15]=fixture.pages[14]|0x43;
    ptr(fixture.pages[18])[15]=0xf043;ptr(fixture.pages[22])[15]=0xf043;
    for(unsigned i=0;i<2;i++){
        tokens[i].epoch=2;tokens[i].type=0;tokens[i].prepared=true;
        assert(nested_owner_typed_refresh(&ctx[i],2,ctx[i].nonce));
    }
    lease.type=0;lease.phase=DTL_OPEN;
    for(unsigned i=0;i<2;i++){
        select_context(i);assert(nested_owner_typed_miss(&ctx[i],0xf000,1));
        struct nested_ept_result r;
        assert(nested_ept_sparse_lookup(ctx[i].tree,0xf000,1,&r)==NESTED_SPARSE_HIT&&r.leaf_type==0);
    }
    select_context(1);
    assert(!nested_owner_typed_miss(&ctx[0],0xf000,1)&&ctx[0].failed&&fatal_count==1);
    assert(!ctx[1].failed); /* Cross-context request must not mutate peer state. */
    assert(nested_ept_contexts_destroy(&s.contexts)&&s.pool.live==0);
    puts("PASS two typed contexts distinct tree/key, inactive refresh/reset, UC exact fill and cross-context rejection");
    return 0;
}
