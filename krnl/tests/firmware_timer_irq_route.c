#include "../firmware_timer_irq_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct callback {unsigned calls;struct ftirt_route*reenter;bool bad;};
static enum ft100_result old_sample(void*p,struct ft100*s,uint64_t e,uint64_t r){
 struct callback*c=p;c->calls++;
 if(c->reenter)assert(!ftirt_sample(c->reenter,r,c,old_sample));
 return c->bad?FT100_BAD:ft100_sample(s,e,r,0);
}
static struct ft100 source(void){struct ft100 s={0};assert(ft100_init(&s,8,100,UINT64_MAX));assert(ft100_start(&s,8,123,10000000,1,0x20020)==FT100_OK);return s;}
int main(void){
 struct fti_owner owner={0};struct ft100 s=source();struct ftirt_route r={0};struct callback c={0};
 assert(ftirt_bind(&r,&s,&owner,7,8));assert(ftirt_sample(&r,456,&c,old_sample)&&c.calls==1);
 assert(ftirt_transfer(&r,3000123,true));assert(r.retired&&r.owner->expirations==3);
 struct ft100 retired=s;
 for(unsigned i=0;i<5;i++)assert(ftirt_sample(&r,4000123+i*1000000,&c,old_sample));
 assert(c.calls==1&&!memcmp(&s,&retired,sizeof s)&&r.owner->expirations==8);
 assert(!ftirt_legacy_sample(&r,9000123,&c,old_sample)&&c.calls==1&&r.failed);
 assert(!ftirt_sample(&r,9000123,&c,old_sample));
 /* Fresh-transfer path never calls legacy, even at a due timestamp. */
 s=source();owner=(struct fti_owner){0};r=(struct ftirt_route){0};c=(struct callback){0};assert(ftirt_bind(&r,&s,&owner,7,8));
 assert(ftirt_transfer(&r,1000123,true)&&r.owner->pending);assert(ftirt_sample(&r,2000123,&c,old_sample)&&c.calls==0);
 assert(!ftirt_transfer(&r,2000123,true)&&r.failed&&c.calls==0);
 /* Failed transfer never publishes new owner or touches source. */
 for(unsigned k=0;k<5;k++){s=source();owner=(struct fti_owner){0};r=(struct ftirt_route){0};assert(ftirt_bind(&r,&s,&owner,7,8));
  if(k==0)s.terminal=true;
  if(k==1)s.divisor=2;
  struct ft100 saved=s;uint64_t ref=k==2?UINT64_MAX:k==3?122:456;
  assert(!ftirt_transfer(&r,ref,k!=4)&&r.failed&&!r.retired&&!r.import.consumed&&!r.owner->ready);
  assert(!memcmp(&s,&saved,sizeof s));
 }
 /* Source bypass is detected, not magically prevented; no legacy callback. */
 s=source();owner=(struct fti_owner){0};r=(struct ftirt_route){0};assert(ftirt_bind(&r,&s,&owner,7,8));assert(ftirt_transfer(&r,456,true));
 s.last++;c=(struct callback){0};assert(!ftirt_sample(&r,457,&c,old_sample)&&!c.calls&&r.failed);
 /* Legacy callback failure/reentry is permanently latched, no later transfer. */
 for(unsigned k=0;k<2;k++){s=source();owner=(struct fti_owner){0};r=(struct ftirt_route){0};assert(ftirt_bind(&r,&s,&owner,7,8));
  c=(struct callback){.bad=k==0,.reenter=k?&r:0};
  assert(!ftirt_sample(&r,456,&c,old_sample)&&c.calls==1&&r.failed);
  assert(!ftirt_transfer(&r,456,true));
 }
 assert(!ftirt_sample(0,0,0,0));assert(!ftirt_transfer(0,0,true));
 puts("firmware_timer_irq_route: PASS (host-only; bypass prevention requires call-site wiring)");
}
