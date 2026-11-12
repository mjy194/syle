#include "pic0.h"
static bool mask_ok(uint8_t m){return m==255||m==254||m==251||m==250;}
static bool valid(const struct pic0*s){
 if(!s||!s->domain||s->phase>3||(!mask_ok(s->imr)&&!(s->mask_pending&&s->imr==0))||s->completed>s->serial||s->reserved>s->serial||s->reserved_seen>s->seen)return false;
 if(s->phase&&(s->initialized||s->aeoi||s->irr||s->isr||s->reserved||s->imr!=0||!s->mask_pending))return false;
 if(s->mask_pending&&s->imr!=0)return false;
 if(!s->reserved&&s->reserved_seen)return false;
 if(!s->initialized&&!s->phase&&(s->vector||s->mask_pending))return false;
 if(s->phase==1&&s->vector)return false;
 if((s->initialized||s->phase>1)&&(s->vector<32||(s->vector&7)))return false;
 if(!s->initialized&&(s->irr||s->isr||s->reserved||s->aeoi))return false;
 if(s->reserved&&(!s->irr||s->isr||(s->imr&1)||s->reserved<=s->completed))return false;
 return true;
}
bool pic0_init(struct pic0*s,uint64_t d,uint64_t initial){if(!s||!d)return false;*s=(struct pic0){.domain=d,.seen=initial,.imr=255};return true;}
bool pic0_edges(struct pic0*s,uint64_t d,uint64_t n){
 if(!valid(s)||s->domain!=d||n<s->seen)return false;
 /* ICW reset/in-progress suppresses capture; retain source watermark so these
  * deliberately reset edges cannot be replayed as future interrupts. */
 if(n>s->seen&&s->initialized)s->irr=true;
 s->seen=n;return true;
}
bool pic0_access(struct pic0*s,uint16_t port,bool write,uint8_t*v){
 if(!valid(s)||!v||s->reserved||(port!=0x20&&port!=0x21))return false;
 struct pic0 n=*s;uint8_t b=*v;
 if(!write)b=port==0x21?n.imr:(n.read_isr?(n.isr?1:0):(n.irr?1:0));
 else if(port==0x20){
  if(b==0x11){n.phase=1;n.initialized=n.aeoi=n.read_isr=n.irr=n.isr=false;n.vector=0;n.imr=0;n.mask_pending=true;n.completed=0;}
  else if(n.phase)return false;
  else if(b==0x0a||b==0x0b)n.read_isr=b==0x0b;
  else if((b==0x20||b==0x60)&&n.initialized)n.isr=false;
  else return false;
 }else{
  if(n.phase==1){if(b<32||(b&7))return false;n.vector=b;n.phase=2;}
  else if(n.phase==2){if(b!=4)return false;n.phase=3;}
  else if(n.phase==3){if(b!=1&&b!=3)return false;n.phase=0;n.initialized=true;n.aeoi=b==3;}
  else {if(!mask_ok(b)||(!n.initialized&&b!=255&&b!=251))return false;n.imr=b;n.mask_pending=false;}
 }
 if(!valid(&n))return false;
 *s=n;*v=b;return true;
}
bool pic0_peek(const struct pic0*s,uint8_t*v){if(!valid(s)||!v||!s->initialized||s->mask_pending||s->phase||s->reserved||!s->irr||s->isr||(s->imr&1))return false;*v=s->vector;return true;}
bool pic0_reserve(struct pic0*s,uint64_t*t,uint8_t*v){
 uint8_t vector;if(!t||!v||!pic0_peek(s,&vector)||s->serial==UINT64_MAX)return false;
 s->reserved=++s->serial;s->reserved_seen=s->seen;*t=s->reserved;*v=vector;return true;
}
bool pic0_accept(struct pic0*s,uint64_t d,uint64_t t,uint8_t v){
 if(!valid(s)||d!=s->domain||!t||v!=s->vector)return false;
 if(!s->reserved&&t==s->completed)return true;
 if(t!=s->reserved)return false;
 s->irr=s->seen>s->reserved_seen;s->isr=!s->aeoi;s->completed=t;s->reserved=0;s->reserved_seen=0;return true;
}
bool pic0_cancel(struct pic0*s,uint64_t d,uint64_t t,uint8_t v){
 if(!valid(s)||d!=s->domain||!t||t!=s->reserved||v!=s->vector)return false;
 s->reserved=0;s->reserved_seen=0;return true;
}
