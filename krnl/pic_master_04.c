#include "pic_master_04.h"
static bool eq(const struct pm04_ticket*a,const struct pm04_ticket*b){return a&&b&&a->domain==b->domain&&a->serial==b->serial&&a->generation==b->generation&&a->source==b->source&&a->vector==b->vector;}
static bool empty(const struct pm04_ticket*t){return !t->domain&&!t->serial&&!t->generation&&!t->source&&!t->vector;}
static uint64_t seen(const struct pic_master_04*s,unsigned n){return n?s->seen4:s->seen0;}
static bool ticket(const struct pic_master_04*s,const struct pm04_ticket*t){
 return t->serial&&t->serial<=s->serial&&t->domain==s->domain&&(t->source==0||t->source==4)&&t->vector==s->vector+t->source&&t->generation<=seen(s,t->source);
}
static bool mask(uint8_t m){return (m&0xea)==0xea;}
static bool valid(const struct pic_master_04*s){
 if(!s||!s->domain||s->phase>3||(s->irr&~17)||(s->isr&~17)||(!mask(s->imr)&&!(s->mask_pending&&s->imr==0)))return false;
 if(s->mask_pending&&s->imr)return false;
 if(s->phase&&(s->initialized||s->aeoi||s->irr||s->isr||s->reserved.serial||s->imr||!s->mask_pending))return false;
 if(!s->initialized&&(s->irr||s->isr||s->reserved.serial||s->aeoi))return false;
 if(!s->initialized&&!s->phase&&(s->vector||s->mask_pending))return false;
 if(s->phase==1&&s->vector)return false;
 if(s->reserved.serial&&((s->isr&1)||(s->reserved.source==4&&s->isr)))return false;
 if((s->initialized||s->phase>1)&&(s->vector<32||(s->vector&7)))return false;
 if(s->reserved.serial){if(!ticket(s,&s->reserved)||!(s->irr&(1u<<s->reserved.source))||(s->imr&(1u<<s->reserved.source))||s->reserved.serial<=s->completed.serial)return false;}
 else if(!empty(&s->reserved))return false;
 if(s->completed.serial){if(!ticket(s,&s->completed))return false;}else if(!empty(&s->completed))return false;
 if(s->quarantined&&!s->reserved.serial)return false;
 return true;
}
bool pm04_init(struct pic_master_04*s,uint64_t d,uint64_t n,bool level){
 if(!s||!d)return false;
 *s=(struct pic_master_04){.domain=d,.seen0=n,.level4=level,.imr=255};return true;
}
bool pm04_edges0(struct pic_master_04*s,uint64_t d,uint64_t n){
 if(!valid(s)||s->quarantined||s->domain!=d||n<s->seen0)return false;
 if(n>s->seen0&&s->initialized)s->irr|=1;
 s->seen0=n;return true;
}
bool pm04_level4(struct pic_master_04*s,uint64_t d,bool level){
 if(!valid(s)||s->quarantined||s->domain!=d)return false;
 if(level&&!s->level4){if(s->seen4==UINT64_MAX)return false;s->seen4++;if(s->initialized)s->irr|=16;}
 s->level4=level;return true;
}
bool pm04_access(struct pic_master_04*s,uint16_t port,bool write,uint8_t*v,bool slave){
 if(!valid(s)||s->quarantined||s->reserved.serial||!v||(port!=0x20&&port!=0x21))return false;
 struct pic_master_04 n=*s;uint8_t b=*v;
 if(!write)b=port==0x21?n.imr:n.read_isr?n.isr:n.irr;
 else if(port==0x20){
  if(b==17){n.phase=1;n.initialized=n.aeoi=n.read_isr=false;n.irr=n.isr=n.vector=n.imr=0;n.mask_pending=true;n.completed=(struct pm04_ticket){0};}
  else if(n.phase)return false;
  else if(b==10||b==11)n.read_isr=b==11;
  else if(n.initialized&&(b==32||b==96||b==100)){uint8_t clear=b==96?1:b==100?16:(n.isr&1)?1:16;n.isr&=~clear;}
  else return false;
 }else{
  if(n.phase==1){if(b<32||(b&7))return false;n.vector=b;n.phase=2;}
  else if(n.phase==2){if(b!=4)return false;n.phase=3;}
  else if(n.phase==3){if(b!=1&&b!=3)return false;n.phase=0;n.initialized=true;n.aeoi=b==3;}
  else {if(!mask(b)||(!(b&4)&&!slave)||(!n.initialized&&b!=255&&b!=251))return false;n.imr=b;n.mask_pending=false;}
 }
 if(!valid(&n))return false;
 *s=n;*v=b;return true;
}
bool pm04_peek(const struct pic_master_04*s,uint8_t*src,uint8_t*vec){
 if(!valid(s)||s->quarantined||!src||!vec||!s->initialized||s->phase||s->mask_pending||s->reserved.serial)return false;
 uint8_t pending=s->irr&~s->imr;unsigned n;
 if((pending&1)&&!(s->isr&1))n=0;
 else if((pending&16)&&!s->isr)n=4;else return false;
 *src=n;*vec=s->vector+n;return true;
}
bool pm04_reserve(struct pic_master_04*s,struct pm04_ticket*t){
 uint8_t n,v;if(!t||!pm04_peek(s,&n,&v)||s->serial==UINT64_MAX)return false;
 s->reserved=(struct pm04_ticket){s->domain,++s->serial,seen(s,n),n,v};*t=s->reserved;return true;
}
bool pm04_accept(struct pic_master_04*s,const struct pm04_ticket*t){
 if(!valid(s)||s->quarantined||!t||!t->serial)return false;
 if(!s->reserved.serial&&eq(t,&s->completed))return true;
 if(!eq(t,&s->reserved))return false;
 uint8_t bit=1u<<t->source;if(seen(s,t->source)==t->generation)s->irr&=~bit;
 if(!s->aeoi)s->isr|=bit;
 s->completed=*t;s->reserved=(struct pm04_ticket){0};return true;
}
bool pm04_cancel(struct pic_master_04*s,const struct pm04_ticket*t){
 if(!valid(s)||s->quarantined||!t||!t->serial||!eq(t,&s->reserved))return false;
 s->reserved=(struct pm04_ticket){0};return true;
}
bool pm04_quarantine(struct pic_master_04*s,const struct pm04_ticket*t){
 if(!valid(s)||!t||!t->serial||!eq(t,&s->reserved))return false;
 s->quarantined=true;return true;
}
