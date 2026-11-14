#include "firmware_timer_irq_owner.h"
static bool live(const struct fti_owner*s){return s&&s->ready&&!s->terminal;}
static bool fail(struct fti_owner*s){if(s)s->terminal=true;return false;}
bool fti_init(struct fti_owner*s,uint64_t d,uint64_t r,uint64_t gap){
 if(!s||s->ready||s->terminal||!d||!gap)return false;
 *s=(struct fti_owner){.domain=d,.origin=r,.last=r,.max_gap=gap,
 .deadline_bus=FTI_PERIOD_BUS,.ready=true};return true;
}
bool fti_sample(struct fti_owner*s,uint64_t d,uint64_t r){
 if(!live(s))return false;
 if(d!=s->domain||r<s->last||r-s->last>s->max_gap||r<s->origin||
    r-s->origin>UINT64_MAX/10)return fail(s);
 uint64_t elapsed=(r-s->origin)*10;
 if(elapsed>=s->deadline_bus){
  uint64_t k=(elapsed-s->deadline_bus)/FTI_PERIOD_BUS+1;
  if(k>(UINT64_MAX-s->deadline_bus)/FTI_PERIOD_BUS||
     k>UINT64_MAX-s->expirations)return fail(s);
  s->deadline_bus+=k*FTI_PERIOD_BUS;s->expirations+=k;s->pending=true;
 }
 s->last=r;return true;
}
bool fti_reserve(struct fti_owner*s,uint64_t*t,uint8_t*v){
 if(!live(s)||!t||!v||!s->pending||s->reserved||s->in_service)return false;
 if(s->generation==UINT64_MAX)return fail(s);
 s->token=++s->generation;s->reserved_expirations=s->expirations;
 s->reserved=true;*t=s->token;*v=FTI_VECTOR;return true;
}
static bool owned(struct fti_owner*s,uint64_t t,bool proof){
 if(!live(s))return false;
 if(!proof||!s->reserved||!t||t!=s->token||!s->pending||s->in_service)
  return fail(s);
 return true;
}
bool fti_cancel(struct fti_owner*s,uint64_t t,bool clean){
 if(!owned(s,t,clean))return false;
 s->reserved=false;s->token=0;s->reserved_expirations=0;return true;
}
bool fti_account(struct fti_owner*s,uint64_t t,bool clean){
 if(!owned(s,t,clean))return false;
 s->pending=s->expirations>s->reserved_expirations;
 s->in_service=true;s->reserved=false;s->token=0;s->reserved_expirations=0;
 return true;
}
bool fti_eoi(struct fti_owner*s){
 if(!live(s))return false;
 if(s->reserved)return fail(s);
 s->in_service=false;return true;
}
void fti_quarantine(struct fti_owner*s){(void)fail(s);}
