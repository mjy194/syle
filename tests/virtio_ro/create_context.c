#include "create_context.h"
static struct vtc_entry *find(struct vtc *c,uint64_t pa){if(!c||!c->initialized||c->ended||!pa)return 0;for(unsigned i=0;i<c->count;i++)if(c->entries[i].pa==pa&&c->entries[i].live)return &c->entries[i];return 0;}
bool vtc_open(struct vtc *c,struct vtc_entry *entries,unsigned capacity){
 if(!c||c->initialized||c->entries||c->count||c->capacity||c->epoch||c->ended||c->failed||c->draining||!entries||!capacity)return false;
 for(unsigned i=0;i<capacity;i++)if(entries[i].pa||entries[i].live||entries[i].pinned||entries[i].quarantined||entries[i].kind||entries[i].allocation.generation||entries[i].allocation.issuer||entries[i].allocation.slot||entries[i].lease.serial||entries[i].lease.issuer||entries[i].lease.slot)return false;
 uint64_t epoch;if(!pna_start(&epoch))return false;
 c->entries=entries;c->capacity=capacity;c->count=0;c->epoch=epoch;c->initialized=true;return true;
}
bool vtc_allocate(struct vtc *c,enum vlm_kind kind,uint64_t *out){
 if(!c||!c->initialized||c->ended||c->failed||c->draining||!out||c->count==c->capacity||kind==VLM_UNSPECIFIED)return false;
 struct vlm_allocation a;if(!pna_alloc_typed(c->epoch,kind,&a))return false;
 struct vtc_entry *e=&c->entries[c->count];*e=(struct vtc_entry){.allocation=a,.kind=kind,.live=true};
 uint64_t pa;if(!pna_page(c->epoch,a,&pa)){e->quarantined=true;c->count++;c->failed=true;return false;}
 e->pa=pa;
 /* A retired PA remains a tombstone for this CREATE. A PA-only caller cannot
  * distinguish old/new generations, so never expose it twice in one context. */
 for(unsigned i=0;i<c->count;i++)if(c->entries[i].pa==pa){
  if(!pna_release(c->epoch,a)){e->quarantined=true;c->count++;c->failed=true;}
  else *e=(struct vtc_entry){0};
  return false;
 }
 c->count++;*out=pa;return true;
}
bool vtc_pin_ram(struct vtc *c,uint64_t pa){
 struct vtc_entry *e=find(c,pa);enum vlm_kind kind;uint64_t actual;
 if(!e||c->failed||c->draining||e->pinned||e->quarantined||!pna_kind(c->epoch,e->allocation,&kind)||kind!=VLM_GUEST_RAM||kind!=e->kind||!pna_page(c->epoch,e->allocation,&actual)||actual!=pa)return false;
 if(!pna_pin(c->epoch,e->allocation,&e->lease))return false;
 e->pinned=true;return true;
}
bool vtc_unpin_ram(struct vtc *c,uint64_t pa){struct vtc_entry *e=find(c,pa);if(!e||!e->pinned||!pna_unpin(c->epoch,e->lease))return false;e->pinned=false;return true;}
bool vtc_release(struct vtc *c,uint64_t pa){struct vtc_entry *e=find(c,pa);if(!e||c->failed||e->pinned||!pna_release(c->epoch,e->allocation))return false;e->live=false;return true;}
bool vtc_close(struct vtc *c){
 if(!c||!c->initialized||c->ended||!pna_drain(c->epoch))return false;
 c->draining=true;bool ok=true;
 for(unsigned i=0;i<c->count;i++){struct vtc_entry *e=&c->entries[i];if(!e->live)continue;if(e->pinned||!pna_release(c->epoch,e->allocation)){ok=false;continue;}e->live=false;}
 if(!ok||!pna_end(c->epoch))return false;
 c->ended=true;return true;
}
