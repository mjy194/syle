#include "../nested_ept_contexts.h"
#include "../cache_lease_refs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define POOL 256
static _Alignas(4096) unsigned char memory[POOL][4096];
struct alloc_state {
    bool used[POOL];
    unsigned live,calls,fail_alloc,flushes,fail_flush;
    uint64_t fail_eptp;
};
static bool allocate(void *context,struct nested_sparse_page *p) {
    struct alloc_state *a=context;
    if(a->calls++==a->fail_alloc) return false;
    for(unsigned i=0;i<POOL;i++) if(!a->used[i]) {
        a->used[i]=true;a->live++;
        *p=(struct nested_sparse_page){0x100000+(uint64_t)i*4096,memory[i]};
        return true;
    }
    return false;
}
static void release(void *context,struct nested_sparse_page p) {
    struct alloc_state *a=context;
    assert(p.hpa>=0x100000&&!(p.hpa&4095));
    unsigned i=(unsigned)((p.hpa-0x100000)/4096);
    assert(i<POOL&&a->used[i]);a->used[i]=false;a->live--;
}
static bool flush(void *context,uint64_t eptp) {
    struct alloc_state *a=context;assert((eptp&4095)==0x1e||(eptp&4095)==0x5e);
    return a->flushes++!=a->fail_flush && eptp!=a->fail_eptp;
}
static const struct nested_sparse_ops ops={allocate,release,flush};
static struct alloc_state fresh(void) {
    memset(memory,0,sizeof memory);
    return (struct alloc_state){.fail_alloc=~0u,.fail_flush=~0u};
}
struct hooks {struct cache_lease_refs refs;unsigned init,reset,destroy;};
_Static_assert(sizeof(struct cache_lease_token)<=sizeof(((struct nested_ept_context_item *)0)->attachment),"token fits");
static struct cache_lease_token *token(struct nested_ept_context_item *p)
{return &p->attachment.uc_token;}
static void hook(void *ctx,struct nested_ept_context_item *p,enum nested_context_event event)
{
    struct hooks *h=ctx;
    if(event==NESTED_CONTEXT_INIT){
        assert(p->tree.root&&!p->attachment_initialized);
        assert(cache_lease_refs_acquire(&h->refs,token(p),p,h->refs.generation));h->init++;
    }else if(event==NESTED_CONTEXT_RESET){
        assert(p->tree.root&&p->attachment_initialized);
        assert(cache_lease_refs_admit(&h->refs,token(p),p,h->refs.generation));h->reset++;
    }else{
        assert(event==NESTED_CONTEXT_DESTROY&&!p->tree.root&&p->attachment_initialized);
        assert(cache_lease_refs_release(&h->refs,token(p),p,h->refs.generation));h->destroy++;
    }
}
static void setup(struct nested_ept_contexts *r,struct alloc_state *a,struct hooks *h)
{
    *a=fresh();*h=(struct hooks){0};assert(cache_lease_refs_init(&h->refs,7));
    assert(nested_ept_contexts_init_config(r,&ops,a,48,2,true,false,hook,h));
}
static void select_ok(struct nested_ept_contexts *r,struct hooks *h,uint64_t key)
{
    assert(nested_ept_contexts_select(r,key)==NESTED_CONTEXT_OK);
    assert(h->refs.refs==r->count);
    for(struct nested_ept_context_item *p=r->items;p;p=p->next)
        assert(cache_lease_refs_admit(&h->refs,token(p),p,7));
}
static struct nested_ept_context_item *find(struct nested_ept_contexts *r,uint64_t key)
{
    for(struct nested_ept_context_item *p=r->items;p;p=p->next)if(p->key==key)return p;
    return NULL;
}
static void clean(struct nested_ept_contexts *r,struct alloc_state *a,struct hooks *h)
{
    a->fail_flush=~0u;a->fail_eptp=0;a->fail_alloc=~0u;
    nested_ept_contexts_release_all_after_clear(r);
    assert(nested_ept_contexts_destroy(r));assert(!a->live&&!h->refs.refs&&!h->refs.head);
    assert(h->init==h->destroy);
}
int main(void)
{
    const uint64_t A=0x101e,B=0x201e,C=0x301e;
    struct nested_ept_contexts r;struct alloc_state a;struct hooks h;
    setup(&r,&a,&h);
    select_ok(&r,&h,A);select_ok(&r,&h,B);select_ok(&r,&h,A);select_ok(&r,&h,C);
    assert(find(&r,A)&&find(&r,C)&&!find(&r,B));assert(h.init==3&&h.destroy==1&&h.refs.refs==2);
    assert(nested_ept_contexts_invalidate(&r,1,A)==NESTED_CONTEXT_OK);
    assert(h.reset==1&&h.refs.refs==2);
    assert(nested_ept_contexts_invalidate(&r,2,UINT64_MAX)==NESTED_CONTEXT_OK);
    assert(h.reset==3&&h.refs.refs==2);clean(&r,&a,&h);
    /* All candidate allocation failure points preserve active/live pins. */
    for(unsigned fail=0;fail<3;fail++){
        setup(&r,&a,&h);select_ok(&r,&h,A);select_ok(&r,&h,B);
        struct nested_ept_context_item *active=r.active;unsigned live=a.live,inits=h.init;
        a.fail_alloc=a.calls+fail;
        assert(nested_ept_contexts_select(&r,C)==NESTED_CONTEXT_OOM);
        assert(r.active==active&&r.count==2&&a.live==live&&h.refs.refs==2&&h.init==inits);
        clean(&r,&a,&h);
    }
    /* Initial candidate flush failure creates retained tree but NO INIT pin. */
    setup(&r,&a,&h);select_ok(&r,&h,A);
    a.fail_flush=a.flushes;
    assert(nested_ept_contexts_select(&r,B)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(r.count==2&&h.refs.refs==1&&h.init==1);clean(&r,&a,&h);
    /* Candidate INIT acquired before victim flush failure: both retained. */
    setup(&r,&a,&h);select_ok(&r,&h,A);select_ok(&r,&h,B);
    struct nested_ept_context_item *active=r.active;
    a.fail_eptp=find(&r,A)->tree.eptp;
    assert(nested_ept_contexts_select(&r,C)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(r.poisoned&&r.active==active&&r.count==3&&h.refs.refs==3&&h.init==3&&!h.destroy);
    cache_lease_refs_poison(&h.refs); /* no admission; safe destroy still releases */
    assert(!cache_lease_refs_admit(&h.refs,token(active),active,7));
    clean(&r,&a,&h);
    /* Type2 partial reset: first epoch advances, next fails; BOTH retain refs. */
    setup(&r,&a,&h);select_ok(&r,&h,A);select_ok(&r,&h,B);
    a.fail_flush=a.flushes+1;
    assert(nested_ept_contexts_invalidate(&r,2,0)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(h.reset==1&&h.refs.refs==2&&r.count==2);
    assert(find(&r,B)->tree.generation==2&&find(&r,A)->tree.generation==1);
    clean(&r,&a,&h);
    /* Failed destroy releases only successful trees, retaining failed pin. */
    setup(&r,&a,&h);select_ok(&r,&h,A);select_ok(&r,&h,B);
    a.fail_eptp=find(&r,A)->tree.eptp;
    assert(!nested_ept_contexts_destroy(&r));
    assert(r.count==1&&h.refs.refs==1&&h.destroy==1&&find(&r,A));
    assert(cache_lease_refs_admit(&h.refs,token(find(&r,A)),find(&r,A),7));
    clean(&r,&a,&h);
    /* A VMCS reference forbids eviction and whole-registry destroy. */
    setup(&r,&a,&h);select_ok(&r,&h,A);
    struct nested_ept_context_item *pinned=nested_ept_contexts_pin_active(&r);
    assert(pinned);select_ok(&r,&h,B);
    unsigned live=a.live,flushes=a.flushes,calls=a.calls;
    assert(nested_ept_contexts_select(&r,C)==NESTED_CONTEXT_LIMIT);
    assert(!nested_ept_contexts_destroy(&r));
    assert(a.live==live&&a.flushes==flushes&&a.calls==calls&&h.refs.refs==2&&!h.destroy);
    /* Reset while stopped does not release either cache lease or VMCS pin. */
    assert(nested_ept_contexts_invalidate(&r,1,A)==NESTED_CONTEXT_OK);
    assert(pinned->pins==1&&h.refs.refs==2&&h.reset==1);
    assert(nested_ept_contexts_unpin(&r,pinned));select_ok(&r,&h,C);
    assert(!find(&r,A)&&h.refs.refs==2&&h.destroy==1);
    clean(&r,&a,&h);
    puts("cache_lease_contexts lifecycle integration: PASS");
}
