#include "firmware_timer_irq_import.h"
static bool fresh(const struct fti_owner*s){
 return s&&!s->domain&&!s->origin&&!s->last&&!s->max_gap&&!s->deadline_bus&&
 !s->expirations&&!s->generation&&!s->token&&!s->reserved_expirations&&
 !s->ready&&!s->pending&&!s->reserved&&!s->in_service&&!s->terminal;
}
static bool fail(struct fti_import_gate*g){if(g)g->failed=true;return false;}
bool fti_import_once(struct fti_import_gate*g,struct fti_owner*out,const struct ft100*s,
 uint64_t domain,uint64_t epoch,uint64_t reference,bool quiet){
 if(!g)return false;
 if(g->consumed||g->failed||g->source||g->domain||g->epoch)return fail(g);
 if(!quiet||!fresh(out)||!s||!domain||!epoch||s->epoch!=epoch||!s->ready||
 s->terminal||!s->armed||s->initial!=10000000||s->divisor!=1||
 s->lvt!=0x20020||s->period_bus!=FTI_PERIOD_BUS||!s->max_gap||
 s->last<s->origin||s->last-s->origin>UINT64_MAX/10||
 !s->deadline_bus)return fail(g);
 /* The start timestamp need not equal origin; preserve any representable
  * absolute phase. A running unmasked source cannot have consumed a due
  * event, so its last successful sample must still precede the deadline. */
 if((s->last-s->origin)*10>=s->deadline_bus)return fail(g);
 struct fti_owner n={.domain=domain,.origin=s->origin,.last=s->last,
 .max_gap=s->max_gap,.deadline_bus=s->deadline_bus,.ready=true};
 if(!fti_sample(&n,domain,reference))return fail(g);
 *out=n;g->domain=domain;g->epoch=epoch;g->source=s;g->consumed=true;return true;
}
