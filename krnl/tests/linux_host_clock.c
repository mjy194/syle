#include "../linux_host_clock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct lhc_sample sample(uint64_t t){return (struct lhc_sample){t,1,3};}
static struct lhc_clock fresh(uint64_t hz){
 struct lhc_clock c={0};struct lhc_source s={hz,1,3,true,true};
 assert(lhc_init(&c,&s,sample(0),UINT64_MAX));assert(lhc_start(&c,sample(0)));return c;
}
static void rejects(struct lhc_clock*c,struct lhc_sample s){struct lhc_clock old=*c;assert(!lhc_advance(c,s));assert(!memcmp(c,&old,sizeof old));}
int main(void){
 struct lhc_clock c=fresh(3000000000),direct=c;
 assert(lhc_advance(&c,sample(1)));assert(c.tick==0); /* no invented +1 */
 for(uint64_t t=1001;t<3000000000;t+=100003)assert(lhc_advance(&c,sample(t)));
 assert(lhc_advance(&c,sample(3000000000)));assert(c.tick==LHC_HZ&&!c.fraction);
 assert(lhc_advance(&direct,sample(3000000000)));assert(c.tick==direct.tick&&c.fraction==direct.fraction);
 assert(lhc_stop(&c,sample(3000000001)));uint64_t f=c.fraction,t=c.tick;
 assert(lhc_start(&c,sample(900000000000)));assert(c.tick==t&&c.fraction==f);
 assert(lhc_advance(&c,sample(903000000000)));assert(c.tick==t+LHC_HZ&&c.fraction==f);
 rejects(&c,sample(0));struct lhc_sample bad=sample(c.last_tsc);bad.cpu=4;rejects(&c,bad);bad=sample(c.last_tsc);bad.epoch=2;rejects(&c,bad);
 c.max_gap=5;rejects(&c,sample(c.last_tsc+6));assert(lhc_advance(&c,sample(c.last_tsc+5)));
 c=fresh(1000000);c.tick=LHC_LIMIT;rejects(&c,sample(1));
 c=fresh(10000000000);assert(lhc_advance(&c,sample(UINT64_C(3600)*10000000000)));assert(c.tick==LHC_HZ*3600);
 c=fresh(1000000);rejects(&c,sample(UINT64_MAX));
 struct lhc_source s={0,1,3,true,true};assert(!lhc_init(&c,&s,sample(0),1));s.hz=3000000000;s.verified=false;assert(!lhc_init(&c,&s,sample(0),1));s.verified=true;s.invariant=false;assert(!lhc_init(&c,&s,sample(0),1));
 c=fresh(3000000000);c.tick=LHC_LIMIT;assert(lhc_advance(&c,sample(1))&&c.tick==LHC_LIMIT);
 struct lhc_clock old=c;assert(!lhc_start(&c,sample(2))&&!memcmp(&old,&c,sizeof c));
 assert(!lhc_stop(&c,sample(0))&&!memcmp(&old,&c,sizeof c));
 for(unsigned j=0;j<3;j++){s=(struct lhc_source){j==0?999999:j==1?10000000001ULL:3000000000ULL,j==2?0:1,3,true,true};old=c;assert(!lhc_init(&c,&s,sample(1),1)&&!memcmp(&c,&old,sizeof c));}
 puts("host clock conversion/fraction/pause/source/gap/overflow PASS");return 0;
}
