#include "../firmware_timer_100m.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 struct ft100 s={0};assert(ft100_init(&s,1,100,1000000000));
 assert(ft100_start(&s,1,100,0xffffffff,2,0x20005)==FT100_OK);
 /* Actual observed initial value gives 85.8993459 seconds, not BASE_TICKS. */
 assert(s.deadline_bus==UINT64_C(8589934590));
 assert(ft100_entry_before(&s,100+858993458));
 assert(!ft100_entry_before(&s,100+858993459));
 uint32_t c=0;assert(ft100_sample(&s,1,100+858993458,&c)==FT100_OK&&c==5);
 uint64_t due=s.deadline_bus;assert(ft100_sample(&s,1,100+858993459,&c)==FT100_STOP&&c==5&&s.deadline_bus==due&&s.armed);
 assert(!ft100_init(&s,1,0,1));assert(ft100_start(&s,1,100,0,2,0x10000)==FT100_BAD);
 struct ft100 p={0};assert(ft100_init(&p,2,0,100));
 assert(ft100_start(&p,2,0,3,1,0x30005)==FT100_OK);
 assert(!ft100_entry_before(&p,1)); /* first expiry is 30ns, before next ref tick */
 assert(ft100_sample(&p,2,1,&c)==FT100_OK&&p.deadline_bus==12&&c==2);
 assert(ft100_sample(&p,2,2,&c)==FT100_OK&&p.deadline_bus==21&&c==1);
 assert(ft100_start(&p,2,2,0,2,0x10000)==FT100_OK&&!p.armed);
 assert(ft100_sample(&p,3,2,&c)==FT100_BAD&&p.terminal);
 for(unsigned div=1;div<=128;div*=2) {
  struct ft100 x={0};assert(ft100_init(&x,3,0,1000));
  assert(ft100_start(&x,3,0,100,div,0x10040)==FT100_OK);
  assert(ft100_sample(&x,3,1,&c)==FT100_OK&&c==(100*div-10+div-1)/div);
 }
 struct ft100 bad={0};assert(ft100_init(&bad,4,10,2));
 assert(ft100_sample(&bad,4,13,&c)==FT100_BAD&&bad.terminal);
 struct ft100 reverse={0};assert(ft100_init(&reverse,5,10,100));
 assert(ft100_sample(&reverse,5,9,&c)==FT100_BAD&&reverse.terminal);
 struct ft100 overflow={0};assert(ft100_init(&overflow,6,0,UINT64_MAX));
 assert(ft100_sample(&overflow,6,UINT64_MAX/10+1,&c)==FT100_BAD&&overflow.terminal);
 struct ft100 oldevent={0};assert(ft100_init(&oldevent,7,0,100));
 assert(ft100_start(&oldevent,7,0,1,1,5)==FT100_OK);
 assert(ft100_start(&oldevent,7,1,0,3,0xffffffff)==FT100_STOP&&oldevent.armed);
 struct ft100 invalid={0};assert(ft100_init(&invalid,8,0,100));
 assert(ft100_start(&invalid,8,0,3,1,0x30005)==FT100_OK);
 assert(ft100_start(&invalid,8,1,2,3,0x30005)==FT100_BAD);
 assert(invalid.last==1&&invalid.deadline_bus==12&&!invalid.terminal);
 assert(ft100_start(&invalid,8,2,2,1,0x40000)==FT100_BAD);
 assert(invalid.last==2&&invalid.deadline_bus==21&&!invalid.terminal);
 struct ft100 add={0};assert(ft100_init(&add,9,0,UINT64_MAX));
 assert(ft100_start(&add,9,UINT64_MAX/10,1,128,0x10040)==FT100_BAD&&add.terminal);
 struct ft100 phase={0};assert(ft100_init(&phase,10,0,UINT64_MAX));
 assert(ft100_start(&phase,10,UINT64_MAX/10-20,1,128,0x30040)==FT100_OK);
 assert(ft100_sample(&phase,10,UINT64_MAX/10,&c)==FT100_BAD&&phase.terminal);
 puts("firmware_timer_100m: PASS (pure clock contract; no wakeup proof)");
}
