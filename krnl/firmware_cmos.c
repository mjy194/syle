#include "firmware_cmos.h"
bool fcr_init(struct fcr_state*s,const struct fwl_plan*layout,uint64_t epoch,uint64_t reference){
 if(!s||s->ready||s->failed||!epoch)return false;
 struct fcmi_state memory={0};if(!fcmi_init(&memory,layout))return false;
 *s=(struct fcr_state){.memory=memory,.origin=reference,.last=reference,.epoch=epoch,.ready=true};lrtc_init(&s->rtc);return true;
}
bool fcr_io(struct fcr_state*s,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,uint64_t epoch,uint64_t reference,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||!s->ready||s->failed)return false;
 if(((q>>16)&0xffff)==0x71 && !(q&8) &&
    (!g||s->rtc.index!=15||(uint8_t)g[0]!=0)){s->failed=true;return false;}
 if(epoch!=s->epoch||reference<s->last||reference<s->origin){s->failed=true;return false;}
 uint64_t elapsed=reference-s->origin,seconds=elapsed/10000000,remainder=elapsed%10000000;
 if(seconds>=UINT64_C(3155760000)){s->failed=true;return false;}
 uint64_t tick=seconds*UINT64_C(1193182)+remainder*UINT64_C(1193182)/10000000;
 if(!lrtc_io(&s->rtc,g,rip,cpl,reason,q,len,tick,ctx,write)){s->failed=true;return false;}
 s->last=reference;return true;
}
