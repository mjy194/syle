#include "../firmware_cmos_alarm_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct cb {unsigned calls,fail;bool recursive;struct fcra_guard*b;struct fcr_state*s;uint64_t*g;struct fat_exit*e;};
static bool wr(void*p,uint32_t f,uint64_t v){struct cb*c=p;c->calls++;assert((f==0x6820&&v==0x46)||(f==0x681e&&v==0x1001));uint8_t op=0xec;
 if(c->recursive)assert(fcra_read(c->b,c->s,c->g,c->e,1,0,&op,1,true,c,wr)==FAT_FATAL);
 return c->calls!=c->fail;
}
static bool oldwrite(void*p,uint32_t f,uint64_t v){(void)p;(void)f;(void)v;return true;}
int main(void){struct fwl_plan layout;struct fwl_request req={0x8000000,3653632,540672,32};assert(fwl_plan_build(&req,&layout));
 struct fcr_state s={0};assert(fcr_init(&s,&layout,7,100));uint64_t g[15];for(unsigned i=0;i<15;i++)g[i]=UINT64_MAX-i;
 struct fcra_guard b={0};uint8_t op=0xec;struct fat_exit e={.rip=0x1000,.reason=30,.qualification=0x710008,.rflags=0x10046,.dr7=0x400};struct cb c={.b=&b,.s=&s,.g=g,.e=&e};
 for(unsigned i=1;i<=5;i+=2)for(unsigned mask=0;mask<2;mask++){
  g[0]=i|(mask?128:0);g[3]=0x70;assert(fcr_io(&s,g,0x2000,0,30,0x700000,1,7,100,0,oldwrite));
  s.rtc.status_c=0x50;g[3]=0x71;g[0]=UINT64_C(0xabcdef12345678ff);struct fcr_state before=s;uint64_t saved[15];memcpy(saved,g,sizeof g);c.calls=0;
  assert(fcra_read(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_COMMITTED&&c.calls==2&&g[0]==UINT64_C(0xabcdef1234567800));
  assert(!memcmp(&s,&before,sizeof s));for(unsigned j=1;j<15;j++)assert(g[j]==saved[j]);
 }
 struct fcr_state pristine=s;
 for(unsigned k=0;k<14;k++){struct fcr_state bad=s;struct fat_exit be=e;uint8_t code=op;unsigned len=1,cpl=0;c.calls=0;
  switch(k){case 0:bad.rtc.index=0;break;case 1:bad.rtc.index=2;break;case 2:bad.rtc.index=4;break;
  case 3:code=0xee;break;case 4:len=2;break;case 5:cpl=3;break;case 6:g[3]=0x70;break;
  case 7:be.qualification=0x710000;break;case 8:be.rflags|=0x100;break;
  case 9:be.pending_debug=1;break;case 10:be.entry_event=0x80000020;break;case 11:bad.failed=true;break;case 12:be.blocking=1;break;case 13:bad.rtc.status_c|=0x20;break;}
  struct fcr_state before=bad;uint64_t old=g[0];
  assert(fcra_read(&b,&bad,g,&be,len,cpl,&code,1,true,&c,wr)==FAT_REJECTED&&!c.calls&&!memcmp(&bad,&before,sizeof bad)&&g[0]==old);g[3]=0x71;
 }
 for(unsigned k=1;k<=3;k++){s=pristine;b=(struct fcra_guard){0};c.calls=0;c.fail=k<=2?k:0;c.recursive=k==3;g[0]=UINT64_MAX;
  assert(fcra_read(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_FATAL&&s.failed&&b.failed&&g[0]==UINT64_MAX);
  s.failed=false;assert(!memcmp(&s,&pristine,sizeof s));s.failed=true;unsigned n=c.calls;
  assert(fcra_read(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_REJECTED&&c.calls==n);
 }
 /* Existing wrapper still rejects alarm reads/writes and AIE enable. */
 for(unsigned i=1;i<=5;i+=2){s=pristine;s.rtc.index=i;assert(!fcr_io(&s,g,0x1000,0,30,0x710008,1,7,100,0,oldwrite));
  s=pristine;s.rtc.index=i;g[0]=0;assert(!fcr_io(&s,g,0x1000,0,30,0x710000,1,7,100,0,oldwrite));}
 s=pristine;s.rtc.index=11;g[0]=0x22;assert(!fcr_io(&s,g,0x1000,0,30,0x710000,1,7,100,0,oldwrite));
 s=pristine;s.rtc.index=11;assert(fcr_io(&s,g,0x1000,0,30,0x710008,1,7,100,0,oldwrite)&&(g[0]&255)==2);
 s=pristine;b=(struct fcra_guard){0};c.calls=0;c.fail=0;c.recursive=false;e.rflags=0x46;
 assert(fcra_read(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_COMMITTED&&c.calls==1);
 puts("firmware_cmos_alarm_read: PASS (cold readonly alarms, AIE off/no AF)");
}
