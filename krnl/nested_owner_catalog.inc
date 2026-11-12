/* N40B opt-in adapter. No hardcoded key, root GPA or leaf GPA in this file. */
#include "nested_owner_dynamic_typed.h"
#include "paging.h"
#include "vmx_memory.h"
static bool same_id(struct nrc_identity a,struct nrc_identity b){
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;
}
static bool same_root(struct nrc_root a,struct nrc_root b){
 return same_id(a.identity,b.identity)&&a.source==b.source&&a.owner==b.owner&&
  a.generation==b.generation&&a.lifetime==b.lifetime&&a.full_eptp==b.full_eptp&&a.root_gpa==b.root_gpa;
}
static bool owner(struct nested_owner_dynamic_typed *t){
 if(!t||t->failed||!t->owner||!t->bridge)return false;
 unsigned a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
 struct nested_owner_sparse *s=t->owner;
 return s->pool.active&&!s->pool.fault&&s->pool.owner==(b>>24)&&
  s->initialized&&!s->destroyed&&s->fixture&&s->multiple&&s->evict&&
  s->contexts.initialized&&!s->contexts.poisoned&&s->contexts.max_resident==2&&
  s->contexts.evict&&!s->contexts.hardware_ad&&!s->contexts.mixed_ad;
}
static bool live(struct nested_owner_dynamic_typed *t){
 return owner(t)&&t->environment_guard&&t->environment_guard(t->guard_context)&&
  t->catalog&&t->catalog==t->bound_catalog&&same_id(t->catalog_identity,t->catalog->identity);
}
static bool fail(struct nested_owner_dynamic_typed *t){
 if(t){t->failed=true;if(t->fatal)t->fatal(t->fatal_context);}return false;
}
bool nested_owner_catalog_key(struct nested_owner_dynamic_typed *t,uint64_t key){
 struct nrc_root root;
 return t&&!t->failed&&t->catalog&&(!t->bound||t->catalog==t->bound_catalog)&&
  nrc_find(t->catalog,t->catalog_identity,key,&root);
}
bool nested_owner_catalog_selected(struct nested_owner_dynamic_typed *t,uint64_t key){
 struct nrc_root root;
 if(!live(t)||!nrc_find(t->bound_catalog,t->catalog_identity,key,&root))return false;
 t->selected_source=root;return true;
}
bool nested_owner_catalog_entry(struct nested_owner_dynamic_typed *t){
 struct nrc_root root;
 return t&&t->bound&&live(t)&&t->owner->contexts.active&&
  t->owner->contexts.active->key==t->owner->selected12&&
  nrc_find(t->bound_catalog,t->catalog_identity,t->owner->selected12,&root)&&
  same_root(root,t->selected_source);
}
bool nested_owner_catalog_authority(void *context,const struct nrc_span *span,uint64_t gpa,struct nrc_page *out){
 struct nested_owner_dynamic_typed *t=context;
 if(!live(t)||!span||!out||!t->walk.type_epoch||
    t->walk.virtual_key!=t->source_session.root.full_eptp||
    (t->walk.type!=TRL_WB&&t->walk.type!=TRL_UC)||
    !same_id(span->identity,t->catalog_identity)||gpa<span->gpa||gpa-span->gpa>=span->bytes)return false;
 /* This fixture provider's execution map has one 4K leaf table. The reusable
  * catalog itself has no 63-page or GPA range restriction. These immutable
  * owner records were made only after the complete unique-allocation and
  * all-alias audit. No release/remap is permitted during this catalog lifetime. */
 struct ept_fixture *f=t->owner->fixture;
 for(unsigned l=0;l<4;l++){
  if(!f->tables[l]||!vmx_memory_region_wb(f->tables[l]))return false;
  const uint64_t *p=paging_phys_to_virt(f->tables[l]);
  if(!p||((uintptr_t)p&4095)||(l<3&&p[0]!=(f->tables[l+1]|7)))return false;
 }
 if((gpa>>12)>=512)return false;
 uint64_t hpa=span->hpa+(gpa-span->gpa);
 bool target=hpa==t->walk.backing_hpa;unsigned type=target?t->walk.type:TRL_WB;
 const uint64_t *pt=paging_phys_to_virt(f->tables[3]);
 if(pt[gpa>>12]!=(hpa|0x40|((uint64_t)type<<3)|span->permissions)||
    (target&&(span->role!=NRC_DATA||(span->permissions&4)))||
    (!target&&!vmx_memory_region_wb(hpa)))return false;
 const uint64_t *view=span->role==NRC_TABLE?paging_phys_to_virt(hpa):NULL;
 *out=(struct nrc_page){span->object,span->lifetime,hpa,view,span->permissions,(uint8_t)type,true};return true;
}
static bool source_guard(void *context,const struct tr_registry_walk *w){
 struct nested_owner_dynamic_typed *t=context;
 if(!live(t)||!w||w->virtual_key!=t->selected_source.full_eptp||
    !same_root(t->source_session.root,t->selected_source)||!nrc_guard(&t->source_session,t->source_nonce))return false;
 if(!t->walk.type_epoch)t->walk=*w;
 return t->walk.virtual_key==w->virtual_key&&t->walk.type_epoch==w->type_epoch&&
  t->walk.backing_hpa==w->backing_hpa&&t->walk.type==w->type;
}
static struct nested_ept_result source_translate(void *context,const struct tr_registry_walk *w,uint64_t gpa,uint8_t access){
 struct nested_owner_dynamic_typed *t=context;
 if(!source_guard(t,w))return (struct nested_ept_result){.status=NESTED_EPT_L0_BACKING_DENIED};
 return nrc_walk(&t->source_session,t->source_nonce,gpa,access);
}
/* Boot-time negatives execute the same authority and admitted miss path.
 * They do not edit source tables, allocate a cache leaf, or reflect a fault. */
bool nested_owner_catalog_probe(struct nested_owner_dynamic_typed *t,uint64_t denied_l2,uint64_t unowned_l1){
 if(!t||!nested_owner_catalog_entry(t)||t->catalog_checks||t->source_session.live)return false;
 struct nrc_root bad=t->selected_source;uint64_t nonce=0;
 bad.identity.domain++;
 if(nrc_begin(t->catalog,bad,t,&t->source_session,&nonce))return false;
 bad=t->selected_source;bad.generation++;
 if(nrc_begin(t->catalog,bad,t,&t->source_session,&nonce))return false;
 if(!nrc_begin(t->catalog,t->selected_source,t,&t->source_session,&t->source_nonce))return false;
 t->walk=(struct tr_registry_walk){0};
 const struct tr_registry_source source={source_guard,source_translate};
 struct nested_ept_result denied;
 enum nested_sparse_fill status=typed_root_registry_miss_source(t->bridge,t->owner->contexts.active,
  denied_l2,2,&source,t,&denied);
 struct nrc_page missing;
 bool rejected=!nrc_resolve(&t->source_session,t->source_nonce,unowned_l1,1,&missing);
 bool ended=nrc_end(&t->source_session,t->source_nonce);
 if(!ended||!rejected||status!=NESTED_SPARSE_INVALID||denied.status!=NESTED_EPT_L0_BACKING_DENIED)return false;
 t->catalog_checks=15;return true;
}
bool nested_owner_dynamic_typed_bind(struct nested_owner_dynamic_typed *t,struct nested_owner_sparse *s){
 if(!t||!s||t->bound||t->failed||!t->bridge||!t->environment_guard||!t->fatal||
    !t->catalog||t->bound_catalog||!nested_owner_catalog_key(t,t->initial_key))return false;
 t->owner=s;t->bound_catalog=t->catalog;
 if(!live(t)||!s->contexts.active||s->contexts.count!=1||
    s->contexts.active->key!=t->initial_key||s->selected12!=t->initial_key||
    s->contexts.transactions!=typed_root_registry_transactions()||s->contexts.transaction_context!=t->bridge||
    !t->bridge->metadata_lock||!t->bridge->metadata_unlock||!s->contexts.active->tree.typed_admit||
    !nested_owner_catalog_selected(t,s->selected12))return fail(t);
 t->bound=true;return true;
}
bool nested_owner_dynamic_typed_miss(struct nested_owner_dynamic_typed *t,uint64_t gpa,uint8_t access){
 if(!t||!nested_owner_catalog_entry(t)||!access||(access&~7)||t->misses==UINT64_MAX||t->source_session.live)return fail(t);
 if(!nrc_begin(t->bound_catalog,t->selected_source,t,&t->source_session,&t->source_nonce))return fail(t);
 t->walk=(struct tr_registry_walk){0};
 const struct tr_registry_source source={source_guard,source_translate};
 enum nested_sparse_fill status=typed_root_registry_miss_source(t->bridge,t->owner->contexts.active,gpa,access,
  &source,t,&t->last_translation);
 bool ended=nrc_end(&t->source_session,t->source_nonce);
 if(!ended||status!=NESTED_SPARSE_OK)return fail(t);
 t->misses++;return true;
}
