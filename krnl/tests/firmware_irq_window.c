#include "../firmware_irq_window.h"
#include <assert.h>
#include <stdio.h>
struct wr {unsigned calls;bool fail;uint32_t field;uint64_t value;};
static bool write(void*x,uint32_t f,uint64_t v){struct wr*w=x;w->calls++;w->field=f;w->value=v;return !w->fail;}
int main(void){
 const uint32_t base=0x8a123180;const uint64_t caps=UINT64_C(4)<<32;
 struct lei_snapshot g={.rflags=2};uint64_t token=0;
 struct fiw_state s={0};assert(fiw_init(&s,1,4096,base,caps));struct wr w={0};
 assert(fiw_before(&s,1,4096,base,true,false,&g,&w,write,&token)==FIW_WAIT&&!w.calls);
 assert(fiw_before(&s,1,4096,base,true,true,&g,&w,write,&token)==FIW_ARMED&&w.field==0x4002&&w.value==(base|4));
 assert(fiw_after(&s,1,4096,token,base|4,7,true,&w,write)==FIW_RECHECK&&w.value==base&&w.calls==2);
 g.rflags=0x202;assert(fiw_before(&s,1,4096,base,true,true,&g,&w,write,&token)==FIW_READY&&w.calls==2);
 g.blocking=1;assert(fiw_before(&s,1,4096,base,true,true,&g,&w,write,&token)==FIW_ARMED);
 assert(fiw_after(&s,1,4096,token,base|4,52,true,&w,write)==FIW_RECHECK);
 for(unsigned phase=0;phase<2;phase++){
  struct fiw_state q={0};assert(fiw_init(&q,1,4096,base,caps));struct wr f={.fail=!phase};
  enum fiw_result r=fiw_before(&q,1,4096,base,true,true,&g,&f,write,&token);
  if(phase){assert(r==FIW_ARMED);f.fail=true;r=fiw_after(&q,1,4096,token,base|4,7,true,&f,write);}
  assert(r==FIW_FATAL&&q.failed);unsigned n=f.calls;
  assert(fiw_before(&q,1,4096,base,true,true,&g,&f,write,&token)==FIW_FATAL&&f.calls==n);
 }
 struct fiw_state q={0};assert(!fiw_init(&q,1,4096,base,caps|4));assert(!fiw_init(&q,1,4096,base,0));assert(!fiw_init(&q,1,4096,base|4,caps));
 assert(fiw_init(&q,1,4096,base,caps));assert(fiw_before(&q,1,4096,base^8,true,true,&g,&w,write,&token)==FIW_FATAL);
 puts("IRQ window: PPR wait, IF/shadow gating, full controls preserve, reason7/52 recheck, sticky failure PASS");
}
