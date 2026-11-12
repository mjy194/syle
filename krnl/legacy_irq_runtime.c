#include "legacy_irq_runtime.h"
static bool fail(struct lir*s){s->failed=true;s->busy=false;return false;}
bool lir_init(struct lir*s,uint64_t d,uint64_t cap){if(!s||!d||((uint32_t)cap&4)||!((cap>>32)&4))return false;*s=(struct lir){.must_one=(uint32_t)cap,.allowed_one=(uint32_t)(cap>>32)};return pt0_init(&s->timer,d)&&pgg_init(&s->irq,&s->timer.pic);}
bool lir_before(struct lir*s,const struct lei_snapshot*g,void*c,gw_read r,gw_write w){
 if(!s||s->failed||s->busy||s->inflight)return false;
 s->busy=true;
 if(!g||!r||!w||s->timer.failed||s->irq.pic!=&s->timer.pic)return fail(s);
 /* Remove any stale request before staging. Failure forbids this entry. */
 if(!gw_window(c,r,w,s->must_one,s->allowed_one,false))return fail(s);
 enum pgg_before_result q=pgg_before(&s->irq,g,c,w);
 if(q==PGG_ERROR)return fail(s);
 if(q==PGG_DEFER_WINDOW&&!gw_window(c,r,w,s->must_one,s->allowed_one,true))return fail(s);
 s->inflight=true;s->busy=false;return true;
}
bool lir_after(struct lir*s,enum pgg_outcome o,uint32_t reason,uint32_t vectoring,void*c,gw_write w){
 if(!s||s->failed||s->busy||!s->inflight)return false;
 s->busy=true;s->inflight=false;
 if(!pgg_after(&s->irq,o,reason,vectoring,c,w)||o==PGG_UNKNOWN)return fail(s);
 s->busy=false;return true;
}
