#include "../firmware_timer_100m_mmio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void fresh(struct ft100_mmio*s){memset(s,0,sizeof*s);assert(ft100_mmio_init(s,7,100,1000));}
static enum ft100_result op(struct ft100_mmio*s,uint64_t t,uint64_t off,bool write,uint32_t*v){return ft100_mmio_access(s,7,t,true,off,4,write,v);}
static void set(struct ft100_mmio*s,uint64_t t,uint64_t off,uint32_t v){assert(op(s,t,off,true,&v)==FT100_OK);}
static uint32_t get(struct ft100_mmio*s,uint64_t t,uint64_t off){uint32_t v=0xdead;assert(op(s,t,off,false,&v)==FT100_OK);return v;}
int main(void){
 struct ft100_mmio s;fresh(&s);assert(!ft100_mmio_init(&s,7,100,1000));
 assert(get(&s,100,0x320)==0x10000&&get(&s,100,0x380)==0&&get(&s,100,0x390)==0&&get(&s,100,0x3e0)==0);
 /* All divider encodings and low vector configuration while unarmed. */
 unsigned dcr[]={0,1,2,3,8,9,10,11},div[]={2,4,8,16,32,64,128,1};
 for(unsigned i=0;i<8;i++){set(&s,100,0x3e0,dcr[i]);assert(s.timer.divisor==div[i]&&get(&s,100,0x3e0)==dcr[i]);}
 set(&s,100,0x320,5);assert(!s.timer.armed&&get(&s,101,0x320)==5);
 /* 100 bus ticks at divide1 = 10 reference ticks. mask changes keep phase. */
 set(&s,101,0x380,100);uint64_t due=s.timer.deadline_bus;
 assert(due==110&&get(&s,104,0x390)==70);
 set(&s,104,0x320,0x10005);assert(s.timer.deadline_bus==due);
 set(&s,105,0x320,5);assert(s.timer.deadline_bus==due&&get(&s,105,0x390)==60);
 uint32_t v=0;assert(op(&s,105,0x3e0,true,&v)==FT100_BAD&&s.timer.deadline_bus==due&&s.dcr==11);
 v=0x20005;assert(op(&s,105,0x320,true,&v)==FT100_BAD&&s.timer.lvt==5);
 /* At due, cancellation or masking cannot erase the unmasked low-vector event. */
 v=0;assert(op(&s,111,0x380,true,&v)==FT100_STOP&&s.timer.terminal&&s.timer.armed&&s.timer.deadline_bus==due&&s.timer.initial==100);
 struct ft100_mmio terminal=s;v=0x10005;assert(op(&s,111,0x320,true,&v)==FT100_BAD&&!memcmp(&s,&terminal,sizeof s));
 /* Even invalid width/null output samples first and preserves STOP. */
 fresh(&s);set(&s,100,0x320,5);set(&s,100,0x380,5);
 assert(ft100_mmio_access(&s,7,101,true,0x999,1,true,0)==FT100_STOP&&s.timer.armed);
 /* Masked oneshot expires legitimately; initial register retains original. */
 fresh(&s);set(&s,100,0x380,50);assert(get(&s,110,0x390)==0&&!s.timer.armed&&get(&s,110,0x380)==50);
 /* Cancel and restart use the sampled current reference as origin of period. */
 set(&s,110,0x380,100);due=s.timer.deadline_bus;set(&s,112,0x380,0);assert(!s.timer.armed&&!s.timer.initial);
 set(&s,113,0x380,100);assert(s.timer.deadline_bus==330&&s.timer.deadline_bus>due);
 /* Periodic masked coalescing, preserve phase on vector/mask rewrite. */
 fresh(&s);set(&s,100,0x320,0x30005);set(&s,100,0x380,10);
 assert(get(&s,107,0x390)==5&&s.timer.deadline_bus==80&&s.timer.armed);
 set(&s,107,0x320,0x20006);assert(s.timer.deadline_bus==80);
 v=0xdead;assert(op(&s,108,0x390,false,&v)==FT100_STOP&&v==0xdead&&s.timer.deadline_bus==80);
 /* Software disabled is terminal; no unmasked event is consumed. */
 fresh(&s);set(&s,100,0x320,0x41);set(&s,100,0x380,10);
 v=0xbeef;assert(ft100_mmio_access(&s,7,101,false,0x390,4,false,&v)==FT100_BAD&&s.timer.terminal&&s.timer.armed&&v==0xbeef);
 fresh(&s);set(&s,100,0x320,0x41);set(&s,100,0x380,10);
 assert(ft100_mmio_access(&s,7,102,false,0x390,4,false,&v)==FT100_STOP&&s.timer.armed);
 /* Bad accesses preserve output/config, but sampling effects are durable. */
 fresh(&s);set(&s,100,0x380,5);v=0xdead;
 assert(op(&s,102,0x391,false,&v)==FT100_BAD&&v==0xdead&&s.timer.last==102&&!s.timer.armed);
 uint64_t offsets[]={0x321,0x381,0x391,0x3e1,0x310,UINT64_MAX};
 for(unsigned i=0;i<sizeof offsets/sizeof*offsets;i++)assert(op(&s,102,offsets[i],false,&v)==FT100_BAD&&v==0xdead);
 assert(ft100_mmio_access(&s,7,102,true,0x320,8,false,&v)==FT100_BAD&&v==0xdead);
 assert(ft100_mmio_access(&s,7,102,true,0x320,4,false,0)==FT100_BAD);
 v=4;assert(op(&s,102,0x3e0,true,&v)==FT100_BAD&&s.dcr==0);
 v=0x40000;assert(op(&s,102,0x320,true,&v)==FT100_BAD&&s.timer.lvt==0x10000);
 assert(op(&s,102,0x390,true,&v)==FT100_BAD);
 /* Clock discontinuity is sticky and output never changed. */
 fresh(&s);v=0xdead;assert(ft100_mmio_access(&s,8,100,true,0x390,4,false,&v)==FT100_BAD&&s.timer.terminal&&v==0xdead);
 fresh(&s);assert(op(&s,99,0x390,false,&v)==FT100_BAD&&s.timer.terminal);
 fresh(&s);assert(op(&s,1101,0x390,false,&v)==FT100_BAD&&s.timer.terminal);
 assert(ft100_mmio_access(0,7,100,true,0x390,4,false,&v)==FT100_BAD);
 puts("firmware timer100 MMIO: phase, masked expiry, persistent STOP, failclosed clock PASS");return 0;
}
