#include "firmware_timer_irq_route.h"
static bool fail(struct ftirt_route*r){if(r){r->failed=true;r->busy=false;fti_quarantine(r->owner);}return false;}
static bool live(struct ftirt_route*r){
 if(!r||!r->ready||r->failed||r->busy||!r->source||!r->owner)return fail(r);
 return true;
}
static bool same(const struct ft100*a,const struct ft100*b){
 return a->epoch==b->epoch&&a->origin==b->origin&&a->last==b->last&&
 a->deadline_bus==b->deadline_bus&&a->period_bus==b->period_bus&&
 a->max_gap==b->max_gap&&a->initial==b->initial&&a->lvt==b->lvt&&
 a->divisor==b->divisor&&a->ready==b->ready&&a->armed==b->armed&&a->terminal==b->terminal;
}
bool ftirt_bind(struct ftirt_route*r,struct ft100*s,struct fti_owner*owner,uint64_t d,uint64_t e){
 if(!r||r->ready||r->failed||!s||!owner||!d||!e||!s->ready||s->terminal||s->epoch!=e)return false;
 *r=(struct ftirt_route){.source=s,.owner=owner,.domain=d,.epoch=e,.ready=true};return true;
}
bool ftirt_transfer(struct ftirt_route*r,uint64_t ref,bool safe){
 if(!live(r))return false;
 if(r->retired)return fail(r);
 struct fti_import_gate g={0};struct fti_owner o=*r->owner;
 struct ft100 snapshot=*r->source;
 if(!fti_import_once(&g,&o,r->source,r->domain,r->epoch,ref,safe)||
    !same(r->source,&snapshot))return fail(r);
 /* No callbacks/intermediate externally visible publication. */
 r->retired_snapshot=snapshot;*r->owner=o;r->import=g;r->retired=true;return true;
}
bool ftirt_legacy_sample(struct ftirt_route*r,uint64_t ref,void*c,ftirt_legacy f){
 if(!live(r))return false;
 if(r->retired||!f||r->source->epoch!=r->epoch||r->source->terminal)return fail(r);
 r->busy=true;
 enum ft100_result result=f(c,r->source,r->epoch,ref);
 if(r->failed||result!=FT100_OK)return fail(r);
 r->busy=false;return true;
}
bool ftirt_sample(struct ftirt_route*r,uint64_t ref,void*c,ftirt_legacy f){
 if(!live(r))return false;
 if(!r->retired)return ftirt_legacy_sample(r,ref,c,f);
 if(!same(r->source,&r->retired_snapshot)||!fti_sample(r->owner,r->domain,ref))return fail(r);
 return true;
}
