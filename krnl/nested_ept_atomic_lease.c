#include "nested_ept_atomic_lease.h"
struct bridge {
 struct ad_updater_gate *gate;
 struct nested_ept_atomic_session *session;
 const struct nested_ept_atomic_ops *ops;
 void *context;
};
static bool guard(void *p,struct ad_entry_stamp s) {
 struct bridge *b=p;
 return ad_updater_gate_guard(b->gate,&b->session->token,s,b->session->nonce)&&b->ops->guard(b->context,s);
}
static bool table(void *p,uint64_t g,uint64_t *h,uint64_t **v) {
 struct bridge*b=p;return b->ops->table(b->context,g,h,v);
}
static bool leaf(void *p,uint64_t g,uint64_t *h,uint8_t *v) {
 struct bridge*b=p;return b->ops->leaf(b->context,g,h,v);
}
static uint64_t load(void*p,uint64_t*v) {
 struct bridge*b=p;return b->ops->load(b->context,v);
}
static bool cas(void*p,uint64_t*v,uint64_t a,uint64_t z) {
 struct bridge*b=p;return b->ops->cas(b->context,v,a,z);
}
struct nested_ept_atomic_result nested_ept_atomic_walk_leased(
 struct ad_updater_gate *g,struct nested_ept_atomic_session *s,
 uint64_t eptp,uint64_t gpa,uint8_t need,unsigned width,
 const struct nested_ept_atomic_ops *o,void *ctx,struct ad_entry_stamp stamp,
 struct nested_ept_atomic_trace *trace)
{
 struct nested_ept_atomic_result r={.status=NEA_INVALID};
 if(!g||!s||!o||!o->guard||!o->table||!o->leaf||!!o->load!=!!o->cas||!trace||
    (s->acquired&&!s->released))return r;
 s->acquired=false;s->released=false;
 if(!ad_updater_gate_acquire(g,stamp,&s->token,&s->nonce)){r.status=NEA_STALE;return r;}
 s->acquired=true;
 struct bridge b={g,s,o,ctx};
 struct nested_ept_atomic_ops ops={guard,table,leaf,o->load?load:0,o->cas?cas:0};
 r=nested_ept_atomic_walk(eptp,gpa,need,width,&ops,&b,stamp,trace);
 s->released=ad_updater_gate_release(g,&s->token,stamp,s->nonce);
 if(!s->released){ad_updater_gate_poison(g);r.status=NEA_STALE;}
 return r;
}
