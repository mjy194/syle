#include "lease_model.h"
bool vlm_init(struct vlm *s,struct vlm_ops ops){
 if(!s||s->initialized||s->serial||s->failed||s->draining||!ops.allocate||!ops.release)return false;
 for(unsigned i=0;i<VLM_SLOTS;i++)if(s->slot[i].pa||s->slot[i].owner||s->slot[i].generation||s->slot[i].lease||s->slot[i].kind)return false;
 /* Fresh static storage is already zero. Do not emit a freestanding memset
  * by assigning the entire production-sized registry aggregate. */
 s->ops=ops;s->initialized=true;return true;
}
static bool match(const struct vlm *s,uint64_t owner,struct vlm_allocation a){return s&&s->initialized&&!s->failed&&owner&&a.issuer==s&&a.slot<VLM_SLOTS&&a.generation&&s->slot[a.slot].pa&&s->slot[a.slot].owner==owner&&s->slot[a.slot].generation==a.generation;}
bool vlm_allocate_typed(struct vlm *s,uint64_t owner,enum vlm_kind kind,struct vlm_allocation *out){
 if(!s||!s->initialized||s->failed||s->draining||!owner||!out||kind<VLM_UNSPECIFIED||kind>VLM_HOST_METADATA||s->serial==UINT64_MAX)return false;
 unsigned free=VLM_SLOTS;for(unsigned i=0;i<VLM_SLOTS;i++)if(!s->slot[i].pa){free=i;break;}
 if(free==VLM_SLOTS)return false;
 uint64_t pa=s->ops.allocate(s->ops.ctx);if(!pa)return false;
 if(pa&~UINT64_C(0x000ffffffffff000)){s->failed=true;return false;}
 for(unsigned i=0;i<VLM_SLOTS;i++)if(s->slot[i].pa==pa){s->failed=true;return false;}
 s->slot[free]=(struct vlm_slot){pa,owner,++s->serial,0,kind};*out=(struct vlm_allocation){free,s->serial,s};return true;
}
bool vlm_allocation_page(const struct vlm *s,uint64_t owner,struct vlm_allocation a,uint64_t *pa){if(!pa||!match(s,owner,a))return false;*pa=s->slot[a.slot].pa;return true;}
bool vlm_pin(struct vlm *s,uint64_t owner,struct vlm_allocation a,struct vlm_handle *out){
 if(!match(s,owner,a)||s->draining||!out||s->serial==UINT64_MAX||s->slot[a.slot].lease)return false;
 s->slot[a.slot].lease=++s->serial;*out=(struct vlm_handle){a.slot,s->serial,s};return true;
}
bool vlm_unpin(struct vlm *s,struct vlm_handle h){
 if(!s||!s->initialized||s->failed||h.issuer!=s||h.slot>=VLM_SLOTS||!h.serial||s->slot[h.slot].lease!=h.serial)return false;
 s->slot[h.slot].lease=0;return true;
}
bool vlm_release(struct vlm *s,uint64_t owner,struct vlm_allocation a){
 if(!match(s,owner,a)||s->slot[a.slot].lease)return false;
 if(!s->ops.release(s->ops.ctx,s->slot[a.slot].pa))return false;
 s->slot[a.slot]=(struct vlm_slot){0};return true;
}
void vlm_begin_drain(struct vlm *s){if(s&&s->initialized)s->draining=true;}
bool vlm_drained(const struct vlm *s){if(!s||!s->initialized||s->failed||!s->draining)return false;for(unsigned i=0;i<VLM_SLOTS;i++)if(s->slot[i].lease)return false;return true;}

bool vlm_allocate(struct vlm *s,uint64_t owner,struct vlm_allocation *a){return vlm_allocate_typed(s,owner,VLM_UNSPECIFIED,a);}
bool vlm_kind_of(const struct vlm *s,uint64_t owner,struct vlm_allocation a,enum vlm_kind *kind){if(!kind||!match(s,owner,a))return false;*kind=s->slot[a.slot].kind;return true;}
