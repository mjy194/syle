#include "../firmware_timer_irq_owner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct fti_owner fresh(void){struct fti_owner s={0};assert(fti_init(&s,7,0,UINT64_MAX));return s;}
static uint64_t reserve(struct fti_owner*s){uint64_t t=0;uint8_t v=0;assert(fti_reserve(s,&t,&v));assert(v==32&&t);return t;}
int main(void){
 /* 1: fixed phase and multi-period coalescing, exact boundary. */
 struct fti_owner s=fresh();assert(fti_sample(&s,7,999999)&&!s.pending);
 assert(fti_sample(&s,7,1000000)&&s.pending&&s.expirations==1);
 assert(fti_sample(&s,7,4500000)&&s.expirations==4&&s.deadline_bus==50000000);
 uint64_t t=reserve(&s);assert(fti_account(&s,t,true)&&s.in_service&&!s.pending);
 /* 2: post-reservation expiry survives old receipt, ISR blocks selection. */
 s=fresh();assert(fti_sample(&s,7,1000000));t=reserve(&s);
 assert(fti_sample(&s,7,4000000)&&s.expirations==4);
 assert(fti_account(&s,t,true)&&s.pending&&s.in_service);
 uint64_t out=99;uint8_t v=99;assert(!fti_reserve(&s,&out,&v)&&out==99&&v==99);
 assert(fti_eoi(&s));t=reserve(&s);assert(fti_account(&s,t,true)&&!s.pending);
 assert(fti_eoi(&s)&&fti_eoi(&s));
 /* 3: definite no-entry cancel merges expiries, re-reserve distinct token. */
 s=fresh();assert(fti_sample(&s,7,1000000));t=reserve(&s);
 assert(fti_sample(&s,7,3000000));assert(fti_cancel(&s,t,true)&&s.pending&&!s.in_service);
 uint64_t next=reserve(&s);assert(next!=t);assert(fti_account(&s,next,true)&&!s.pending);
 /* 4: missing receipt/wrong token/double account are fatal, no erase. */
 for(unsigned k=0;k<3;k++){s=fresh();assert(fti_sample(&s,7,1000000));t=reserve(&s);
  if(k==0)assert(!fti_account(&s,t,false));
  if(k==1)assert(!fti_cancel(&s,t+1,true));
  if(k==2){assert(fti_account(&s,t,true));assert(!fti_account(&s,t,true));}
  assert(s.terminal);assert(!fti_sample(&s,7,2000000));assert(!fti_init(&s,7,0,1));
  if(k!=2)assert(s.pending&&s.reserved);
 }
 /* 5: time domain/backwards/gap/elapsed overflow preserve ownership. */
 for(unsigned k=0;k<4;k++){s=fresh();assert(fti_sample(&s,7,1000000));t=reserve(&s);
  uint64_t d=7,r=2000000;if(k==0)d=8;if(k==1)r=999999;
  if(k==2)s.max_gap=1;
  if(k==3)r=UINT64_MAX;
  assert(!fti_sample(&s,d,r)&&s.terminal&&s.pending&&s.reserved&&s.token==t&&s.last==1000000);
 }
 /* 6: checked deadline/expiry/token overflow; injection never attempted. */
 s=fresh();s.deadline_bus=UINT64_MAX-10;s.origin=0;
 assert(!fti_sample(&s,7,UINT64_MAX/10)&&s.terminal);
 s=fresh();s.expirations=UINT64_MAX;assert(!fti_sample(&s,7,1000000)&&s.terminal);
 s=fresh();assert(fti_sample(&s,7,1000000));s.generation=UINT64_MAX;
 assert(!fti_reserve(&s,&out,&v)&&s.terminal&&s.pending);
 /* 7: exclusivity/EOI while reserved and quarantine latch. */
 s=fresh();assert(fti_sample(&s,7,1000000));t=reserve(&s);
 struct fti_owner old=s;assert(!fti_reserve(&s,&out,&v)&&!memcmp(&s,&old,sizeof s));
 assert(!fti_eoi(&s)&&s.terminal&&s.reserved);fti_quarantine(&s);
 assert(!fti_cancel(&s,t,true));
 /* 8: null/fresh/init/nonpending and same-time samples do not invent events. */
 s=(struct fti_owner){0};assert(!fti_sample(&s,7,0));assert(!fti_init(&s,0,0,1));
 assert(!fti_init(&s,7,0,0));assert(!fti_init(0,7,0,1));s=fresh();old=s;
 assert(!fti_reserve(&s,&out,&v)&&!memcmp(&s,&old,sizeof s));
 assert(fti_sample(&s,7,0)&&!s.pending);assert(!fti_account(0,1,true));
 assert(!fti_eoi(0));assert(!fti_sample(0,7,0));fti_quarantine(0);
 puts("firmware_timer_irq_owner: PASS (host-only; no entry receipt proof)");return 0;
}
