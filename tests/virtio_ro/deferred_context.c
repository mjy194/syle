#include "deferred_context.h"
static struct vdc_entry *find(struct vdc *c,uint64_t pa){if(!c||!c->opened||c->ended||!pa)return 0;for(unsigned i=0;i<c->count;i++)if(c->entries[i].present&&c->entries[i].pa==pa)return &c->entries[i];return 0;}
bool vdc_open(struct vdc *c,struct vdc_entry *entries,unsigned budget){
 if(!c||c->opened||c->entries||c->count||c->budget||c->epoch||c->draining||c->ended||c->failed||!entries||!budget||budget>pna_capacity())return false;
 for(unsigned i=0;i<budget;i++)if(entries[i].pa||entries[i].present||entries[i].retired||entries[i].pinned||entries[i].allocation.generation||entries[i].allocation.issuer||entries[i].allocation.slot||entries[i].lease.serial||entries[i].lease.issuer||entries[i].lease.slot||entries[i].kind)return false;
 uint64_t epoch;if(!pna_start(&epoch))return false;c->entries=entries;c->budget=budget;c->epoch=epoch;c->opened=true;return true;
}
bool vdc_allocate(struct vdc *c,enum vlm_kind kind,uint64_t *out){
 if(!c||!c->opened||c->ended||c->draining||c->failed||!out||c->count==c->budget||kind==VLM_UNSPECIFIED)return false;
 struct vlm_allocation a;if(!pna_alloc_typed(c->epoch,kind,&a))return false;
 struct vdc_entry *e=&c->entries[c->count++];*e=(struct vdc_entry){.allocation=a,.kind=kind,.present=true};
 uint64_t pa;if(!pna_page(c->epoch,a,&pa)){c->failed=true;return false;}
 e->pa=pa;
 for(unsigned i=0;i+1<c->count;i++)if(c->entries[i].pa==pa){c->failed=true;return false;}
 *out=pa;return true;
}
bool vdc_pin_ram(struct vdc *c,uint64_t pa){struct vdc_entry *e=find(c,pa);enum vlm_kind kind;uint64_t actual;if(!e||e->retired||e->pinned||c->draining||c->failed||!pna_kind(c->epoch,e->allocation,&kind)||kind!=VLM_GUEST_RAM||kind!=e->kind||!pna_page(c->epoch,e->allocation,&actual)||actual!=pa||!pna_pin(c->epoch,e->allocation,&e->lease))return false;e->pinned=true;return true;}
bool vdc_unpin(struct vdc *c,uint64_t pa){struct vdc_entry *e=find(c,pa);if(!e||!e->pinned||!pna_unpin(c->epoch,e->lease))return false;e->pinned=false;return true;}
bool vdc_retire(struct vdc *c,uint64_t pa){struct vdc_entry *e=find(c,pa);if(!e||c->failed||c->draining||e->retired||e->pinned)return false;e->retired=true;return true;}
bool vdc_finalize(struct vdc *c){
 if(!c||!c->opened||c->ended||!pna_drain(c->epoch))return false;
 c->draining=true;
 bool ok=true;for(unsigned i=0;i<c->count;i++){struct vdc_entry *e=&c->entries[i];if(!e->present)continue;if(e->pinned||!pna_release(c->epoch,e->allocation)){ok=false;continue;}e->present=false;e->retired=true;}
 if(!ok||!pna_end(c->epoch))return false;
 c->ended=true;return true;
}
