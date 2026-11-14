#include "../firmware_timer_div2to1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define N UINT64_C(0xffffffff)
static struct ft100_mmio fresh(void){struct ft100_mmio s={0};assert(ft100_mmio_init(&s,7,100,UINT64_C(1000000000000)));assert(ft100_start(&s.timer,7,100,UINT32_MAX,2,0x30005)==FT100_OK);return s;}
static uint32_t count(struct ft100_mmio*s,uint64_t r){uint32_t out=0;assert(ft100_sample(&s->timer,7,r,&out)==FT100_OK);return out;}
int main(void){
 struct ft100_mmio live=fresh();uint32_t before=count(&live,103);struct ft100_mmio stage=live,old=live;
 assert(before==UINT32_MAX-15&&ft100_mmio_redivide_2_to_1(&stage,7,103,true,4,0xb)==FT100_OK);
 assert(stage.timer.initial==UINT32_MAX&&stage.timer.period_bus==N&&stage.timer.divisor==1&&stage.dcr==0xb&&stage.timer.deadline_bus==30+(N-15)&&count(&stage,103)==before);
 assert(count(&stage,104)==before-10&&count(&old,104)==before-5);assert(live.timer.divisor==2&&live.dcr==0); /* not yet published */
 /* Masked periods are sampled FIRST, then remaining count is conserved. */
 for(unsigned k=1;k<=5;k++){
  live=fresh();uint64_t ref=100+(2*N*k)/10+17;before=count(&live,ref);uint64_t deadline=live.timer.deadline_bus,elapsed=(ref-100)*10;
  assert(deadline>elapsed&&deadline%2==0);assert(ft100_mmio_redivide_2_to_1(&live,7,ref,true,4,0xb)==FT100_OK&&count(&live,ref)==before&&live.timer.deadline_bus==elapsed+(deadline-elapsed)/2&&live.timer.initial==UINT32_MAX);
 }
 /* Exact old periodic boundary N*2 is divisible by10: no skipped partial phase. */
 live=fresh();uint64_t boundary=100+2*N/10;assert(count(&live,boundary)==UINT32_MAX);
 assert(ft100_mmio_redivide_2_to_1(&live,7,boundary,true,4,0xb)==FT100_OK&&count(&live,boundary)==UINT32_MAX);
 uint64_t next_boundary=boundary+N/10;assert(count(&live,next_boundary)==5);
 assert(count(&live,next_boundary+1)==UINT32_MAX-5); /* crossed boundary by5 bus ticks */
 /* Illegal/odd state is not rounded or restarted. */
 live=fresh();live.timer.deadline_bus--;uint64_t baddeadline=live.timer.deadline_bus;
 assert(ft100_mmio_redivide_2_to_1(&live,7,100,true,4,0xb)==FT100_BAD&&live.timer.deadline_bus==baddeadline&&live.timer.divisor==2&&live.dcr==0);
 for(unsigned test=0;test<9;test++){
  live=fresh();unsigned width=4;uint32_t value=0xb;
  if(test==0)live.dcr=1;
  if(test==1)live.timer.divisor=4;
  if(test==2)live.timer.initial--;
  if(test==3)live.timer.lvt=0x30006;
  if(test==4)live.timer.period_bus-=2;
  if(test==5)live.timer.armed=false;
  if(test==6)width=1;
  if(test==7)value=0;
  if(test==8)live.timer.lvt=0x10005;
  old=live;assert(ft100_mmio_redivide_2_to_1(&live,7,100,true,width,value)==FT100_BAD&&!memcmp(&live,&old,sizeof live));
 }
 /* Due unmasked event precedes even unsupported config/width/value. */
 live=fresh();live.timer.lvt=0x20005;assert(ft100_mmio_redivide_2_to_1(&live,7,boundary,true,1,0)==FT100_STOP&&live.timer.terminal&&live.timer.divisor==2&&live.timer.armed);
 old=live;assert(ft100_mmio_redivide_2_to_1(&live,7,boundary,true,4,0xb)==FT100_BAD&&!memcmp(&live,&old,sizeof live));
 live=fresh();assert(ft100_mmio_redivide_2_to_1(&live,7,100,false,4,0xb)==FT100_BAD&&live.timer.terminal);
 live=fresh();assert(ft100_mmio_redivide_2_to_1(&live,8,100,true,4,0xb)==FT100_BAD&&live.timer.terminal);
 live=fresh();assert(ft100_mmio_redivide_2_to_1(&live,7,99,true,4,0xb)==FT100_BAD&&live.timer.terminal);
 live=fresh();live.timer.max_gap=UINT64_MAX;assert(ft100_mmio_redivide_2_to_1(&live,7,UINT64_MAX,true,4,0xb)==FT100_BAD&&live.timer.terminal);
 /* Masked periodic overflow stays terminal, never wrapped/restarted. */
 live=fresh();live.timer.max_gap=UINT64_MAX;uint64_t ref=100+UINT64_MAX/10;
 assert(ft100_mmio_redivide_2_to_1(&live,7,ref,true,4,0xb)==FT100_BAD&&live.timer.terminal);
 /* Default profile remains strict even for this otherwise supported new case. */
 live=fresh();uint32_t value=0xb;assert(ft100_mmio_access(&live,7,100,true,0x3e0,4,true,&value)==FT100_BAD&&live.timer.divisor==2);
 assert(ft100_mmio_redivide_2_to_1(0,7,100,true,4,0xb)==FT100_BAD);
 puts("virtual div2to1: fixed masked periodic, exact count/phase,100ns observations,due/overflow/default gates PASS");
 return 0;
}
