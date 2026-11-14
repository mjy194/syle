#include "scoped_access.h"
#include <limits.h>
static bool valid(struct vsc *s){return s&&s->initialized&&!s->ended&&s->authority&&s->authority->begun&&!s->authority->owned.ended&&s->epoch==s->authority->owned.epoch;}
static struct vtc_entry *entry(struct vsc *s,struct vlm_allocation a){
 if(!valid(s))return 0;
 for(unsigned i=0;i<s->authority->owned.count;i++){
  struct vtc_entry *e=&s->authority->owned.entries[i];
  if(e->allocation.issuer==a.issuer&&e->allocation.slot==a.slot&&e->allocation.generation==a.generation&&e->live&&!e->quarantined)return e;
 }
 return 0;
}
bool vsc_init(struct vsc *s,struct vsa *a,const struct vsc_ops *o){
 if(!s||s->initialized||s->authority||s->epoch||s->borrowers||s->busy||s->closing||s->faulted||s->ended||!a||!a->begun||a->owned.ended||a->owned.count||!o||!o->host_buffer||!o->transfer)return false;
 s->authority=a;s->ops=*o;s->epoch=a->owned.epoch;s->initialized=true;return true;
}
bool vsc_allocate(struct vsc *s,enum sg_memory_role role,struct vlm_allocation *out){
 if(!valid(s)||s->busy||s->closing||s->faulted||!out)return false;
 uint64_t pa=vsa_allocate(s->authority,role);if(!pa)return false;
 struct vtc_entry *e=&s->authority->owned.entries[s->authority->owned.count-1];
 if(!pna_pin(s->epoch,e->allocation,&e->lease)){s->faulted=true;return false;}
 e->pinned=true;*out=e->allocation;return true;
}
bool vsc_copy(struct vsc *s,struct vlm_allocation a,enum vlm_kind expected,size_t offset,void *buffer,size_t n,bool to_owned){
 struct vtc_entry *e=entry(s,a);uint64_t pa;enum vlm_kind kind;
 if(!e||s->busy||s->closing||s->faulted||!e->pinned||!n||offset>4096||n>4096-offset||s->borrowers==UINT_MAX||
 !pna_page(s->epoch,a,&pa)||pa!=e->pa||!pna_kind(s->epoch,a,&kind)||kind!=expected||kind!=e->kind)return false;
 /* trusted synchronous callbacks, no general asynchronous/NMI synchronization */
 s->busy=true;s->borrowers++;
 bool admitted=s->ops.host_buffer(s->ops.context,buffer,n);
 bool ok=admitted&&s->ops.transfer(s->ops.context,pa,offset,buffer,n,to_owned);
 if(admitted&&!ok)s->faulted=true; /* partial/uncertain copy must not be retried */
 s->borrowers--;s->busy=false;return ok;
}
bool vsc_drain(struct vsc *s){if(!valid(s))return false;s->closing=true;return !s->borrowers&&!s->busy;}
bool vsc_retire_after_quiescence(struct vsc *s){
 if(!valid(s)||!s->closing||s->borrowers||s->busy)return false;
 for(unsigned i=0;i<s->authority->owned.count;i++){
  struct vtc_entry *e=&s->authority->owned.entries[i];
  if(e->live&&e->pinned){if(!pna_unpin(s->epoch,e->lease))return false;e->pinned=false;}
 }
 if(!vsa_end(s->authority))return false;
 s->ended=true;return true;
}
