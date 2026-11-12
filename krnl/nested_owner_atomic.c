#include "nested_owner_atomic.h"
#include "nested_owner_sparse.h"
#define ADDRESS UINT64_C(0x000ffffffffff000)
static unsigned current(struct nested_owner_atomic *a){return a->config.current_cpu(a->config.context);}
bool nested_owner_atomic_stopped(void *opaque){
 struct nested_owner_atomic *a=opaque;
 return a&&a->sparse&&a->vcpu&&!a->failed&&a->physical_ad&&
  a->sparse->pool.active&&!a->sparse->pool.fault&&
  (!a->healthy||a->healthy(a->healthy_context))&&
  current(a)==a->sparse->pool.owner&&current(a)==a->vcpu->owner_apic&&
  !a->vcpu->busy&&!a->vcpu->poison&&
  nested_domain_dispatch_consumer_stopped(a->dispatch,a->sparse->pool.owner,a->token);
}
bool nested_owner_atomic_allowed(void *opaque,uint64_t key){
 struct nested_owner_atomic *a=opaque;struct nrc_root root;
 return a&&!a->failed&&a->config.catalog&&
  nrc_find(a->config.catalog,a->config.catalog_identity,key,&root);
}
bool nested_owner_atomic_empty(struct nested_owner_sparse *s,const struct nested_sparse_ops *ops,void *ctx){
 struct nested_owner_atomic *a=s?s->atomic_catalog:0;
 if(!a||a->sparse!=s||!nested_owner_atomic_stopped(a)||!s->initialized||s->destroyed||
    !s->fixture||!s->multiple||!s->evict||s->contexts.initialized||s->creations||
    s->selected12!=0x1005e||!nested_owner_atomic_allowed(a,s->selected12))return false;
 if(!nested_ept_contexts_init_config(&s->contexts,ops,ctx,s->pool.maxphys,2,true,true,0,0)||
    !tr_atomic_init(&a->atomic,&a->bridge,&s->contexts,a->lease,s->pool.owner,
     &a->root_provider,a->provider_context,&a->config))return false;
 return true;
}
bool nested_owner_atomic_initial_select(struct nested_owner_atomic *a){
 struct nested_owner_sparse *s=a?a->sparse:0;
 if(!s||!nested_owner_atomic_stopped(a)||!a->atomic.initialized||!s->contexts.initialized||
    s->creations||s->contexts.count||s->contexts.items||s->contexts.active||
    (a->empty_ready&&!a->atomic.maintenance&&!a->atomic.replacement)||
    nested_ept_contexts_select(&s->contexts,s->selected12)!=NESTED_CONTEXT_OK)return false;
 s->creations=1;return true;
}
bool nested_owner_atomic_bootstrap(struct nested_owner_sparse *s,const struct nested_sparse_ops *ops,void *ctx){
 if(!nested_owner_atomic_empty(s,ops,ctx))return false;
 struct nested_owner_atomic *a=s->atomic_catalog;
 return (!a->empty_ready||a->empty_ready(a,a->empty_context))&&nested_owner_atomic_initial_select(a);
}
static bool private_permissions(struct nested_owner_atomic *,uint64_t,uint8_t *,uint64_t *);
static void record_rejection(struct nested_owner_atomic *a,uint64_t gpa,uint64_t q,uint64_t pin,
 enum nested_atomic_reject_stage stage,bool permissions_valid,uint8_t actual,int bridge_status){
 if(!a||__atomic_load_n(&a->rejection.valid,__ATOMIC_ACQUIRE))return;
 a->rejection.gpa=gpa;a->rejection.qualification=q;a->rejection.pin_controls=pin;
 a->rejection.reason=48;a->rejection.stage=stage;a->rejection.bridge_status=bridge_status;
 a->rejection.actual_permissions=actual;a->rejection.permissions_valid=permissions_valid;
 __atomic_store_n(&a->rejection.valid,1,__ATOMIC_RELEASE);
}
static bool reject_fault(struct nested_owner_atomic *a,uint64_t gpa,uint64_t q,uint64_t pin,
 enum nested_atomic_reject_stage stage,bool permissions_valid,uint8_t actual,int bridge_status){
 record_rejection(a,gpa,q,pin,stage,permissions_valid,actual,bridge_status);
 a->failed=true;return false;
}
bool nested_owner_atomic_fault(struct nested_owner_atomic *a,uint64_t gpa,uint64_t q,uint64_t pin){
 uint8_t actual=0;uint64_t leaf=0;
 if(!nested_owner_atomic_stopped(a)){
  record_rejection(a,gpa,q,pin,NOA_REJECT_STOPPED,false,0,-1);return false;
 }
 /* Intel SDM Vol. 3C, EPT-violation qualification (Table 28-7) and
  * section 28.2.3. This profile disables MBEC and supervisor shadow stacks:
  * bits 6 and 14 are undefined, not zero requirements. Bits 9:11 are
  * supplemental metadata (undefined outside translated-linear faults), and
  * bits 63:17 are not currently defined. None supplies refill authority.
  * Defined unsupported events 13/15/16 fail closed. IDT vectoring was checked
  * by the caller; bit 12 is defined only without NMI exiting or with virtual
  * NMIs. Read the actual pin controls rather than assuming requested bits. */
 if(!(q&7))return reject_fault(a,gpa,q,pin,NOA_REJECT_ACCESS,false,0,-1);
 if((q&UINT64_C(0x1a000))||((q&0x1000)&&(!(pin&8)||(pin&32))))
  return reject_fault(a,gpa,q,pin,NOA_REJECT_UNSUPPORTED,false,0,-1);
 if((q&0x100)&&!(q&0x80))return reject_fault(a,gpa,q,pin,NOA_REJECT_GLA,false,0,-1);
 if((q&0x80)&&!(q&0x100)&&(q&3)!=3)
  return reject_fault(a,gpa,q,pin,NOA_REJECT_PAGEWALK,false,0,-1);
 if(!((q&7)&~((q>>3)&7)))return reject_fault(a,gpa,q,pin,NOA_REJECT_ALLOWED,false,0,-1);
 if(!private_permissions(a,gpa,&actual,&leaf))
  return reject_fault(a,gpa,q,pin,NOA_REJECT_PRIVATE_TREE,false,0,-1);
 if(actual!=((q>>3)&7))return reject_fault(a,gpa,q,pin,NOA_REJECT_PERMISSIONS,true,actual,-1);
 struct nested_ept_result result;
 enum tr_atomic_status status=tr_atomic_fault(&a->atomic,a->sparse->contexts.active,gpa,(uint8_t)(q&7),&result);
 if(status!=TR_ATOMIC_FILLED&&status!=TR_ATOMIC_PROMOTED)
  return reject_fault(a,gpa,q,pin,NOA_REJECT_BRIDGE,true,actual,status);
 a->sparse->misses++;
 if(status==TR_ATOMIC_FILLED)a->fills++;
 else{
  a->promotions++;
  if((gpa&~UINT64_C(4095))==(a->target_gpa?a->target_gpa:UINT64_C(0xf000))){
   unsigned key=a->sparse->selected12==0x1005e?0:a->sparse->selected12==0x1405e?1:2;
   a->target_promotions[key]++;
  }
 }
 if((q&0x80)&&!(q&0x100))a->paging_faults++;
 return true;
}
bool nested_owner_atomic_entry(struct nested_owner_atomic *a,uint64_t vmcs,uint64_t eptp,bool l2){
 if(!nested_owner_atomic_stopped(a)||!a->sparse->initialized||a->sparse->destroyed||
    vmcs!=(l2?a->vcpu->vmcs02:a->vcpu->vmcs01))return false;
 struct nested_owner_sparse *s=a->sparse;
 if(l2){
  if(!s->hardware_item||s->hardware_item!=s->contexts.active||s->selected12!=s->hardware_item->key||
     !tr_atomic_entry(&a->atomic,s->hardware_item,eptp))return false;
  a->entries++;
 }else{
  if(eptp!=s->fixture->eptp||(eptp&4095)!=0x1e||!tr_atomic_ready(&a->atomic))return false;
  a->ready_entries++;
 }
 return true;
}
/* Stopped owner only. Resolve every tree HPA through actual live pool slots,
 * never a freed page or unchecked physical pointer; preserve typed admission. */
static bool item_permissions(struct nested_owner_atomic *a,struct nested_ept_context_item *item,uint64_t gpa,uint8_t *permissions,uint64_t *out){
 if(!out||!permissions||!nested_owner_atomic_stopped(a)||!item)return false;
 struct nested_owner_sparse *s=a->sparse;bool member=false;
 for(struct nested_ept_context_item *i=s->contexts.items;i;i=i->next)if(i==item)member=true;
 if(!member)return false;
 uint64_t h=item->tree.eptp&ADDRESS;uint8_t effective=7;
 for(unsigned level=0;level<4;level++){
  const uint64_t *table=0;
  for(size_t n=0;n<s->pool.count;n++)if(s->pool.slots[n].live&&s->pool.pages[n].hpa==h){table=s->pool.pages[n].view;break;}
  if(!table)return false;
  unsigned shift=39-level*9;
  uint64_t entry=__atomic_load_n(&table[(gpa>>shift)&511],__ATOMIC_ACQUIRE);
  effective&=entry&7;
  if(!effective){*permissions=0;*out=0;return true;}
  if(level<3&&(entry&0x80))return false;
  if(level==3){*permissions=effective;*out=entry;return true;}
  h=entry&ADDRESS;
 }
 return false;
}
static bool private_permissions(struct nested_owner_atomic *a,uint64_t gpa,uint8_t *permissions,uint64_t *out){
 return a&&a->sparse&&item_permissions(a,a->sparse->contexts.active,gpa,permissions,out);
}
bool nested_owner_atomic_item_leaf(struct nested_owner_atomic *a,struct nested_ept_context_item *item,uint64_t gpa,uint64_t *out){
 uint8_t permission=0;return item_permissions(a,item,gpa,&permission,out)&&permission;
}
bool nested_owner_atomic_leaf(struct nested_owner_atomic *a,uint64_t gpa,uint64_t *out){
 uint8_t permission=0;
 return private_permissions(a,gpa,&permission,out)&&permission;
}
