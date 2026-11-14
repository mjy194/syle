#include "linux_host_clock.h"
static bool source_ok(const struct lhc_source*s){
 return s&&s->verified&&s->invariant&&s->epoch&&s->hz>=UINT64_C(1000000)&&s->hz<=UINT64_C(10000000000);
}
static bool valid(const struct lhc_clock*c){
 return c&&c->ready&&source_ok(&c->source)&&c->tick<=LHC_LIMIT&&c->fraction<c->source.hz&&c->max_gap;
}
static bool sample_ok(const struct lhc_clock*c,struct lhc_sample s){
 return valid(c)&&s.cpu==c->source.cpu&&s.epoch==c->source.epoch&&s.tsc>=c->last_tsc;
}
bool lhc_init(struct lhc_clock*c,const struct lhc_source*s,struct lhc_sample a,uint64_t gap){
 if(!c||!source_ok(s)||!gap||a.cpu!=s->cpu||a.epoch!=s->epoch)return false;
 *c=(struct lhc_clock){.source=*s,.last_tsc=a.tsc,.max_gap=gap,.ready=true};return true;
}
bool lhc_start(struct lhc_clock*c,struct lhc_sample s){
 if(!sample_ok(c,s)||c->running)return false;
 /* Paused interval is deliberately excluded; preserve fractional tick. */
 c->last_tsc=s.tsc;c->running=true;return true;
}
bool lhc_advance(struct lhc_clock*c,struct lhc_sample s){
 if(!sample_ok(c,s)||!c->running)return false;
 uint64_t delta=s.tsc-c->last_tsc;if(delta>c->max_gap)return false;
 uint64_t hz=c->source.hz,seconds=delta/hz,remainder=delta%hz;
 /* remainder*LHC_HZ+fraction <= 1.194e16 with the bounded source rate.
  * No __udivti3 dependency or per-sample rounding drift in kernel code. */
 uint64_t units=remainder*LHC_HZ+c->fraction;
 uint64_t tail=units/hz;
 if(seconds>(LHC_LIMIT-c->tick)/LHC_HZ)return false;
 uint64_t whole=seconds*LHC_HZ;
 if(tail>LHC_LIMIT-c->tick-whole)return false;
 c->tick+=whole+tail;c->fraction=units%hz;c->last_tsc=s.tsc;return true;
}
bool lhc_stop(struct lhc_clock*c,struct lhc_sample s){
 if(!lhc_advance(c,s))return false;
 c->running=false;return true;
}
