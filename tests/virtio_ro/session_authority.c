#include "session_authority.h"
bool vsa_begin(void *ctx){
 struct vsa *s=ctx;
 if(!s||s->begun||!s->storage||!s->capacity||s->capacity>pna_capacity())return false;
 if(!vtc_open(&s->owned,s->storage,s->capacity))return false;
 s->begun=true;return true;
}
bool vsa_end(void *ctx){
 struct vsa *s=ctx;
 if(!s)return false;
 if(!s->begun)return true; /* begin failed before taking authority ownership */
 if(s->owned.ended)return true;
 return vtc_close(&s->owned);
}
uint64_t vsa_allocate(void *ctx,enum sg_memory_role role){
 struct vsa *s=ctx;enum vlm_kind kind;
 if(!s||!s->begun)return 0;
 switch(role){
 case SG_MEMORY_RAM:kind=VLM_GUEST_RAM;break;
 case SG_MEMORY_EPT:kind=VLM_EPT_TABLE;break;
 case SG_MEMORY_VMXON:case SG_MEMORY_VMCS:kind=VLM_VMX_CONTROL;break;
 default:return 0;
 }
 uint64_t pa;return vtc_allocate(&s->owned,kind,&pa)?pa:0;
}
bool vsa_query(struct vsa *s,uint64_t pa,enum vlm_kind *out){
 if(!s||!s->begun||s->owned.ended||!pa||!out)return false;
 for(unsigned i=0;i<s->owned.count;i++){
  struct vtc_entry *e=&s->owned.entries[i];uint64_t actual;enum vlm_kind kind;
  if(e->pa!=pa||!e->live||e->quarantined)continue;
  if(!pna_page(s->owned.epoch,e->allocation,&actual)||actual!=pa||
   !pna_kind(s->owned.epoch,e->allocation,&kind)||kind!=e->kind)return false;
  *out=kind;return true;
 }
 return false;
}
bool vsa_release(void *ctx,uint64_t pa){struct vsa *s=ctx;enum vlm_kind kind;
 if(!vsa_query(s,pa,&kind))return false;
 /* Unknown/quarantined allocations must be resolved before freeing any
  * possibly aliased published PA. Persistent lookup uncertainty or an alias
  * remains fail-closed; do not invoke end early to break this barrier. */
 for(unsigned i=0;i<s->owned.count;i++){
  struct vtc_entry *q=&s->owned.entries[i];uint64_t qpa;
  if(q->live&&q->quarantined&&
   (!pna_page(s->owned.epoch,q->allocation,&qpa)||qpa==pa))return false;
 }
 /* A later allocation may have latched failed with an unpublished token.
  * That forbids allocation, not cleanup of earlier validated published pages.
  * Preserve pinned/quarantined records; close handles unpublished tokens only
  * after the session has completed its real VMX/RAM release sequence. */
 for(unsigned i=0;i<s->owned.count;i++){
  struct vtc_entry *e=&s->owned.entries[i];
  if(e->pa!=pa||!e->live)continue;
  if(e->pinned||e->quarantined||!pna_release(s->owned.epoch,e->allocation))return false;
  e->live=false;return true;
 }
 return false;
}
