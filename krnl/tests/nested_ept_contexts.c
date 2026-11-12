#include "../nested_ept_contexts.h"
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
static void setup(struct nested_ept_contexts *r,struct alloc_state *a,unsigned budget,bool evict) {
    *a=fresh();assert(nested_ept_contexts_init_budget(r,&ops,a,48,budget,evict));
}
static struct nested_ept_context_item *find(struct nested_ept_contexts *r,uint64_t key) {
    for(struct nested_ept_context_item *p=r->items;p;p=p->next) if(p->key==key) return p;
    return NULL;
}
static void fill(struct nested_ept_contexts *r,uint64_t hpa) {
    struct nested_ept_result result={.status=NESTED_EPT_OK,.l1_gpa=0x8000,.hpa=hpa,
        .effective12=7,.effective02=7,.leaf_resolved=true};
    assert(nested_ept_sparse_fill(nested_ept_contexts_active(r),0x1000,&result)==NESTED_SPARSE_OK);
}
struct hook_state {unsigned inits,resets,destroys,live,records;};
static void lifecycle(void *context,struct nested_ept_context_item *item,
    enum nested_context_event event) {
    struct hook_state *h=context;
    assert(!((uintptr_t)&item->attachment&15));
    if(event==NESTED_CONTEXT_INIT) {
        assert(!item->attachment_initialized&&item->tree.root&&item->tree.generation==1);
        for(unsigned i=0;i<16;i++) assert(item->attachment.q[i]==0);
        item->attachment.q[0]=0xabcdef;
        item->attachment.q[1]=item->tree.generation;
        h->inits++;h->live++;
    } else {
        assert(item->attachment_initialized&&item->attachment.q[0]==0xabcdef);
        assert(h->records>=item->attachment.q[2]);
        h->records-=(unsigned)item->attachment.q[2];item->attachment.q[2]=0;
        if(event==NESTED_CONTEXT_RESET) {
            assert(item->tree.root&&item->tree.generation==item->attachment.q[1]+1);
            item->attachment.q[1]=item->tree.generation;h->resets++;
        } else {
            assert(event==NESTED_CONTEXT_DESTROY&&!item->tree.root&&item->tree.eptp==0);
            assert(h->live);h->live--;h->destroys++;item->attachment.q[0]=0;
        }
    }
}
static void add_records(struct nested_ept_context_item *item,struct hook_state *h,unsigned n) {
    item->attachment.q[2]+=n;h->records+=n;
}
int main(void) {
    struct nested_ept_contexts r;struct alloc_state a;
    setup(&r,&a,8,false);
    struct nested_ept_result result={.hpa=123};
    assert(nested_ept_contexts_active(&r)==NULL);
    assert(nested_ept_contexts_lookup(&r,0x1000,1,&result)==NESTED_SPARSE_MISS);
    const uint64_t bad[]={0x10000,0x1005e,0x1001f,0x100000001001eULL,UINT64_MAX};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) {
        assert(nested_ept_contexts_select(&r,bad[i])==NESTED_CONTEXT_INVALID&&a.live==0);
    }
    /* More than four dynamically allocated, independently cached contexts. */
    for(unsigned i=1;i<=6;i++) {
        assert(nested_ept_contexts_select(&r,((uint64_t)i<<16)|0x1e)==NESTED_CONTEXT_OK);
        fill(&r,0x800000+i*4096);
    }
    assert(r.count==6&&a.live==60);
    unsigned calls=a.calls,flushes=a.flushes;
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    assert(a.calls==calls&&a.flushes==flushes);
    assert(nested_ept_contexts_lookup(&r,0x1123,1,&result)==NESTED_SPARSE_HIT&&result.hpa==0x801123);
    uint64_t eptp=r.active->tree.eptp;
    assert(nested_ept_contexts_invalidate(&r,1,0x7001e)==NESTED_CONTEXT_OK&&a.flushes==flushes);
    assert(nested_ept_contexts_invalidate(&r,1,0x1001e)==NESTED_CONTEXT_OK);
    assert(r.active->tree.generation==2&&r.active->tree.eptp==eptp);
    assert(find(&r,0x2001e)->tree.generation==1);
    assert(nested_ept_contexts_lookup(&r,0x1000,1,&result)==NESTED_SPARSE_MISS);
    assert(nested_ept_contexts_invalidate(&r,2,UINT64_MAX)==NESTED_CONTEXT_OK);
    assert(r.active->tree.generation==3&&find(&r,0x2001e)->tree.generation==2);
    assert(a.live==18);
    assert(nested_ept_contexts_invalidate(&r,1,0x1005e)==NESTED_CONTEXT_INVALID);
    assert(nested_ept_contexts_invalidate(&r,0,0)==NESTED_CONTEXT_INVALID);
    assert(nested_ept_contexts_destroy(&r)&&a.live==0);

    /* Default N12 resource behavior retained, dynamically implemented. */
    a=fresh();assert(nested_ept_contexts_init(&r,&ops,&a,48));
    for(unsigned i=1;i<=4;i++)
        assert(nested_ept_contexts_select(&r,((uint64_t)i<<16)|0x1e)==NESTED_CONTEXT_OK);
    assert(nested_ept_contexts_select(&r,0x5001e)==NESTED_CONTEXT_LIMIT);
    assert(nested_ept_contexts_destroy(&r)&&a.live==0);

    setup(&r,&a,3,true);
    for(unsigned i=1;i<=3;i++) {
        assert(nested_ept_contexts_select(&r,((uint64_t)i<<16)|0x1e)==NESTED_CONTEXT_OK);
        fill(&r,0x800000+i*4096);
    }
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK); /* touches A */
    assert(nested_ept_contexts_select(&r,0x4001e)==NESTED_CONTEXT_OK); /* B is LRU */
    assert(!find(&r,0x2001e)&&find(&r,0x1001e)&&find(&r,0x3001e)&&r.count==3);
    assert(nested_ept_contexts_select(&r,0x2001e)==NESTED_CONTEXT_OK); /* reload empty */
    assert(nested_ept_contexts_lookup(&r,0x1000,1,&result)==NESTED_SPARSE_MISS);
    assert(r.active->tree.generation==1);
    assert(nested_ept_contexts_destroy(&r)&&a.live==0);

    /* Pin-aware hardware reference ownership, not software active alone. */
    setup(&r,&a,2,true);
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    struct nested_ept_context_item *old=nested_ept_contexts_pin_active(&r);
    assert(old&&old->pins==1);
    assert(nested_ept_contexts_select(&r,0x2001e)==NESTED_CONTEXT_OK);
    struct nested_ept_context_item *new_item=nested_ept_contexts_pin_active(&r);
    unsigned live=a.live;calls=a.calls;
    assert(nested_ept_contexts_select(&r,0x3001e)==NESTED_CONTEXT_LIMIT);
    assert(a.calls==calls&&a.live==live&&r.active==new_item);
    assert(!nested_ept_contexts_destroy(&r)&&a.live==live&&r.active==new_item);
    assert(!nested_ept_contexts_unpin(&r,(struct nested_ept_context_item *)(uintptr_t)1));
    assert(nested_ept_contexts_unpin(&r,old)); /* successful VMWRITE new reference */
    assert(!nested_ept_contexts_unpin(&r,old));
    assert(nested_ept_contexts_select(&r,0x3001e)==NESTED_CONTEXT_OK);
    assert(!find(&r,0x1001e)&&find(&r,0x2001e)->pins==1);
    r.active->pins=UINT32_MAX;
    assert(nested_ept_contexts_pin_active(&r)==NULL&&r.active->pins==UINT32_MAX);
    nested_ept_contexts_release_all_after_clear(&r);
    assert(nested_ept_contexts_destroy(&r)&&a.live==0);

    /* Candidate node + root table + root metadata: rollback each OOM point,
     * even at full budget, never evict before candidate exists. */
    for(unsigned fail=0;fail<3;fail++) {
        setup(&r,&a,2,true);
        assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);fill(&r,0x800000);
        assert(nested_ept_contexts_select(&r,0x2001e)==NESTED_CONTEXT_OK);fill(&r,0x900000);
        live=a.live;old=r.active;uint64_t tick=r.tick;
        a.fail_alloc=a.calls+fail;
        assert(nested_ept_contexts_select(&r,0x3001e)==NESTED_CONTEXT_OOM);
        assert(a.live==live&&r.count==2&&r.active==old&&r.tick==tick&&!r.poisoned);
        assert(find(&r,0x1001e));
        assert(nested_ept_contexts_lookup(&r,0x1000,1,&result)==NESTED_SPARSE_HIT&&result.hpa==0x900000);
        assert(nested_ept_contexts_destroy(&r)&&a.live==0);
    }
    setup(&r,&a,2,true);
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    assert(nested_ept_contexts_select(&r,0x2001e)==NESTED_CONTEXT_OK);
    old=r.active;uint64_t tick=r.tick;a.fail_flush=a.flushes; /* candidate init */
    assert(nested_ept_contexts_select(&r,0x3001e)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(r.poisoned&&r.active==old&&r.tick==tick&&r.count==3&&a.live==9);
    assert(find(&r,0x3001e)->tree.poisoned);
    assert(nested_ept_contexts_active(&r)==NULL);
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_POISONED);
    assert(nested_ept_contexts_destroy(&r)&&a.live==0);

    setup(&r,&a,2,true);
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    a.fail_eptp=r.active->tree.eptp; /* fail victim flush, NOT candidate flush */
    assert(nested_ept_contexts_select(&r,0x2001e)==NESTED_CONTEXT_OK);
    old=r.active;tick=r.tick;
    assert(nested_ept_contexts_select(&r,0x3001e)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(r.poisoned&&r.active==old&&r.tick==tick&&r.count==3&&a.live==9);
    assert(find(&r,0x1001e)->tree.poisoned&&!find(&r,0x3001e)->tree.poisoned);
    assert(!nested_ept_contexts_destroy(&r)&&r.count==1&&a.live==3);
    a.fail_eptp=0;assert(nested_ept_contexts_destroy(&r)&&a.live==0);

    /* LRU clock overflow is not wrapped and never changes selection/cache. */
    setup(&r,&a,2,true);
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    old=r.active;uint64_t touch=old->last_used;r.tick=UINT64_MAX;calls=a.calls;
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OVERFLOW);
    assert(nested_ept_contexts_select(&r,0x2001e)==NESTED_CONTEXT_OVERFLOW);
    assert(r.active==old&&old->last_used==touch&&a.calls==calls);
    assert(nested_ept_contexts_destroy(&r)&&a.live==0);

    setup(&r,&a,2,false);
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);fill(&r,0x800000);
    assert(nested_ept_contexts_select(&r,0x2001e)==NESTED_CONTEXT_OK);fill(&r,0x900000);
    a.fail_eptp=find(&r,0x1001e)->tree.eptp; /* list order: B resets then A fails */
    assert(nested_ept_contexts_invalidate(&r,2,0)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(r.poisoned&&find(&r,0x2001e)->tree.generation==2&&find(&r,0x1001e)->tree.generation==1);
    assert(!nested_ept_contexts_destroy(&r)&&r.count==1&&a.live==10);
    a.fail_eptp=0;assert(nested_ept_contexts_destroy(&r)&&a.live==0);

    /* A poisoned fill propagates; pins can still be released after CLEAR. */
    setup(&r,&a,2,true);
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    old=nested_ept_contexts_pin_active(&r);
    struct nested_ept_result mapping={.status=NESTED_EPT_OK,.l1_gpa=0x8000,.hpa=0x800000,
        .effective12=7,.effective02=7,.leaf_resolved=true};
    a.fail_flush=a.flushes;
    assert(nested_ept_sparse_fill(&old->tree,0x1000,&mapping)==NESTED_SPARSE_FLUSH_FAILED);
    assert(nested_ept_contexts_select(&r,0x2001e)==NESTED_CONTEXT_POISONED);
    assert(nested_ept_contexts_lookup(&r,0x1000,1,&result)==NESTED_SPARSE_LOOKUP_POISONED);
    assert(!nested_ept_contexts_destroy(&r));
    nested_ept_contexts_release_all_after_clear(&r);
    assert(nested_ept_contexts_destroy(&r)&&a.live==0);

    a=fresh();assert(nested_ept_contexts_init_budget(&r,&ops,&a,32,2,false));
    assert(nested_ept_contexts_select(&r,0xfffff01e)==NESTED_CONTEXT_OK);
    assert(nested_ept_contexts_select(&r,0x10000001eULL)==NESTED_CONTEXT_INVALID);
    assert(nested_ept_contexts_destroy(&r)&&a.live==0);
    assert(!nested_ept_contexts_init_budget(&r,&ops,&a,48,0,true));
    assert(!nested_ept_contexts_init(&r,&ops,&a,49));
    /* Configured AD roots and inline lifecycle state are isolated per item. */
    struct hook_state h={0};
    a=fresh();assert(nested_ept_contexts_init_config(&r,&ops,&a,48,2,true,true,lifecycle,&h));
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_INVALID);
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_OK);
    assert((r.active->tree.eptp&4095)==0x5e&&r.active->attachment_initialized);
    add_records(r.active,&h,3);
    assert(nested_ept_contexts_select(&r,0x2005e)==NESTED_CONTEXT_OK);
    add_records(r.active,&h,4);
    assert(h.inits==2&&h.live==2&&h.records==7);
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_OK&&h.inits==2);
    assert(nested_ept_contexts_invalidate(&r,1,0x1001e)==NESTED_CONTEXT_INVALID);
    assert(nested_ept_contexts_invalidate(&r,1,0x3005e)==NESTED_CONTEXT_OK&&h.resets==0);
    assert(nested_ept_contexts_invalidate(&r,1,0x1005e)==NESTED_CONTEXT_OK);
    assert(h.resets==1&&h.records==4&&find(&r,0x1005e)->attachment.q[1]==2);
    assert(nested_ept_contexts_invalidate(&r,2,UINT64_MAX)==NESTED_CONTEXT_OK);
    assert(h.resets==3&&h.records==0);
    assert(nested_ept_contexts_destroy(&r)&&a.live==0&&h.live==0&&h.destroys==2);

    /* Failed RESET and failed destruction preserve attachment exactly. */
    h=(struct hook_state){0};a=fresh();
    assert(nested_ept_contexts_init_config(&r,&ops,&a,48,2,true,true,lifecycle,&h));
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_OK);
    add_records(r.active,&h,5);old=r.active;a.fail_eptp=old->tree.eptp;
    assert(nested_ept_contexts_invalidate(&r,1,0x1005e)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(h.resets==0&&h.records==5&&old->attachment.q[1]==1&&old->attachment.q[2]==5);
    assert(!nested_ept_contexts_destroy(&r)&&h.destroys==0&&h.records==5);
    a.fail_eptp=0;
    assert(nested_ept_contexts_destroy(&r)&&a.live==0&&h.destroys==1&&h.records==0);

    /* Initial flush failure never INITs candidate, so cleanup never calls
     * DESTROY on its uninitialized attachment. */
    h=(struct hook_state){0};a=fresh();
    assert(nested_ept_contexts_init_config(&r,&ops,&a,48,2,true,true,lifecycle,&h));
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_OK);
    a.fail_flush=a.flushes;
    assert(nested_ept_contexts_select(&r,0x2005e)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(h.inits==1&&!find(&r,0x2005e)->attachment_initialized);
    assert(nested_ept_contexts_destroy(&r)&&h.destroys==1&&h.live==0&&a.live==0);

    /* Victim-flush failure happens after candidate INIT: retain both states. */
    h=(struct hook_state){0};a=fresh();
    assert(nested_ept_contexts_init_config(&r,&ops,&a,48,2,true,true,lifecycle,&h));
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_OK);
    a.fail_eptp=r.active->tree.eptp;add_records(r.active,&h,2);
    assert(nested_ept_contexts_select(&r,0x2005e)==NESTED_CONTEXT_OK);
    assert(nested_ept_contexts_select(&r,0x3005e)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(h.inits==3&&h.live==3&&h.destroys==0&&h.records==2);
    assert(find(&r,0x3005e)->attachment_initialized);
    assert(!nested_ept_contexts_destroy(&r)&&h.live==1&&h.destroys==2&&h.records==2);
    a.fail_eptp=0;
    assert(nested_ept_contexts_destroy(&r)&&a.live==0&&h.live==0&&h.destroys==3&&h.records==0);

    /* Pins prevent eviction and callbacks until the last hardware owner clears. */
    h=(struct hook_state){0};a=fresh();
    assert(nested_ept_contexts_init_config(&r,&ops,&a,48,2,true,true,lifecycle,&h));
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_OK);
    old=nested_ept_contexts_pin_active(&r);
    assert(nested_ept_contexts_select(&r,0x2005e)==NESTED_CONTEXT_OK);
    new_item=nested_ept_contexts_pin_active(&r);
    assert(nested_ept_contexts_select(&r,0x3005e)==NESTED_CONTEXT_LIMIT&&h.inits==2);
    assert(nested_ept_contexts_unpin(&r,old));
    assert(nested_ept_contexts_select(&r,0x3005e)==NESTED_CONTEXT_OK);
    assert(h.inits==3&&h.destroys==1&&h.live==2);
    assert(!nested_ept_contexts_destroy(&r)&&h.destroys==1);
    nested_ept_contexts_release_all_after_clear(&r);
    assert(nested_ept_contexts_destroy(&r)&&h.destroys==3&&h.live==0&&a.live==0);

    h=(struct hook_state){0};a=fresh();
    assert(nested_ept_contexts_init_config(&r,&ops,&a,48,2,false,false,lifecycle,&h));
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_INVALID);
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    assert((r.active->tree.eptp&4095)==0x1e);
    assert(nested_ept_contexts_destroy(&r)&&h.destroys==1&&a.live==0);
    /* Explicit mixed keys retain independent AD0/AD1 caches for one root. */
    h=(struct hook_state){0};a=fresh();
    assert(nested_ept_contexts_init_mixed(&r,&ops,&a,48,4,false,lifecycle,&h));
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    struct nested_ept_context_item *ad0=r.active;
    assert(!ad0->tree.hardware_ad&&(ad0->tree.eptp&4095)==0x1e);
    fill(&r,0xa00000);add_records(ad0,&h,2);
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_OK);
    struct nested_ept_context_item *ad1=r.active;
    assert(ad1!=ad0&&ad1->tree.hardware_ad&&(ad1->tree.eptp&4095)==0x5e);
    assert(ad1->tree.eptp!=ad0->tree.eptp);
    fill(&r,0xb00000);add_records(ad1,&h,3);
    assert(nested_ept_contexts_select(&r,0x2005e)==NESTED_CONTEXT_OK);
    add_records(r.active,&h,4);
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    assert(nested_ept_contexts_lookup(&r,0x1000,1,&result)==NESTED_SPARSE_HIT&&result.hpa==0xa00000);
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_OK);
    assert(nested_ept_contexts_lookup(&r,0x1000,1,&result)==NESTED_SPARSE_HIT&&result.hpa==0xb00000);
    assert(h.inits==3&&h.records==9);
    assert(nested_ept_contexts_invalidate(&r,1,0x1001e)==NESTED_CONTEXT_OK);
    assert(ad0->tree.generation==2&&ad1->tree.generation==2&&h.resets==2&&h.records==4);
    assert(find(&r,0x2005e)->tree.generation==1);
    assert(nested_ept_contexts_invalidate(&r,1,0x1005e)==NESTED_CONTEXT_OK);
    assert(ad0->tree.generation==3&&ad1->tree.generation==3&&h.resets==4);
    const uint64_t mixed_bad[]={0x1009e,0x1007e,0x1001f,0x1005f,0x100000001005eULL};
    for(unsigned i=0;i<sizeof mixed_bad/sizeof *mixed_bad;i++) {
        assert(nested_ept_contexts_select(&r,mixed_bad[i])==NESTED_CONTEXT_INVALID);
        assert(nested_ept_contexts_invalidate(&r,1,mixed_bad[i])==NESTED_CONTEXT_INVALID);
    }
    assert(nested_ept_contexts_destroy(&r)&&a.live==0&&h.live==0&&h.records==0&&h.destroys==3);
    /* Root alias reset failure cannot claim both modes completed. */
    h=(struct hook_state){0};a=fresh();
    assert(nested_ept_contexts_init_mixed(&r,&ops,&a,48,2,false,lifecycle,&h));
    assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
    ad0=r.active;a.fail_eptp=ad0->tree.eptp;
    assert(nested_ept_contexts_select(&r,0x1005e)==NESTED_CONTEXT_OK);
    ad1=r.active;
    assert(nested_ept_contexts_invalidate(&r,1,0x1005e)==NESTED_CONTEXT_FLUSH_FAILED);
    assert(ad1->tree.generation==2&&ad0->tree.generation==1&&h.resets==1&&r.poisoned);
    assert(!nested_ept_contexts_destroy(&r)&&h.live==1);
    a.fail_eptp=0;assert(nested_ept_contexts_destroy(&r)&&a.live==0&&h.live==0);
    puts("PASS dynamic EPT contexts, mixed AD aliases and inline lifecycle ownership");

}
