#include "../firmware_platform_id.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct writes {unsigned count;bool fail;uint64_t rip;};
static bool wr(void*x,uint32_t field,uint64_t value){struct writes*w=x;w->count++;assert(field==0x681e);w->rip=value;return !w->fail;}
int main(void){
 uint64_t initial[]={0,1,UINT64_MAX,UINT64_C(7)<<50,UINT64_C(0x123456789abcdef0)};
 for(unsigned i=0;i<sizeof initial/sizeof*initial;i++){uint64_t v=initial[i];assert(fpid_read(0x600,0x17,&v)&&v==0);}
 /* Exhaust low signatures/MSR space and explicitly cover high-bit variants. */
 for(uint32_t sig=0;sig<=0xffff;sig++){uint64_t v=UINT64_MAX;bool ok=sig==0x600;assert(fpid_read(sig,0x17,&v)==ok&&v==(ok?0:UINT64_MAX));}
 for(uint32_t msr=0;msr<=0xffff;msr++){uint64_t v=UINT64_C(0x123456789abcdef0);bool ok=msr==0x17;assert(fpid_read(0x600,msr,&v)==ok&&v==(ok?0:UINT64_C(0x123456789abcdef0)));}
 for(unsigned i=0;i<32;i++){
  uint64_t v=0xaabb;assert(!fpid_read(0x600^(UINT32_C(1)<<i),0x17,&v)&&v==0xaabb);
  assert(!fpid_read(0x600,0x17^(UINT32_C(1)<<i),&v)&&v==0xaabb);
 }
 uint64_t v=0xaabb;assert(!fpid_read(UINT32_MAX,UINT32_MAX,&v)&&v==0xaabb);
 assert(!fpid_read(0x600,0x17,0)&&!fpid_read(0,0,0));
 struct fpid_state state={0};uint64_t g[15],saved[15];for(unsigned i=0;i<15;i++)g[i]=0x100+i;g[2]=0xffffffff00000017ULL;
 memcpy(saved,g,sizeof g);uint8_t insn[]={0x0f,0x32};struct writes w={0};
 assert(fpid_apply(&state,0x600,true,g,0x8000,2,0,31,insn,2,true,&w,wr));
 assert(!state.failed&&w.count==1&&w.rip==0x8002&&!g[0]&&!g[3]);for(unsigned i=0;i<15;i++)if(i!=0&&i!=3)assert(g[i]==saved[i]);
 memcpy(g,saved,sizeof g);w=(struct writes){0};
 assert(!fpid_apply(&state,0,false,g,UINT64_MAX,0,3,32,0,0,false,&w,wr)&&!state.failed&&!w.count&&!memcmp(g,saved,sizeof g));
 for(unsigned test=0;test<15;test++){
  state=(struct fpid_state){0};w=(struct writes){0};memcpy(g,saved,sizeof g);insn[0]=0x0f;insn[1]=0x32;
  uint32_t sig=0x600,reason=31;bool intel=true;unsigned len=2,cpl=0;size_t n=2;uint64_t rip=0x8000;
  if(test==0)sig++;
  if(test==1)intel=false;
  if(test==2)reason=32;
  if(test==3)len=3;
  if(test==4)cpl=3;
  if(test==5)n=1;
  if(test==6)insn[0]=0x90;
  if(test==7)insn[1]=0x30;
  if(test==8)rip=UINT64_MAX;
  if(test==9)rip=0x800000000000ULL;
  if(test==10)rip=0x7fffffffffffULL;
  if(test==11)g[2]=0x18;
  if(test==12)w.fail=true;
  uint64_t before[15];memcpy(before,g,sizeof g);
  assert(!fpid_apply(&state,sig,intel,g,rip,len,cpl,reason,test==13?0:insn,n,true,&w,test==14?0:wr)&&state.failed&&!memcmp(g,before,sizeof g));
  assert(w.count==(test==12?1u:0u));unsigned calls=w.count;
  assert(!fpid_apply(&state,0x600,true,g,0x8000,2,0,31,insn,2,true,&w,wr)&&w.count==calls);
 }
 state=(struct fpid_state){0};w=(struct writes){0};insn[0]=0x0f;insn[1]=0x32;
 assert(!fpid_apply(&state,0x600,true,0,0x8000,2,0,31,insn,2,true,&w,wr)&&state.failed&&!w.count);
 assert(!fpid_apply(0,0x600,true,g,0x8000,2,0,31,insn,2,true,&w,wr)&&!w.count);
 puts("platform ID: exact synthetic signature/MSR, zero virtual class, rejected output stable PASS");return 0;
}
