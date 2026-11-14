#include "firmware_timer_irq_session_import.h"
static bool owner_fresh(const struct fti_owner*s){
 return !s->domain&&!s->origin&&!s->last&&!s->max_gap&&!s->deadline_bus&&
 !s->expirations&&!s->generation&&!s->token&&!s->reserved_expirations&&
 !s->ready&&!s->pending&&!s->reserved&&!s->in_service&&!s->terminal;
}
static bool session_fresh(const struct ftis_session*s){
 return s&&!s->ready&&!s->failed&&!s->busy&&!s->domain&&!s->vmcs&&
 !s->attempts&&!s->active_attempt&&owner_fresh(&s->owner)&&
 s->event.phase==LEI_IDLE&&!s->event.token&&!s->event.vector&&
 !s->event.armed&&!s->event.cancellable&&!s->event.cleaned;
}
static bool route_fresh(const struct ftirt_route*r){
 return r&&!r->source&&!r->owner&&!r->ready&&!r->retired&&!r->failed&&!r->busy&&
 !r->domain&&!r->epoch&&!r->import.consumed&&!r->import.failed&&
 !r->import.domain&&!r->import.epoch&&!r->import.source;
}
static bool fail(struct ftis_session*s,struct ftirt_route*r){
 if(s){s->failed=true;fti_quarantine(&s->owner);}
 if(r)r->failed=true;
 return false;
}
bool ftis_import(struct ftis_session*s,struct ftirt_route*r,struct ft100*source,
 uint64_t domain,uint64_t vmcs,uint64_t epoch,uint64_t reference,bool safe){
 if(!session_fresh(s)||!route_fresh(r)||!vmcs||!safe)return fail(s,r);
 if(!ftirt_bind(r,source,&s->owner,domain,epoch)||!ftirt_transfer(r,reference,safe))return fail(s,r);
 /* This is the only persistent owner; session publication comes last. */
 s->domain=domain;s->vmcs=vmcs;s->ready=true;return true;
}
