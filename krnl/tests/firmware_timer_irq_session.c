#include "../firmware_timer_irq_session.h"
#include <assert.h>
#include <stdio.h>
struct writes {unsigned calls,fail;uint32_t fields[8];uint64_t values[8];struct ftis_session*reenter;};
static bool wr(void*p,uint32_t f,uint64_t v){struct writes*w=p;unsigned n=++w->calls;
 assert(n<=8);w->fields[n-1]=f;w->values[n-1]=v;
 if(w->reenter)assert(!ftis_sample(w->reenter,7,0x1234,1000000));
 return n!=w->fail;
}
static struct lei_snapshot ready={.rflags=0x202};
static struct ftis_session fresh(void){struct ftis_session s={0};assert(ftis_init(&s,7,0x1234,0,UINT64_MAX));assert(ftis_sample(&s,7,0x1234,1000000));return s;}
static uint64_t stage(struct ftis_session*s,struct writes*w){uint64_t a=0;assert(ftis_before(s,7,0x1234,&ready,w,wr,&a)==FTIS_STAGED);assert(a);return a;}
int main(void){
 struct ftis_session s=fresh();struct writes w={0};uint64_t a=stage(&s,&w);
 assert(w.calls==3&&w.fields[2]==0x4016&&w.values[2]==0x80000020);
 assert(ftis_sample(&s,7,0x1234,3000000));
 assert(ftis_after(&s,7,0x1234,a,false,true,31,0,&w,wr)==FTIS_ACCOUNTED);
 assert(w.calls==6&&s.owner.pending&&s.owner.in_service&&!s.active_attempt);
 assert(ftis_eoi(&s,7,0x1234));w=(struct writes){0};a=stage(&s,&w);
 assert(ftis_after(&s,7,0x1234,a,false,true,30,0,&w,wr)==FTIS_ACCOUNTED&&!s.owner.pending);
 assert(ftis_eoi(&s,7,0x1234)&&ftis_eoi(&s,7,0x1234));
 /* Each stage and cleanup write failure: never account, terminal. */
 for(unsigned n=1;n<=6;n++){s=fresh();w=(struct writes){.fail=n};a=0;
  enum ftis_result r=ftis_before(&s,7,0x1234,&ready,&w,wr,&a);
  if(n<=3)assert(r==FTIS_FATAL&&a==0);
  else {assert(r==FTIS_STAGED);assert(ftis_after(&s,7,0x1234,a,false,true,31,0,&w,wr)==FTIS_FATAL);}
  assert(w.calls==n&&s.failed&&s.owner.terminal&&s.owner.pending&&!s.owner.in_service);
 }
 /* Verified no-entry and exact entry-failed outcomes cancel only after cleanup. */
 for(unsigned k=0;k<3;k++){s=fresh();w=(struct writes){0};a=stage(&s,&w);
  assert(ftis_after(&s,7,0x1234,a,k==0,k!=0,k==0?0:0x80000020+k,0,&w,wr)==FTIS_STOP_NO_ENTRY);
  assert(w.calls==6&&s.failed&&s.owner.pending&&!s.owner.reserved&&!s.owner.in_service);
 }
 for(unsigned n=4;n<=6;n++){s=fresh();w=(struct writes){.fail=n};a=stage(&s,&w);
  assert(ftis_after(&s,7,0x1234,a,true,false,0,0,&w,wr)==FTIS_FATAL&&s.owner.reserved&&w.calls==n);
 }
 /* Whitelist unchanged: EPT, timer, vectoring, contradictions, unknown stop. */
 const uint32_t reasons[]={48,52,31,31,0x80000021,999};
 for(unsigned k=0;k<6;k++){s=fresh();w=(struct writes){0};a=stage(&s,&w);
  assert(ftis_after(&s,7,0x1234,a,k==3,true,reasons[k],k==2?0x80000020:k==4?1:0,&w,wr)==FTIS_FATAL);
  assert(w.calls==3&&s.owner.pending&&!s.owner.in_service);
 }
 /* Identity, wrong/repeated attempt, evidence cannot be a cleaned bool. */
 for(unsigned k=0;k<4;k++){s=fresh();w=(struct writes){0};a=stage(&s,&w);
  if(k==3)assert(ftis_after(&s,7,0x1234,a,false,true,31,0,&w,wr)==FTIS_ACCOUNTED);
  unsigned calls=w.calls;
  assert(ftis_after(&s,k==0?8:7,k==1?0x5678:0x1234,k==2?a+1:a,false,true,31,0,&w,wr)==FTIS_FATAL);
  assert(w.calls==calls);
 }
 /* Readiness/defer no reserve, EOI forbidden unresolved, callback reentrancy. */
 s=fresh();w=(struct writes){0};struct lei_snapshot blocked=ready;blocked.rflags=2;a=99;
 assert(ftis_before(&s,7,0x1234,&blocked,&w,wr,&a)==FTIS_DEFER&&a==99&&!w.calls&&!s.owner.reserved);
 a=stage(&s,&w);assert(!ftis_eoi(&s,7,0x1234)&&s.failed);
 s=fresh();w=(struct writes){.reenter=&s};a=99;
 assert(ftis_before(&s,7,0x1234,&ready,&w,wr,&a)==FTIS_FATAL&&w.calls==1&&a==99);
 s=fresh();w=(struct writes){0};s.attempts=UINT64_MAX;a=99;
 assert(ftis_before(&s,7,0x1234,&ready,&w,wr,&a)==FTIS_FATAL&&!w.calls);
 s=fresh();assert(!ftis_sample(&s,8,0x1234,1000001)&&s.failed);
 assert(!ftis_init(&s,7,0x1234,0,1));
 puts("firmware_timer_irq_session: PASS (host only; external native evidence required)");
}
