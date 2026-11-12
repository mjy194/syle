#include "../nested_owner_pool.h"
#include "../nested_ept_contexts.h"
#include <assert.h>
#include <stdio.h>
static _Alignas(4096) unsigned char mem[32][4096];
static struct nested_sparse_page pages[32];
static struct nested_owner_slot slots[32];
struct state {struct nested_owner_pool pool;bool fail;uint64_t fail_root;size_t peak;};
static bool alloc(void*c,struct nested_sparse_page*p){struct state*s=c;bool ok=nested_owner_pool_alloc(&s->pool,p);if(s->pool.live>s->peak)s->peak=s->pool.live;return ok;}
static void release(void*c,struct nested_sparse_page p){nested_owner_pool_free(&((struct state*)c)->pool,p);}
static bool flush(void*c,uint64_t root){assert((root&4095)==0x1e);return !((struct state*)c)->fail&&root!=((struct state*)c)->fail_root;}
static const struct nested_sparse_ops ops={alloc,release,flush};
static void init(struct state*s,struct nested_ept_contexts*r,size_t count){
 *s=(struct state){0};
 assert(nested_owner_pool_init(&s->pool,0,2,pages,slots,count,48));
 assert(nested_ept_contexts_init_budget(r,&ops,s,48,2,true));
}
static void clean(struct state*s,struct nested_ept_contexts*r){
 nested_ept_contexts_release_all_after_clear(r);s->fail=false;s->fail_root=0;
 assert(nested_ept_contexts_destroy(r)&&!s->pool.live&&!s->pool.fault);
 assert(nested_owner_pool_release_all(&s->pool,0,2,true));
}
static void eviction_tests(void){
 struct state s;struct nested_ept_contexts r;
 /* Two empty contexts cost six pages. Candidate needs three more BEFORE
  * victim release. Exercise each candidate allocation failure boundary. */
 for(size_t cap=6;cap<9;cap++){
  init(&s,&r,cap);
  assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
  assert(nested_ept_contexts_select(&r,0x1101e)==NESTED_CONTEXT_OK);
  struct nested_ept_context_item*b=nested_ept_contexts_pin_active(&r);
  assert(s.pool.live==6&&b);
  assert(nested_ept_contexts_select(&r,0x1201e)==NESTED_CONTEXT_OOM);
  assert(r.active==b&&b->pins==1&&r.count==2&&s.pool.live==6&&!r.poisoned);
  assert(s.peak==cap);clean(&s,&r);
 }
 init(&s,&r,32);
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item*a=nested_ept_contexts_pin_active(&r);
 struct nested_ept_result mapped={.status=NESTED_EPT_OK,.l1_gpa=0x8000,.hpa=0x800000,
  .effective12=3,.effective02=3,.leaf_resolved=true};
 assert(nested_ept_sparse_fill(&a->tree,0x1000,&mapped)==NESTED_SPARSE_OK);
 assert(s.pool.live==10); /* 3 context + 3 child table pairs + leaf record */
 assert(nested_ept_contexts_select(&r,0x1101e)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item*b=nested_ept_contexts_pin_active(&r);
 assert(s.pool.live==13);
 assert(nested_ept_contexts_select(&r,0x1201e)==NESTED_CONTEXT_LIMIT); /* inactive A pinned */
 assert(r.active==b&&a->pins==1&&b->pins==1&&s.pool.live==13);
 assert(nested_ept_contexts_unpin(&r,a));
 assert(nested_ept_contexts_select(&r,0x1201e)==NESTED_CONTEXT_OK); /* evicts A only */
 struct nested_ept_context_item*c=nested_ept_contexts_pin_active(&r);
 assert(c&&b->pins==1&&r.count==2&&s.pool.live==6&&s.peak==16);
 assert(nested_ept_contexts_unpin(&r,b));
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK); /* B victim; A rebuilt */
 struct nested_ept_result out;
 assert(nested_ept_contexts_lookup(&r,0x1000,1,&out)==NESTED_SPARSE_MISS);
 assert(r.active->key==0x1001e&&c->pins==1&&s.pool.live==6);
 clean(&s,&r);
 /* Victim invalidation failure retains BOTH the victim and new candidate. */
 init(&s,&r,9);
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);a=r.active;
 assert(nested_ept_contexts_select(&r,0x1101e)==NESTED_CONTEXT_OK);
 b=nested_ept_contexts_pin_active(&r);s.fail_root=a->tree.eptp;
 assert(nested_ept_contexts_select(&r,0x1201e)==NESTED_CONTEXT_FLUSH_FAILED);
 assert(r.poisoned&&r.count==3&&r.active==b&&b->pins==1&&s.pool.live==9&&s.peak==9);
 assert(!nested_ept_contexts_destroy(&r)&&s.pool.live==9); /* pin protects all */
 nested_ept_contexts_release_all_after_clear(&r);s.fail=true;
 assert(!nested_ept_contexts_destroy(&r)&&s.pool.live==9); /* retry still fails */
 clean(&s,&r);
 puts("eviction: empty resident 6 pages + candidate 3 = peak 9; filled A/B peak 16");
}
int main(void){
 struct state s={0};for(unsigned i=0;i<32;i++)pages[i]=(struct nested_sparse_page){0x100000+4096*i,mem[i]};
 assert(nested_owner_pool_init(&s.pool,0,1,pages,slots,32,48));
 struct nested_sparse_ops ops={alloc,release,flush};struct nested_ept_contexts r;
 assert(nested_ept_contexts_init_budget(&r,&ops,&s,48,2,false));
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item*a=nested_ept_contexts_pin_active(&r);assert(a);
 assert(nested_ept_contexts_select(&r,0x1101e)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item*b=nested_ept_contexts_pin_active(&r);assert(b&&a!=b);
 assert(nested_ept_contexts_unpin(&r,a));
 size_t live=s.pool.live;assert(nested_ept_contexts_select(&r,0x1201e)==NESTED_CONTEXT_LIMIT);
 assert(r.active==b&&s.pool.live==live);
 uint64_t ga=a->tree.generation,gb=b->tree.generation;
 assert(nested_ept_contexts_invalidate(&r,1,0x1001e)==NESTED_CONTEXT_OK);
 assert(a->tree.generation==ga+1&&b->tree.generation==gb);
 assert(nested_ept_contexts_invalidate(&r,2,~0ull)==NESTED_CONTEXT_OK);
 assert(a->tree.generation==ga+2&&b->tree.generation==gb+1);
 s.fail=true;assert(nested_ept_contexts_invalidate(&r,1,0x1101e)==NESTED_CONTEXT_FLUSH_FAILED);
 assert(r.poisoned&&s.pool.live==live);
 assert(!nested_ept_contexts_destroy(&r)&&s.pool.live==live);
 nested_ept_contexts_release_all_after_clear(&r);s.fail=false;
 assert(nested_ept_contexts_destroy(&r)&&s.pool.live==0&&!s.pool.fault);
 assert(nested_owner_pool_release_all(&s.pool,0,1,true));eviction_tests();puts("owner pool contexts: PASS");
}
