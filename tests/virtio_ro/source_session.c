#include "source_session.h"
static bool spans_ok(const struct src_span*s){if(!s)return false;for(unsigned i=0;i<3;i++)if(!s[i].p||!s[i].n)return false;return true;}
bool src_init(struct src_owner*s,const struct src_span spans[3]){
 if(!s||s->initialized||s->generation||s->held||s->retired||!spans_ok(spans))return false;
 for(unsigned i=0;i<3;i++)s->spans[i]=spans[i];
 s->initialized=true;s->generation=1;return true;
}
bool src_acquire(struct src_owner*s,struct src_ref*out){
 if(!s||!out||!s->initialized||s->retired||s->held||s->generation==UINT64_MAX)return false;
 s->generation++;s->held=true;*out=(struct src_ref){s,s->generation};return true;
}
static bool valid(struct src_ref r){return r.issuer&&r.issuer->initialized&&!r.issuer->retired&&r.issuer->held&&r.generation==r.issuer->generation;}
bool src_contains(struct src_ref r,unsigned kind,const void*p,size_t n){
 if(!valid(r)||kind>=3||!p)return false;
 struct src_span s=r.issuer->spans[kind];uintptr_t a=(uintptr_t)p,b=(uintptr_t)s.p;
 return a>=b&&a-b<=s.n&&n<=s.n-(a-b);
}
bool src_release(struct src_ref r){if(!valid(r))return false;r.issuer->held=false;return true;}
bool src_replace(struct src_owner*s,const struct src_span spans[3]){
 if(!s||!s->initialized||s->retired||s->held||s->generation==UINT64_MAX||!spans_ok(spans))return false;
 for(unsigned i=0;i<3;i++)s->spans[i]=spans[i];
 s->generation++;return true;
}
bool src_retire(struct src_owner*s){if(!s||!s->initialized||s->held||s->retired)return false;s->retired=true;return true;}
