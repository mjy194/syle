#include "linux_pic04_runtime.h"
static bool fail(struct lpr_runtime*r){r->failed=true;r->busy=false;return false;
 }
bool lpr_init(struct lpr_runtime*r,const struct pic_master_04*p,uint64_t cap){if(!r||!p||((uint32_t)cap&4)||!((cap>>32)&4))return false;
 *r=(struct lpr_runtime){.must_one=(uint32_t)cap,.allowed_one=(uint32_t)(cap>>32)};if(!pmg_init(&r->irq,p))return false;
 r->ready=true;return true;}
bool lpr_before(struct lpr_runtime*r,struct pic_master_04*p,const struct lei_snapshot*g,void*x,gw_read rd,gw_write wr){
 if(!r||!r->ready||r->failed||r->busy||r->inflight)return false;
 r->busy=true;
 if(!p||!g||!rd||!wr||!gw_window(x,rd,wr,r->must_one,r->allowed_one,false))return fail(r);
 enum pmg_before_result b=pmg_before(&r->irq,p,g,x,wr);
 if(b==PMG_ERROR||(b==PMG_DEFER_WINDOW&&!gw_window(x,rd,wr,r->must_one,r->allowed_one,true)))return fail(r);
 r->inflight=true;r->busy=false;return true;
}
bool lpr_after(struct lpr_runtime*r,struct pic_master_04*p,enum pmg_outcome o,uint32_t reason,uint32_t vector,void*x,gw_write wr){
 if(!r||!r->ready||r->failed||r->busy||!r->inflight)return false;
 r->busy=true;r->inflight=false;
 if(!pmg_after(&r->irq,p,o,reason,vector,x,wr)||o==PMG_UNKNOWN)return fail(r);
 r->busy=false;return true;
}
