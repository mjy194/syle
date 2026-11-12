#ifdef VMX_NESTED_ROOT_CATALOG_TEST
#include "nested_owner_catalog.inc"
#else
#include "nested_owner_dynamic_typed.h"
#include "paging.h"
#include "vmx_memory.h"
static bool key_valid(uint64_t key) {
 return key==0x1001e||key==0x1401e||key==0x1801e;
}
static bool owner(struct nested_owner_dynamic_typed *t) {
 if(!t||t->failed||!t->owner||!t->bridge)return false;
 unsigned a,b,c,d;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
 struct nested_owner_sparse *s=t->owner;
 return s->pool.active&&!s->pool.fault&&s->pool.owner==(b>>24)&&
  s->initialized&&!s->destroyed&&s->fixture&&s->multiple&&s->evict&&
  s->contexts.initialized&&!s->contexts.poisoned&&s->contexts.max_resident==2&&
  s->contexts.evict&&!s->contexts.hardware_ad&&!s->contexts.mixed_ad;
}
static bool live(struct nested_owner_dynamic_typed *t) {
 return owner(t)&&t->environment_guard&&t->environment_guard(t->guard_context);
}
static bool fail(struct nested_owner_dynamic_typed *t) {
 if(t){t->failed=true;if(t->fatal)t->fatal(t->fatal_context);}
 return false;
}
static bool resolve(void *context,const struct tr_registry_walk *walk,
 uint64_t gpa,uint8_t need,uint64_t *hpa,const uint64_t **view,uint8_t *permissions) {
 (void)need;struct nested_owner_dynamic_typed *t=context;
 if(!live(t)||!walk||!key_valid(walk->virtual_key)||!walk->type_epoch||
    (walk->type!=TRL_UC&&walk->type!=TRL_WB)||!gpa||(gpa&4095)||gpa>=0x40000)return false;
 struct ept_fixture *f=t->owner->fixture;
 /* f000 is the one immutable data backing. It is never exposed as table
  * bytes, including during WB. Every table/ordinary page is verified WB. */
 if(EPT_GUEST_PAGES!=63||f->pages[14]!=walk->backing_hpa)return false;
 for(unsigned l=0;l<4;l++) {
  if(!f->tables[l]||f->tables[l]==walk->backing_hpa||!vmx_memory_region_wb(f->tables[l]))return false;
  const uint64_t *table=paging_phys_to_virt(f->tables[l]);
  if(!table||((uintptr_t)table&4095)||(l<3&&table[0]!=(f->tables[l+1]|7)))return false;
 }
 const uint64_t *pt=paging_phys_to_virt(f->tables[3]);unsigned n=gpa>>12;
 *hpa=f->pages[n-1];*permissions=pt[n]&7;*view=NULL;
 bool target=*hpa==walk->backing_hpa;
 unsigned type=target?walk->type:TRL_WB;
 if(!*hpa||(*hpa&4095)||
    (pt[n]&~UINT64_C(7))!=(*hpa|0x40|((uint64_t)type<<3))||!(*permissions&1))return false;
 if(target)return gpa==0xf000&&!(*permissions&4);
 if(!vmx_memory_region_wb(*hpa))return false;
 *view=paging_phys_to_virt(*hpa);
 return *view&&!((uintptr_t)*view&4095);
}
bool nested_owner_dynamic_typed_bind(struct nested_owner_dynamic_typed *t,struct nested_owner_sparse *s) {
 if(!t||!s||t->bound||t->failed||!t->bridge||!t->environment_guard||!t->fatal)return false;
 t->owner=s;
 if(!live(t)||!s->contexts.active||s->contexts.count!=1||
    s->contexts.active->key!=0x1001e||s->selected12!=0x1001e||
    s->contexts.transactions!=typed_root_registry_transactions()||
    s->contexts.transaction_context!=t->bridge||
    !t->bridge->metadata_lock||!t->bridge->metadata_unlock||
    !s->contexts.active->tree.typed_admit)return fail(t);
 t->bound=true;return true;
}
bool nested_owner_dynamic_typed_miss(struct nested_owner_dynamic_typed *t,uint64_t gpa,uint8_t access) {
 if(!t||!t->bound||!live(t)||!access||(access&~7)||t->misses==UINT64_MAX)return fail(t);
 struct nested_owner_sparse *s=t->owner;struct nested_ept_context_item *item=s->contexts.active;
 if(!item||!key_valid(s->selected12)||item->key!=s->selected12||item->tree.hardware_ad)return fail(t);
 struct nested_ept_policy policy={.maxphys=s->pool.maxphys};
 if(typed_root_registry_miss(t->bridge,item,gpa,access,resolve,t,&policy)!=NESTED_SPARSE_OK)return fail(t);
 t->misses++;return true;
}

#endif
