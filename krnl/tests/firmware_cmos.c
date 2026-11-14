#include "../firmware_cmos.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned calls;static bool fail;
static bool wr(void*p,uint32_t f,uint64_t v){(void)p;assert(f==0x681e&&v==0x1001);calls++;return !fail;}
static bool io(struct fcr_state*s,uint64_t*g,unsigned port,bool input,uint64_t ref){g[3]=port;return fcr_io(s,g,0x1000,0,30,((uint64_t)port<<16)|(input?8:0),1,1,ref,0,wr);}
int main(void){
 struct fwl_plan layout;struct fwl_request request={128*1024*1024,3653632,540672,32};
 assert(fwl_plan_build(&request,&layout));
 struct fcr_state s={0};uint64_t g[15]={0};assert(fcr_init(&s,&layout,1,100));
 g[0]=0x8f;assert(io(&s,g,0x70,false,100)&&s.rtc.index==15&&s.rtc.nmi_mask);
 g[0]=0x12345678;assert(io(&s,g,0x71,true,100)&&g[0]==0x12345600);
 g[0]=0x0f;assert(io(&s,g,0x70,false,100)&&!s.rtc.nmi_mask);
 /* Exact observed cold-boot clear, repeated, with NMI latch preserved. */
 g[0]=0x8f;assert(io(&s,g,0x70,false,100));
 g[0]=0x123400;calls=0;assert(io(&s,g,0x71,false,101)&&calls==1);
 assert(s.rtc.index==15&&s.rtc.nmi_mask&&g[0]==0x123400);
 assert(io(&s,g,0x71,false,102));assert(io(&s,g,0x71,true,102)&&g[0]==0x123400);
 struct fcr_state broken=s;g[0]=0;fail=true;calls=0;
 assert(!io(&broken,g,0x71,false,103)&&broken.failed&&broken.rtc.failed&&calls==1);
 assert(broken.last==s.last&&broken.rtc.tick==s.rtc.tick&&broken.rtc.nmi_mask);
 fail=false;calls=0;assert(!io(&broken,g,0x71,false,103)&&!calls);
 for(unsigned index=0;index<128;index++)for(unsigned val=0;val<256;val++){
  if(index==15&&val==0)continue;
  struct fcr_state denied={0};assert(fcr_init(&denied,&layout,1,0));
  g[0]=index;assert(io(&denied,g,0x70,false,0));g[0]=val;calls=0;
  assert(!io(&denied,g,0x71,false,1)&&denied.failed&&!calls&&denied.last==0);
 }
 struct fcr_state saved=s;g[0]=0x8a;fail=true;calls=0;
 assert(!io(&s,g,0x70,false,103)&&s.failed&&s.rtc.failed&&calls==1);
 assert(s.rtc.index==saved.rtc.index&&s.rtc.nmi_mask==saved.rtc.nmi_mask&&s.last==saved.last&&g[0]==0x8a);fail=false;
 for(unsigned i=0;i<4;i++){
  struct fcr_state x={0};assert(fcr_init(&x,&layout,1,0));g[0]=i==0?0x10:0x0f;assert(io(&x,g,0x70,false,0));calls=0;g[0]=0xfe;
  bool ok=i==0?io(&x,g,0x71,true,0):i==1?io(&x,g,0x71,false,0):i==2?io(&x,g,0x70,true,0):fcr_io(&x,g,0x1000,0,30,UINT64_C(0x710001),1,1,0,0,wr);
  assert(!ok&&x.failed&&!calls&&g[0]==0xfe);
 }
 /* Every memory descriptor matches the admitted layout-derived model. */
 for(unsigned i=0;i<5;i++){
  unsigned ix=(unsigned[]){0x34,0x35,0x5b,0x5c,0x5d}[i];uint8_t expected;
  struct fcr_state x={0};assert(fcr_init(&x,&layout,1,0));
  assert(fcmi_read(&x.memory,ix,&expected));g[0]=ix;
  assert(io(&x,g,0x70,false,0));g[0]=0x123400;
  assert(io(&x,g,0x71,true,1)&&g[0]==(0x123400|expected));
 }
 struct fcr_state invalid={0};struct fwl_plan bad=layout;bad.ram.size-=4096;
 assert(!fcr_init(&invalid,&bad,1,0)&&!invalid.ready);
 /* Same elapsed Hv source, declared synthetic calendar, no host time. */
 struct fcr_state date={0};assert(fcr_init(&date,&layout,1,500));
 g[0]=0;assert(io(&date,g,0x70,false,500));assert(io(&date,g,0x71,true,10000500)&&(g[0]&255)==1);
 assert(date.rtc.tick==1193182);
 g[0]=7;assert(io(&date,g,0x70,false,10000500));assert(io(&date,g,0x71,true,10000500)&&(g[0]&255)==1);
 g[0]=8;assert(io(&date,g,0x70,false,10000500));assert(io(&date,g,0x71,true,10000500)&&(g[0]&255)==1);
 g[0]=9;assert(io(&date,g,0x70,false,10000500));assert(io(&date,g,0x71,true,10000500)&&(g[0]&255)==0);
 struct fcr_state fractional={0};assert(fcr_init(&fractional,&layout,1,0));g[0]=15;
 assert(io(&fractional,g,0x70,false,9999999)&&fractional.rtc.tick==1193181);
 calls=0;assert(!io(&fractional,g,0x71,true,9999998)&&fractional.failed&&!calls);
 struct fcr_state epoch={0};assert(fcr_init(&epoch,&layout,1,0));calls=0;
 assert(!fcr_io(&epoch,g,0x1000,0,30,UINT64_C(0x700000),1,2,0,0,wr)&&epoch.failed&&!calls);
 puts("firmware CMOS: cold boot, NMI latch, synthetic Hv calendar, atomic rejects PASS");
}
