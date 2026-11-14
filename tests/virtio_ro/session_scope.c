#include "session_scope.h"
static bool valid(struct css*s,struct css_handle h){return s&&s->initialized&&h.issuer==s&&h.epoch==s->epoch;}
bool css_init(struct css*s,uint64_t epoch,const struct css_ops*o,struct css_handle*out){
 if(!s||!out||!epoch||!o||!o->source||!o->transfer||s->initialized||s->epoch||s->busy||s->borrows||s->closing||s->faulted||s->pinned)return false;
 s->epoch=epoch;s->ops=*o;s->initialized=true;*out=(struct css_handle){s,epoch};return true;
}
bool css_close(struct css*s,struct css_handle h){
 if(!valid(s,h))return false;
 s->closing=true;return !s->busy&&!s->borrows&&!s->pinned;
}
bool css_write(struct css*s,struct css_handle h,struct vlm_allocation a,enum vlm_kind expected,size_t off,const void*source,size_t n){
 uint64_t pa;enum vlm_kind kind;
 if(!valid(s,h)||s->closing||s->faulted||s->busy||s->borrows||s->pinned||!source||!n||off>4096||n>4096-off)return false;
 /* Entire authority/source/transfer sequence is serialized in this host
  * profile. Actual marker-zero/native exclusion is NOT inferred. */
 s->busy=true;s->borrows=1;
 if(!pna_page(s->epoch,a,&pa)||!pna_kind(s->epoch,a,&kind)||kind!=expected||!pna_pin(s->epoch,a,&s->lease)){
  s->borrows=0;s->busy=false;return false;
 }
 s->pinned=true;
 bool admitted=s->ops.source(s->ops.ctx,source,n);
 bool ok=admitted&&s->ops.transfer(s->ops.ctx,pa,off,source,n);
 if(admitted&&!ok)s->faulted=true;
 if(!pna_unpin(s->epoch,s->lease)){s->faulted=true;return false;}
 s->pinned=false;s->borrows=0;s->busy=false;return ok;
}
