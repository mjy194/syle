#include "../nested_owner_typed.h"
#include "../paging.h"
#include "../vmx_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) unsigned char pages[32][4096];
static struct nested_owner_sparse owner;
static struct nested_owner_typed states[2];
static struct ept_fixture fixture;
struct token {bool reserved,attached,published;};
static struct {struct token tokens[2];unsigned reservations,allocations,flushes,ready;bool busy,ready_fail,alloc_fail,flush_fail;} test;
void *paging_phys_to_virt(uint64_t h){(void)h;return NULL;}
int vmx_memory_region_wb(uint64_t h){(void)h;return 0;}
static bool guard(void *p){(void)p;return true;}
static void fatal(void *p){(void)p;}
static bool begin(void *p,enum nested_context_operation kind){(void)p;(void)kind;assert(!test.busy);test.busy=true;return true;}
static void end(void *p,enum nested_context_operation kind){(void)p;(void)kind;assert(test.busy);test.busy=false;}
static bool reserve(void *p,uint64_t key,void **out){(void)p;assert(test.busy&&test.reservations<2);assert(key==(test.reservations?0x1401e:0x1001e));struct token *t=&test.tokens[test.reservations++];t->reserved=true;*out=t;return true;}
static bool cancel(void *p,void *v){(void)p;struct token *t=v;assert(t->reserved&&!t->attached);t->reserved=false;return true;}
static bool attach(void *p,void *v,struct nested_ept_context_item *i,bool ok){(void)p;(void)ok;struct token*t=v;assert(t->reserved&&i->tree.root);t->attached=true;return true;}
static void retain(void *p,void *v,struct nested_ept_context_item *i){(void)p;struct token*t=v;assert(t->reserved&&i->tree.root);}
static bool publish(void *p,void *v,struct nested_ept_context_item *i){(void)p;(void)i;struct token*t=v;assert(t->attached);t->published=true;return true;}
static bool reset(void *p,void *v,struct nested_ept_context_item *i){(void)p;(void)v;(void)i;return true;}
static bool retire(void *p,void *v,struct nested_ept_context_item *i){(void)p;struct token*t=v;assert(t->attached&&i->pins==0&&test.flushes);return true;}
static const struct nested_context_transactions tx={begin,end,reserve,cancel,attach,retain,publish,reset,retire};
static bool alloc(void *ctx,struct nested_sparse_page *out){struct nested_owner_sparse*s=ctx;test.allocations++;assert(test.reservations&&test.busy);return !test.alloc_fail&&nested_owner_pool_alloc(&s->pool,out);}
static void release(void *ctx,struct nested_sparse_page p){struct nested_owner_sparse*s=ctx;nested_owner_pool_free(&s->pool,p);}
static bool flush(void *ctx,uint64_t eptp){(void)ctx;assert((eptp&4095)==0x1e);test.flushes++;return !test.flush_fail;}
static const struct nested_sparse_ops ops={alloc,release,flush};
static bool refill_begin(struct nested_owner_sparse*s){(void)s;return true;}
static bool refill_end(struct nested_owner_sparse*s){(void)s;return true;}
static bool ready(struct nested_owner_sparse*s){
 assert(s->contexts.count==2&&s->creations==2&&s->contexts.active->key==0x1001e);
 assert(states[0].bound&&states[1].bound&&states[0].tree!=states[1].tree);
 assert(states[0].tree->typed_context==&states[0]&&states[1].tree->typed_context==&states[1]);
 assert(states[0].eptp12==0x1001e&&states[1].eptp12==0x1401e);
 test.ready++;return !test.ready_fail;
}
static void configure(void){
 memset(&owner,0,sizeof owner);memset(states,0,sizeof states);memset(&test,0,sizeof test);
 unsigned a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
 for(unsigned i=0;i<32;i++)owner.pages[i]=(struct nested_sparse_page){0x100000+i*4096,pages[i]};
 assert(nested_owner_pool_init(&owner.pool,b>>24,1,owner.pages,owner.slots,32,48));
 owner.multiple=true;owner.initialized=true;owner.fixture=&fixture;owner.selected12=0x1001e;
 for(unsigned i=0;i<2;i++){states[i].environment_guard=guard;states[i].fatal=fatal;owner.typed_contexts[i]=&states[i];}
 /* This is the actual typed_configure shape: it has no preparation callback. */
 owner.typed_transactions=&tx;owner.typed_transaction_context=&test;
 owner.contexts_ready=ready;owner.refill_begin=refill_begin;owner.refill_end=refill_end;
}
static void reject_without_allocation(void){assert(!nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));assert(test.allocations==0&&test.reservations==0&&owner.pool.live==0&&test.ready==0);}
int main(void){
 configure();owner.contexts_ready=NULL;reject_without_allocation();
 configure();owner.refill_begin=NULL;reject_without_allocation();
 configure();owner.refill_end=NULL;reject_without_allocation();
 configure();owner.typed_transactions=NULL;reject_without_allocation();
 configure();owner.typed_contexts[0]=NULL;reject_without_allocation();
 configure();owner.typed_contexts[1]=NULL;reject_without_allocation();
 configure();owner.typed_contexts[1]=owner.typed_contexts[0];reject_without_allocation();
 configure();states[0].environment_guard=NULL;reject_without_allocation();
 configure();states[1].fatal=NULL;reject_without_allocation();
 configure();struct nested_context_transactions missing=tx;missing.retain=NULL;owner.typed_transactions=&missing;reject_without_allocation();
 configure();owner.evict=true;reject_without_allocation();
 configure();assert(nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));
 assert(test.reservations==2&&test.allocations==6&&test.flushes==2&&test.ready==1&&owner.pool.live==6);
 assert(test.tokens[0].published&&test.tokens[1].published&&!test.busy);
 unsigned allocations=test.allocations;assert(!nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner)&&test.allocations==allocations);
 assert(nested_owner_sparse_reset(&owner)); /* Actual wrapper resets both via injected registry ops. */
 for(struct nested_ept_context_item*i=owner.contexts.items;i;i=i->next)assert(i->tree.generation==2);
 assert(nested_owner_sparse_stop(&owner)&&owner.pool.live==0&&owner.destroyed);
 configure();test.alloc_fail=true;assert(!nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));assert(test.reservations==1&&owner.pool.live==0&&!test.tokens[0].reserved);
 configure();test.flush_fail=true;assert(!nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));assert(owner.pool.live&&owner.contexts.poisoned&&test.tokens[0].attached);test.flush_fail=false;assert(nested_owner_sparse_stop(&owner)&&owner.pool.live==0);
 configure();test.ready_fail=true;assert(!nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));assert(owner.pool.live==6&&test.ready==1);assert(nested_owner_sparse_stop(&owner)&&owner.pool.live==0);
 puts("PASS production typed-context bootstrap real registry assembly, missing wiring rejection before allocation and failure retention");return 0;
}
