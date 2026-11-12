#include "../typed_directory.h"
#include "../nested_ept_contexts.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define N 64
static _Alignas(4096) unsigned char pages[N][4096];
struct reservation {struct td_record record;struct td_identity id;struct td_receipt receipt;struct td_retire_ticket ticket;bool used;};
struct fixture {struct typed_directory d;struct td_operation op;uint64_t nonce,lifetime,flush_serial;struct reservation token[N];bool used[N],fail_alloc,fail_flush,fail_receipt;unsigned live,allocs,flushes;};
static bool allocate(void *v,struct nested_sparse_page *p){struct fixture *f=v;f->allocs++;if(f->fail_alloc)return false;for(unsigned i=0;i<N;i++)if(!f->used[i]){f->used[i]=true;f->live++;*p=(struct nested_sparse_page){0x100000+i*4096,pages[i]};return true;}return false;}
static void release(void *v,struct nested_sparse_page p){struct fixture *f=v;unsigned i=(unsigned)((p.hpa-0x100000)/4096);assert(i<N&&f->used[i]);f->used[i]=false;f->live--;}
static bool flush(void *v,uint64_t e){struct fixture *f=v;assert((e&4095)==0x1e);f->flushes++;if(f->fail_flush)return false;f->flush_serial++;return true;}
static bool begin(void *v,enum nested_context_operation op){(void)op;struct fixture *f=v;return td_begin(&f->d,&f->op,&f->nonce);}
static void end(void *v,enum nested_context_operation op){(void)op;struct fixture *f=v;assert(td_end(&f->d,&f->op,f->nonce));}
static bool reserve(void *v,uint64_t key,void **out){(void)key;struct fixture *f=v;for(unsigned i=0;i<N;i++)if(!f->token[i].used){struct reservation *r=&f->token[i];if(!td_reserve(&f->d,&f->op,f->nonce,&r->record,0,++f->lifetime,&r->id))return false;r->used=true;*out=r;return true;}return false;}
static bool cancel(void *v,void *token){struct fixture *f=v;struct reservation *r=token;return td_cancel(&f->d,&f->op,f->nonce,&r->record,r->id);}
static bool attach(void *v,void *token,struct nested_ept_context_item *p,bool ok){struct fixture *f=v;struct reservation *r=token;if(!td_attach(&f->d,&f->op,f->nonce,&r->record,r->id,p->tree.eptp,p->tree.eptp&~UINT64_C(4095),p->tree.generation,0))return false;return ok||td_retain(&f->d,&f->op,f->nonce,&r->record,r->id);}
static void retain(void *v,void *token,struct nested_ept_context_item *p){(void)p;struct fixture *f=v;struct reservation *r=token;assert(td_retain(&f->d,&f->op,f->nonce,&r->record,r->id));}
static bool publish(void *v,void *token,struct nested_ept_context_item *p){(void)p;struct fixture *f=v;struct reservation *r=token;return td_publish(&f->d,&f->op,f->nonce,&r->record,r->id);}
static bool reset(void *v,void *token,struct nested_ept_context_item *p){struct fixture *f=v;struct reservation *r=token;return td_cache_reset(&f->d,&f->op,f->nonce,&r->record,r->id,p->tree.generation);}
static bool retire(void *v,void *token,struct nested_ept_context_item *p){(void)p;struct fixture *f=v;struct reservation *r=token;if(r->record.state!=TD_RETIRING&&!td_retire_begin(&f->d,&f->op,f->nonce,&r->record,r->id,&r->ticket))return false;
 /* This test backend executes only owner 0's flush. A remote historical bit
  * cannot be fabricated from that local success. Receipt storage survives
  * all tree/item frees and is deliberately never recycled by this fixture. */
 if(f->fail_receipt||(r->record.history&~UINT64_C(1)))return false;
 return td_retire_commit(&f->d,&f->op,f->nonce,&r->record,r->id,r->ticket,r->record.history,f->flush_serial,&r->receipt)&&td_forget(&f->d,&f->op,f->nonce,&r->record,r->id);}
static const struct nested_context_transactions tx={.begin=begin,.end=end,.reserve=reserve,.cancel=cancel,.attach=attach,.publish=publish,.reset=reset,.retire=retire,.retain=retain};
static void init(struct fixture *f,struct nested_ept_contexts *r){memset(f,0,sizeof(*f));assert(td_init(&f->d,7,9,3,1));struct nested_sparse_ops ops={allocate,release,flush};assert(nested_ept_contexts_init_budget(r,&ops,f,48,2,true));assert(nested_ept_contexts_set_transactions(r,&tx,f));}
static void history(struct fixture *f,struct nested_ept_context_item *p,unsigned cpu){struct reservation *r=p->transaction_token;assert(begin(f,NESTED_CONTEXT_SELECT));assert(td_resident(&f->d,&f->op,f->nonce,&r->record,r->id,cpu));end(f,NESTED_CONTEXT_SELECT);}
int main(void){struct fixture f;struct nested_ept_contexts r;struct td_ticket ticket;struct td_snapshot snap[32];size_t count;
 init(&f,&r);assert(nested_ept_contexts_select(&r,0x101e)==NESTED_CONTEXT_OK);history(&f,r.active,0);struct td_identity old=((struct reservation *)r.active->transaction_token)->id;assert(nested_ept_contexts_select(&r,0x201e)==NESTED_CONTEXT_OK);assert(nested_ept_contexts_select(&r,0x301e)==NESTED_CONTEXT_OK);assert(f.d.receipts&&f.d.receipts->identity.root_id==old.root_id);assert(td_close(&f.d)&&td_seal(&f.d,&ticket));assert(td_snapshot_read(&f.d,ticket,snap,32,&count)&&count==3);unsigned allocs=f.allocs;assert(nested_ept_contexts_select(&r,0x401e)!=NESTED_CONTEXT_OK&&f.allocs==allocs);assert(td_reopen(&f.d,ticket,2));assert(nested_ept_contexts_invalidate(&r,2,0)==NESTED_CONTEXT_OK);assert(nested_ept_contexts_destroy(&r)&&f.live==0);
 init(&f,&r);f.fail_alloc=true;assert(nested_ept_contexts_select(&r,0x101e)==NESTED_CONTEXT_OOM);assert(!f.d.records&&f.d.operations==0&&f.live==0);f.fail_alloc=false;assert(nested_ept_contexts_destroy(&r));
 init(&f,&r);f.fail_flush=true;assert(nested_ept_contexts_select(&r,0x101e)==NESTED_CONTEXT_FLUSH_FAILED);assert(f.d.records&&f.d.records->attached&&f.d.records->state==TD_RETAINED&&f.live);f.fail_flush=false;assert(nested_ept_contexts_destroy(&r)&&f.live==0&&f.d.receipts);
 init(&f,&r);assert(nested_ept_contexts_select(&r,0x101e)==NESTED_CONTEXT_OK);history(&f,r.active,0);unsigned live=f.live;f.fail_receipt=true;assert(!nested_ept_contexts_destroy(&r)&&f.live==live&&!f.d.receipts);f.fail_receipt=false;assert(nested_ept_contexts_destroy(&r)&&f.live==0&&f.d.receipts);
 init(&f,&r);assert(nested_ept_contexts_select(&r,0x101e)==NESTED_CONTEXT_OK);history(&f,r.active,1);live=f.live;assert(!nested_ept_contexts_destroy(&r)&&f.live==live&&!f.d.receipts); /* deliberately retained: no remote receipt */
 puts("PASS typed directory with real registry reserve/init-fail/eviction/freeze/reset/durable-before-free/remote-history rejection");return 0;}
