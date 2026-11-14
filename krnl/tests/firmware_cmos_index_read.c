#include "../firmware_cmos_index_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct callback {unsigned calls,fail;bool reenter;struct fcri_guard*b;struct fcr_state*s;uint64_t*g;struct fat_exit*e;};
static bool wr(void*p,uint32_t f,uint64_t v){struct callback*c=p;c->calls++;assert((f==0x6820&&v==0x46)||(f==0x681e&&v==0x1001));
 uint8_t op=0xec;if(c->reenter)assert(fcri_read(c->b,c->s,c->g,c->e,1,0,&op,1,true,c,wr)==FAT_FATAL);
 return c->calls!=c->fail;
}
static bool oldwrite(void*p,uint32_t f,uint64_t v){(void)p;(void)f;(void)v;return true;}
int main(void){struct fwl_plan layout;struct fwl_request req={0x8000000,3653632,540672,32};assert(fwl_plan_build(&req,&layout));
 struct fcr_state s={0};assert(fcr_init(&s,&layout,7,100));struct fcri_guard b={0};
 struct fat_exit e={.rip=0x1000,.reason=30,.qualification=0x700008,.rflags=0x10046,.dr7=0x400};
 uint64_t g[15]={UINT64_C(0x123456789abcde00)};g[3]=0x70;uint8_t op=0xec;
 struct callback c={.b=&b,.s=&s,.g=g,.e=&e};
 for(unsigned v=0;v<256;v++){
  /* Real existing OUT transaction creates both index and mask latches. */
  g[0]=v;assert(fcr_io(&s,g,0x2000,0,30,0x700000,1,7,100,0,oldwrite));
  g[0]=UINT64_C(0x123456789abcdeaa);struct fcr_state old=s;c.calls=0;
  assert(fcri_read(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_COMMITTED&&g[0]==(UINT64_C(0x123456789abcde00)|v));
  assert(c.calls==2&&!memcmp(&s,&old,sizeof s));
 }
 s.rtc.index=12;s.rtc.status_c=0x50;struct fcr_state pristine=s;
 for(unsigned k=0;k<15;k++){struct fat_exit be=e;struct fcr_state bad=s;unsigned len=1,cpl=0;uint8_t code=0xec;g[3]=0x70;c.calls=0;
  switch(k){case 0:code=0xe4;break;case 1:len=2;break;case 2:g[3]=0x71;break;
  case 3:be.qualification=0x700048;break;case 4:be.qualification=0x710008;break;
  case 5:cpl=3;break;case 6:be.rflags|=0x100;break;case 7:be.blocking=1;break;
  case 8:be.dr7=0x401;break;case 9:be.entry_event=0x80000020;break;
  case 10:bad.rtc.index=128;break;case 11:bad.rtc.status_c=1;break;
  case 12:bad.ready=false;break;case 13:bad.failed=true;break;case 14:be.pending_debug=1;break;}
  struct fcr_state before=bad;uint64_t old=g[0];assert(fcri_read(&b,&bad,g,&be,len,cpl,&code,1,true,&c,wr)==FAT_REJECTED&&!c.calls&&g[0]==old&&!memcmp(&bad,&before,sizeof bad));
 }
 g[3]=0x70;
 for(unsigned k=1;k<=3;k++){s=pristine;b=(struct fcri_guard){0};c.calls=0;c.fail=k<=2?k:0;c.reenter=k==3;g[0]=UINT64_MAX;
  assert(fcri_read(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_FATAL&&s.failed&&b.failed&&g[0]==UINT64_MAX);
  assert(s.rtc.index==12&&s.rtc.status_c==0x50&&s.rtc.tick==pristine.rtc.tick&&s.last==pristine.last);
  unsigned n=c.calls;assert(fcri_read(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_REJECTED&&c.calls==n);
 }
 /* Old index input still rejects; no linux_rtc behavior was widened. */
 struct lrtc_state oldrtc;lrtc_init(&oldrtc);assert(!lrtc_io(&oldrtc,g,0x1000,0,30,0x700008,1,0,0,oldwrite));
 s=pristine;b=(struct fcri_guard){0};c.calls=0;c.fail=0;c.reenter=false;e.rflags=0x46;
 assert(fcri_read(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_COMMITTED&&c.calls==1&&s.rtc.status_c==0x50);
 g[3]=0x71;assert(fcr_io(&s,g,0x2000,0,30,0x710008,1,7,10000100,0,oldwrite));assert(s.last==10000100&&s.rtc.tick==1193182&&s.rtc.status_c==0);
 puts("firmware_cmos_index_read: PASS (private latch, unchanged default RTC)");
}
