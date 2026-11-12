#include "nested_ept_contexts.h"
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
struct token { bool live,attached,published,retained; unsigned flush; };
struct tx {
 struct alloc_state *a; struct token token[16]; unsigned n,depth,begins,ends,retired;
 bool closed,fail_reserve,fail_cancel,fail_attach,fail_publish,fail_retire,fail_reset;
};
static bool begin(void *v,enum nested_context_operation op) {
 struct tx *t=v;(void)op;if(t->closed)return false;
 assert(!t->depth);t->depth=1;t->begins++;return true;
}
static void end(void *v,enum nested_context_operation op) {
 struct tx *t=v;(void)op;assert(t->depth==1);t->depth=0;t->ends++;
}
static bool reserve(void *v,uint64_t key,void **out) {
 struct tx *t=v;assert(t->depth&&key);if(t->fail_reserve)return false;
 assert(t->n<16);struct token *p=&t->token[t->n++];*p=(struct token){.live=true};*out=p;return true;
}
static bool cancel(void *v,void *token) {
 struct tx *t=v;struct token *p=token;assert(t->depth&&p->live&&!p->attached);
 if(t->fail_cancel)return false;
 p->live=false;return true;
}
static bool attach(void *v,void *token,struct nested_ept_context_item *item,bool ok) {
 struct tx *t=v;struct token *p=token;
 assert(t->depth&&p->live&&item->tree.root&&item->transaction_token==p);
 assert(ok?!item->tree.poisoned:item->tree.poisoned);
 p->attached=true;p->flush=t->a->flushes;return !t->fail_attach;
}
static void retain(void *v,void *token,struct nested_ept_context_item *item) {
 struct tx *t=v;struct token *p=token;assert(t->depth&&p->live&&item->tree.root);
 p->attached=true;p->retained=true;
}
static bool publish(void *v,void *token,struct nested_ept_context_item *item) {
 struct tx *t=v;struct token *p=token;assert(t->depth&&p->attached&&item->tree.root);
 if(t->fail_publish)return false;
 p->published=true;return true;
}
static bool reset(void *v,void *token,struct nested_ept_context_item *item) {
 struct tx *t=v;struct token *p=token;assert(t->depth&&p->attached&&item->tree.generation>1);
 return !t->fail_reset;
}
static bool retire(void *v,void *token,struct nested_ept_context_item *item) {
 struct tx *t=v;struct token *p=token;
 assert(t->depth&&p->live&&p->attached&&item->tree.root&&t->a->live>=3);
 assert(t->a->flushes>p->flush);p->flush=t->a->flushes;
 if(t->fail_retire)return false;
 p->live=false;t->retired++;return true;
}
static const struct nested_context_transactions transactions={begin,end,reserve,cancel,attach,retain,publish,reset,retire};
static void init(struct nested_ept_contexts *r,struct alloc_state *a,struct tx *t) {
 setup(r,a,2,true);*t=(struct tx){.a=a};assert(nested_ept_contexts_set_transactions(r,&transactions,t));
 assert(!nested_ept_contexts_set_transactions(r,&transactions,t));
}
int main(void) {
 struct nested_ept_contexts r;struct alloc_state a;struct tx t;
 init(&r,&a,&t);t.fail_reserve=true;
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OOM&&a.calls==0&&t.n==0);
 t.fail_reserve=false;t.closed=true;
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_INVALID&&a.calls==0);
 t.closed=false;a.fail_alloc=0;
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OOM&&!t.token[0].live);
 a.fail_alloc=~0u;assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item *old=r.active;
 t.closed=true;assert(!nested_ept_contexts_pin_active(&r));
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_INVALID&&r.active==old);
 assert(!nested_ept_contexts_destroy(&r));t.closed=false;
 assert(nested_ept_contexts_pin_active(&r)==old);assert(!nested_ept_contexts_destroy(&r));
 assert(nested_ept_contexts_unpin(&r,old));
 t.fail_retire=true;unsigned live=a.live;
 assert(!nested_ept_contexts_destroy(&r)&&a.live==live&&r.poisoned&&old->tree.root);
 t.fail_retire=false;assert(nested_ept_contexts_destroy(&r)&&a.live==0&&t.retired==1&&t.begins==t.ends);
 /* Initial flush failure still attaches a root and retires it on retry. */
 init(&r,&a,&t);a.fail_flush=0;
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_FLUSH_FAILED);
 assert(t.token[0].attached&&!t.token[0].published&&a.live==3&&r.items);
 a.fail_flush=~0u;assert(nested_ept_contexts_destroy(&r)&&a.live==0&&t.retired==1);
 /* Attach/publication failure cannot abandon root pages or select it. */
 for(unsigned mode=0;mode<2;mode++) {
  init(&r,&a,&t);t.fail_attach=mode==0;t.fail_publish=mode==1;
  assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_POISONED);
  assert(!r.active&&r.items&&a.live==3);
  assert(nested_ept_contexts_destroy(&r)&&a.live==0);
 }
 init(&r,&a,&t);a.fail_alloc=0;t.fail_cancel=true;
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_POISONED&&a.live==0&&t.token[0].live);
 /* Reservation is external retained ownership, not silently consumed. */
 t.fail_cancel=false;assert(begin(&t,NESTED_CONTEXT_DISPOSE));assert(cancel(&t,&t.token[0]));end(&t,NESTED_CONTEXT_DISPOSE);
 assert(nested_ept_contexts_destroy(&r));
 /* Failed victim receipt retains old tree AND offline candidate. */
 init(&r,&a,&t);
 assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
 assert(nested_ept_contexts_select(&r,0x2001e)==NESTED_CONTEXT_OK);old=r.active;
 t.fail_retire=true;
 assert(nested_ept_contexts_select(&r,0x3001e)==NESTED_CONTEXT_FLUSH_FAILED);
 assert(r.active==old&&r.count==3&&a.live==9&&t.token[2].attached&&t.token[2].retained&&!t.token[2].published);
 t.fail_retire=false;assert(nested_ept_contexts_destroy(&r)&&a.live==0&&t.retired==3);
 init(&r,&a,&t);assert(nested_ept_contexts_select(&r,0x1001e)==NESTED_CONTEXT_OK);
 t.fail_reset=true;assert(nested_ept_contexts_invalidate(&r,1,0x1001e)==NESTED_CONTEXT_POISONED);
 assert(r.items&&r.items->tree.generation==2&&t.token[0].live);
 assert(nested_ept_contexts_destroy(&r)&&a.live==0);

 puts("PASS opt-in context transaction reserve/freeze/receipt/failure retention");
}
