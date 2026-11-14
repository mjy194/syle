#include "linux_host_clock_runtime.h"
static bool sample(const struct lhcr_source*s,struct lhc_sample*v){return s&&s->sample&&s->sample(s->ctx,v);}
bool lhcr_init(struct lhc_clock*c,const struct lhcr_source*s){struct lhc_sample v;return sample(s,&v)&&lhc_init(c,&s->properties,v,s->max_gap);}
bool lhcr_start(struct lhc_clock*c,const struct lhcr_source*s){
 if(!c||!c->ready)return false;
 if(c->running)return true; /* STEP must not re-anchor. */
 struct lhc_sample v;return sample(s,&v)&&lhc_start(c,v);
}
bool lhcr_stage(const struct lhc_clock*c,const struct lhcr_source*s,struct lhc_clock*out){
 if(!c||!out)return false;
 struct lhc_sample v;struct lhc_clock n=*c;
 if(!sample(s,&v)||!lhc_advance(&n,v))return false;
 *out=n;return true;
}
bool lhcr_sync(struct lhc_clock*c,struct lpd_domain*d,const struct lhcr_source*s,bool stop){
 if(!c||!d||!c->ready||!lpd_valid(d)||d->master.reserved.serial||d->timer.tick!=c->tick)return false;
 if(!c->running)return true;
 struct lhc_clock n;struct lpd_domain domain=*d;lpd_bind(&domain);
 if(!lhcr_stage(c,s,&n)||!lpd_tick(&domain,n.tick))return false;
 if(stop)n.running=false;
 *c=n;*d=domain;lpd_bind(d);return true;
}
