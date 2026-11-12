#include "typed_directory_registry.h"
static void fault(struct td_registry_bridge *b) {b->faulted=true;td_poison(b->directory);}
static struct td_registry_slot *slot(struct td_registry_bridge *b,void *token) {
 if(!b||!token)return NULL;
 for(struct td_registry_slot *p=b->slots;p;p=p->next)
  if(p==token)return p->bridge==b&&p->reserved&&!p->forgotten?p:NULL;
 return NULL;
}
static bool begin(void *ctx,enum nested_context_operation kind) {
 struct td_registry_bridge *b=ctx;
 if(!b||b->faulted||b->busy||!td_begin(b->directory,&b->operation,&b->nonce))return false;
 b->busy=true;b->kind=kind;return true;
}
static void end(void *ctx,enum nested_context_operation kind) {
 struct td_registry_bridge *b=ctx;
 if(!b||!b->busy||b->kind!=kind){if(b)fault(b);return;}
 /* PIN APIs have no per-item callbacks. Reconcile the actual registry pin
  * count before ending their enclosing serialized operation. No observer is
  * allowed between the registry mutation and this callback. */
 if(!b->faulted&&(kind==NESTED_CONTEXT_PIN||kind==NESTED_CONTEXT_UNPIN||kind==NESTED_CONTEXT_CLEAR_PINS)) {
  for(struct td_registry_slot *s=b->slots;s;s=s->next)if(!s->forgotten&&s->item) {
   while(s->record.pins<s->item->pins)
    if(!td_pin(b->directory,&b->operation,b->nonce,&s->record,s->identity)){fault(b);break;}
   if(b->faulted)break;
   while(s->record.pins>s->item->pins)
    if(!td_unpin(b->directory,&b->operation,b->nonce,&s->record,s->identity)){fault(b);break;}
   if(b->faulted)break;
  }
 }
 if(!td_end(b->directory,&b->operation,b->nonce)){fault(b);return;}
 b->busy=false;
}
static bool reserve(void *ctx,uint64_t key,void **out) {
 struct td_registry_bridge *b=ctx;struct td_registry_slot *s=NULL;uint64_t life=0;
 if(!b||b->faulted||!b->busy||!out||!b->provider.reserve(b->provider_context,&s,&life))return false;
 if(!s||!life){fault(b);return false;}
 for(struct td_registry_slot *p=b->slots;p;p=p->next)if(p==s){fault(b);return false;}
 *s=(struct td_registry_slot){.next=b->slots,.bridge=b,.virtual_key=key};
 b->slots=s; /* Stable storage remains owned even if registration fails. */
 if(!td_reserve(b->directory,&b->operation,b->nonce,&s->record,b->owner,life,&s->identity)){fault(b);return false;}
 s->reserved=true;*out=s;return true;
}
static bool cancel(void *ctx,void *token) {
 struct td_registry_bridge *b=ctx;struct td_registry_slot *s=slot(b,token);
 if(!s||!b->busy||b->faulted||!td_cancel(b->directory,&b->operation,b->nonce,&s->record,s->identity))return false;
 s->forgotten=true;return true;
}
static bool attach(void *ctx,void *token,struct nested_ept_context_item *item,bool init_ok) {
 struct td_registry_bridge *b=ctx;struct td_registry_slot *s=slot(b,token);
 if(!s||!b->busy||b->faulted||!item||s->item||item->key!=s->virtual_key)return false;
 s->item=item; /* keep owner pointer even if subsequent binding rejects */
 if(!td_attach(b->directory,&b->operation,b->nonce,&s->record,s->identity,item->tree.eptp,
               item->tree.eptp&UINT64_C(0x000ffffffffff000),item->tree.generation,0))return false;
 if(!init_ok)return td_retain(b->directory,&b->operation,b->nonce,&s->record,s->identity);
 return true;
}
static void retain(void *ctx,void *token,struct nested_ept_context_item *item) {
 struct td_registry_bridge *b=ctx;struct td_registry_slot *s=slot(b,token);
 if(!s||!b->busy||!item){if(b)fault(b);return;}
 s->item=item;
 if(!s->record.attached&&!td_attach(b->directory,&b->operation,b->nonce,&s->record,s->identity,
    item->tree.eptp,item->tree.eptp&UINT64_C(0x000ffffffffff000),item->tree.generation,0)){fault(b);return;}
 if(!td_retain(b->directory,&b->operation,b->nonce,&s->record,s->identity))fault(b);
}
static bool publish(void *ctx,void *token,struct nested_ept_context_item *item) {
 struct td_registry_bridge *b=ctx;struct td_registry_slot *s=slot(b,token);
 return s&&b->busy&&!b->faulted&&s->item==item&&
  td_publish(b->directory,&b->operation,b->nonce,&s->record,s->identity);
}
static bool reset(void *ctx,void *token,struct nested_ept_context_item *item) {
 struct td_registry_bridge *b=ctx;struct td_registry_slot *s=slot(b,token);
 if(!s||!b->busy||b->faulted||s->item!=item)return false;
 if(!td_cache_reset(b->directory,&b->operation,b->nonce,&s->record,s->identity,item->tree.generation)){fault(b);return false;}
 return true;
}
static bool retire(void *ctx,void *token,struct nested_ept_context_item *item) {
 struct td_registry_bridge *b=ctx;struct td_registry_slot *s=slot(b,token);uint64_t ack=0,serial=0;
 if(!s||!b->busy||b->faulted||s->item!=item||item->pins)return false;
 if(s->record.state!=TD_RETIRING&&!td_retire_begin(b->directory,&b->operation,b->nonce,&s->record,s->identity,&s->retirement))return false;
 if(!b->provider.flush_evidence(b->provider_context,s,s->retirement,&ack,&serial))return false;
 if(!td_retire_commit(b->directory,&b->operation,b->nonce,&s->record,s->identity,s->retirement,ack,serial,&s->receipt))return false;
 if(!td_forget(b->directory,&b->operation,b->nonce,&s->record,s->identity)){fault(b);return false;}
 s->forgotten=true;s->item=NULL;return true;
}
static const struct nested_context_transactions tx={begin,end,reserve,cancel,attach,retain,publish,reset,retire};
const struct nested_context_transactions *td_registry_transactions(void) {return &tx;}
bool td_registry_bridge_init(struct td_registry_bridge *b,struct typed_directory *d,uint64_t owner,const struct td_registry_provider *p,void *context) {
 if(!b||!d||!p||!p->reserve||!p->flush_evidence||!d->domain||d->closed||d->poisoned||owner>=64||!(d->participants&(UINT64_C(1)<<owner)))return false;
 *b=(struct td_registry_bridge){.directory=d,.provider=*p,.provider_context=context,.owner=owner};return true;
}
bool td_registry_identity(struct td_registry_bridge *b,void *token,struct td_record **r,struct td_identity *id) {
 struct td_registry_slot *s=slot(b,token);
 if(!s||b->faulted||!r||!id)return false;
 *r=&s->record;*id=s->identity;return true;
}
