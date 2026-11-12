#include "../linux_vcpu_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct mock { struct lvs_image live; unsigned calls,fail,entries,writes,loads; };
static bool step(struct mock*m){return ++m->calls!=m->fail;}
static unsigned slot(uint32_t id){for(unsigned i=0;i<LVS_MSR_COUNT;i++)if(id==lvs_msr_ids[i])return i;assert(0);return 0;}
static bool save(void*p,uint8_t*x){struct mock*m=p;memcpy(x,m->live.fx,512);return step(m);}
static bool load(void*p,const uint8_t*x){struct mock*m=p;m->loads++;memcpy(m->live.fx,x,512);return step(m);}
static bool rd(void*p,uint32_t id,uint64_t*v){struct mock*m=p;*v=m->live.msr[slot(id)];return step(m);}
static bool wr(void*p,uint32_t id,uint64_t v){struct mock*m=p;m->writes++;m->live.msr[slot(id)]=v;return step(m);}
static bool enter(void*p){struct mock*m=p;m->entries++;if(!step(m))return false;
 for(unsigned i=0;i<512;i++)m->live.fx[i]=(uint8_t)(i*3+7);
 for(unsigned i=0;i<LVS_MSR_COUNT;i++)m->live.msr[i]=0xabc000+i;
 return true;}
static struct lvs_ops ops(struct mock*m){return (struct lvs_ops){m,save,load,rd,wr,enter};}
static void init(struct mock*m,struct lvs_state*s){memset(m,0,sizeof *m);memset(s,0,sizeof *s);
 for(unsigned i=0;i<512;i++)m->live.fx[i]=(uint8_t)(i+1);
 for(unsigned i=0;i<LVS_MSR_COUNT;i++)m->live.msr[i]=0x123000+i;
 assert(lvs_init(s));assert(!lvs_init(s));
 assert(s->guest.fx[0]==0x7f&&s->guest.fx[1]==3&&s->guest.fx[24]==0x80&&s->guest.fx[25]==0x1f);
}
int main(void){
 for(unsigned f=0;f<=29;f++){
  struct mock m;struct lvs_state s;init(&m,&s);struct lvs_image host=m.live;
  struct lvs_ops o=ops(&m);m.fail=f;enum lvs_result r=lvs_run(&s,&o);
  if(f&&f<=7){assert(r==LVS_CAPTURE_FAILED&&m.entries==0&&!s.host_dirty&&s.guest_valid);}
  else if(f<=15&&f){assert(r==LVS_ENTRY_FAILED&&!s.host_dirty&&s.guest_valid);}
  else if(f<=22&&f){assert(r==LVS_GUEST_CAPTURE_FAILED&&!s.host_dirty&&!s.guest_valid);
   unsigned calls=m.calls;assert(lvs_run(&s,&o)==LVS_BAD_STATE&&m.calls==calls);}
  else if(f){assert(r==LVS_HOST_DIRTY&&s.host_dirty&&s.guest_valid);assert(m.calls==29&&m.writes==12&&m.loads==2);
   unsigned calls=m.calls;assert(lvs_run(&s,&o)==LVS_BAD_STATE&&m.calls==calls);
   struct lvs_ops wrong=o;wrong.ctx=&s;assert(!lvs_restore_host(&s,&wrong)&&m.calls==calls);
   m.fail=0;assert(lvs_restore_host(&s,&o)&&!s.host_dirty&&m.calls==calls+7);
  }else{assert(r==LVS_EXIT&&s.guest_valid&&!s.host_dirty);assert(s.guest.msr[5]==0xabc005);
   assert(lvs_run(&s,&o)==LVS_EXIT&&m.entries==2);}
  assert(!memcmp(&m.live,&host,sizeof host));
 }
 struct mock m;struct lvs_state s;init(&m,&s);struct lvs_ops o=ops(&m);
 m.fail=23;assert(lvs_run(&s,&o)==LVS_HOST_DIRTY);struct lvs_image snapshot=s.host;
 m.fail=m.calls+2;assert(!lvs_restore_host(&s,&o)&&s.host_dirty);
 assert(!memcmp(&snapshot,&s.host,sizeof snapshot));m.fail=0;assert(lvs_restore_host(&s,&o));
 assert(!lvs_init(NULL));assert(lvs_run(NULL,&o)==LVS_BAD_STATE);assert(!lvs_restore_host(NULL,&o));
 o.enter=NULL;assert(lvs_run(&s,&o)==LVS_BAD_STATE);
 puts("linux_vcpu_state: 29 fault points, retries, identity, repeat PASS");
}
