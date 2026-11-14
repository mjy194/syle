#include "firmware_pmtimer.h"
bool fpm_init(struct fpm_state*s,uint64_t e,uint64_t r,uint64_t gap){
 if(!s||s->ready||s->failed||!e||!gap)return false;
 *s=(struct fpm_state){.origin=r,.last=r,.epoch=e,.max_gap=gap,.pmba=1,.ready=true};return true;
}
bool fpm_sample(struct fpm_state*s,uint64_t e,uint64_t r){
 if(!s||!s->ready||s->failed)return false;
 if(e!=s->epoch||r<s->last||r<s->origin||r-s->last>s->max_gap){s->failed=true;return false;}
 uint64_t elapsed=r-s->origin;
 /* Since 3579545<10000000, full result <= elapsed and cannot overflow. */
 uint64_t ticks=(elapsed/10000000)*UINT64_C(3579545)+(elapsed%10000000)*UINT64_C(3579545)/10000000;
 if((ticks>>23)!=(s->ticks>>23))s->timer_status=true;
 s->ticks=ticks;s->last=r;return true;
}
bool fpm_config(struct fpm_state*s,unsigned off,unsigned width,bool write,uint32_t*v){
 if(!s||!s->ready||s->failed||!v)return false;
 if(off==0x40&&width==4){
  if(write){if(*v!=1&&*v!=0xb001)return false;if(s->enable&&*v!=s->pmba)return false;s->pmba=*v;}
  else *v=s->pmba;
  return true;
 }
 if(off==0x80&&width==1){
  if(write){if(*v>1||(*v&&s->pmba!=0xb001))return false;s->enable=(uint8_t)*v;}
  else *v=s->enable;
  return true;
 }
 return false;
}
bool fpm_io(struct fpm_state*s,uint64_t e,uint64_t r,unsigned port,unsigned width,bool input,uint32_t*v){
 if(!s||!s->ready||s->failed||!v||!s->enable||s->pmba!=0xb001||port!=0xb008||width!=4||!input)return false;
 if(!fpm_sample(s,e,r))return false;
 *v=(uint32_t)s->ticks&0xffffff;return true;
}
