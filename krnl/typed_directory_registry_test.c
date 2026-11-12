#include "typed_directory_registry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PAGES 64
static _Alignas(4096) unsigned char memory[PAGES][4096];
struct fixture {
 struct typed_directory directory;struct td_registry_bridge bridge;
 struct nested_ept_contexts registry;struct td_registry_slot slots[32];
 unsigned reserved,live;uint64_t serial;
 bool used[PAGES],fail_alloc,fail_flush,fail_reserve,fail_evidence;
};
static bool alloc_page(void *v,struct nested_sparse_page *p) {
 struct fixture *f=v;if(f->fail_alloc)return false;
 for(unsigned i=0;i<PAGES;i++)if(!f->used[i]){f->used[i]=true;f->live++;*p=(struct nested_sparse_page){0x100000+i*4096,memory[i]};return true;}
 return false;
}
static void free_page(void *v,struct nested_sparse_page p) {
 struct fixture *f=v;unsigned i=(unsigned)((p.hpa-0x100000)/4096);
 assert(i<PAGES&&f->used[i]);f->used[i]=false;f->live--;
}
static bool flush(void *v,uint64_t key) {
 struct fixture *f=v;assert((key&4095)==0x1e);if(f->fail_flush)return false;f->serial++;return true;
}
static bool reserve(void *v,struct td_registry_slot **s,uint64_t *lifetime) {
 struct fixture *f=v;if(f->fail_reserve||f->reserved==32)return false;
 *s=&f->slots[f->reserved++];*lifetime=f->reserved;return true;
}
static bool evidence(void *v,struct td_registry_slot *s,struct td_retire_ticket t,uint64_t *acked,uint64_t *serial) {
 struct fixture *f=v;assert(s->identity.root_id==t.identity.root_id);
 if(f->fail_evidence||(t.history&~UINT64_C(1)))return false;
 *acked=t.history;*serial=f->serial;return true;
}
static void init(struct fixture *f) {
 memset(f,0,sizeof *f);assert(td_init(&f->directory,10,11,3,1));
 struct td_registry_provider provider={reserve,evidence};
 assert(td_registry_bridge_init(&f->bridge,&f->directory,0,&provider,f));
 struct nested_sparse_ops ops={alloc_page,free_page,flush};
 assert(nested_ept_contexts_init_budget(&f->registry,&ops,f,48,2,true));
 assert(nested_ept_contexts_set_transactions(&f->registry,td_registry_transactions(),&f->bridge));
}
static void history(struct fixture *f,unsigned cpu) {
 struct td_record *r;struct td_identity id;
 assert(td_registry_identity(&f->bridge,f->registry.active->transaction_token,&r,&id));
 struct td_operation op;uint64_t n;assert(td_begin(&f->directory,&op,&n));
 assert(td_resident(&f->directory,&op,n,r,id,cpu));assert(td_end(&f->directory,&op,n));
}
int main(void) {
 struct fixture f;init(&f);
 assert(nested_ept_contexts_select(&f.registry,0x101e)==NESTED_CONTEXT_OK);history(&f,0);
 struct nested_ept_context_item *a=nested_ept_contexts_pin_active(&f.registry);assert(a);
 struct td_registry_slot *sa=a->transaction_token;assert(sa->record.pins==1);
 struct td_ticket ticket;assert(td_close(&f.directory));assert(td_seal(&f.directory,&ticket));
 unsigned live=f.live;assert(nested_ept_contexts_select(&f.registry,0x201e)!=NESTED_CONTEXT_OK&&f.live==live);
 assert(td_reopen(&f.directory,ticket,2));assert(nested_ept_contexts_unpin(&f.registry,a)&&sa->record.pins==0);
 assert(nested_ept_contexts_select(&f.registry,0x201e)==NESTED_CONTEXT_OK);
 assert(nested_ept_contexts_select(&f.registry,0x301e)==NESTED_CONTEXT_OK);
 assert(sa->forgotten&&sa->receipt.live&&f.directory.receipts&&!f.bridge.faulted);
 assert(nested_ept_contexts_invalidate(&f.registry,2,0)==NESTED_CONTEXT_OK);
 for(struct td_registry_slot *s=f.bridge.slots;s;s=s->next)if(!s->forgotten)assert(s->record.cache_generation==2);
 assert(nested_ept_contexts_destroy(&f.registry)&&f.live==0&&!f.bridge.faulted);
 init(&f);f.fail_reserve=true;
 assert(nested_ept_contexts_select(&f.registry,0x101e)==NESTED_CONTEXT_OOM&&f.live==0&&!f.directory.records&&!f.bridge.busy);
 init(&f);f.fail_alloc=true;
 assert(nested_ept_contexts_select(&f.registry,0x101e)==NESTED_CONTEXT_OOM&&f.slots[0].forgotten&&!f.directory.records&&!f.bridge.faulted);
 init(&f);f.fail_flush=true;
 assert(nested_ept_contexts_select(&f.registry,0x101e)==NESTED_CONTEXT_FLUSH_FAILED);
 assert(f.slots[0].record.attached&&f.slots[0].record.cache_generation==0&&f.slots[0].record.state==TD_RETAINED&&!f.bridge.busy&&!f.bridge.faulted);
 f.fail_flush=false;assert(nested_ept_contexts_destroy(&f.registry)&&f.live==0&&f.slots[0].receipt.live);
 init(&f);assert(nested_ept_contexts_select(&f.registry,0x101e)==NESTED_CONTEXT_OK);history(&f,0);
 f.fail_evidence=true;live=f.live;assert(!nested_ept_contexts_destroy(&f.registry)&&f.live==live&&!f.slots[0].receipt.live);
 uint64_t before_epoch=f.directory.type_epoch,before_nonce=f.directory.next_nonce;
 assert(f.slots[0].record.state==TD_RETIRING&&!td_close(&f.directory));
 assert(!f.directory.closed&&f.directory.type_epoch==before_epoch&&f.directory.next_nonce==before_nonce);
 assert(!td_seal(&f.directory,&ticket));
 f.fail_evidence=false;assert(nested_ept_contexts_destroy(&f.registry)&&f.live==0);
 assert(td_close(&f.directory)&&td_seal(&f.directory,&ticket));
 assert(td_reopen(&f.directory,ticket,before_epoch+1));
 init(&f);assert(nested_ept_contexts_select(&f.registry,0x101e)==NESTED_CONTEXT_OK);history(&f,1);
 live=f.live;assert(!nested_ept_contexts_destroy(&f.registry)&&f.live==live&&!f.slots[0].receipt.live); /* no fabricated remote ACK */
 init(&f);assert(nested_ept_contexts_select(&f.registry,0x101e)==NESTED_CONTEXT_OK);
 f.directory.version=UINT64_MAX;a=nested_ept_contexts_pin_active(&f.registry);
 assert(a&&a->pins==1&&f.bridge.faulted&&f.directory.poisoned);
 assert(!nested_ept_contexts_pin_active(&f.registry));assert(!td_close(&f.directory));
 assert(f.live&&!nested_ept_contexts_destroy(&f.registry)); /* uncertainty retains actual pin */
 puts("typed_directory_registry PASS");return 0;
}
