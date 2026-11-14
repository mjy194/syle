#include "../firmware_cmos_valid_write.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct cb {unsigned calls,fail;bool recursive;struct fcrv_guard*b;struct fcr_state*s;uint64_t*g;struct fat_exit*e;};
static bool wr(void*p,uint32_t f,uint64_t v){struct cb*c=p;c->calls++;assert((f==0x6820&&v==0x46)||(f==0x681e&&v==0x1001));uint8_t op=0xee;
 if(c->recursive)assert(fcrv_write(c->b,c->s,c->g,c->e,1,0,&op,1,true,c,wr)==FAT_FATAL);
 return c->calls!=c->fail;
}
static bool oldwrite(void*p,uint32_t f,uint64_t v){(void)p;(void)f;(void)v;return true;}
int main(void){struct fwl_plan layout;struct fwl_request req={0x8000000,3653632,540672,32};assert(fwl_plan_build(&req,&layout));
 struct fcr_state s={0};assert(fcr_init(&s,&layout,7,100));uint64_t g[15];for(unsigned i=0;i<15;i++)g[i]=UINT64_MAX-i;
 g[0]=0x8d;g[3]=0x70;assert(fcr_io(&s,g,0x2000,0,30,0x700000,1,7,100,0,oldwrite));
 s.rtc.status_c=0x50;g[3]=0x71;g[0]=UINT64_C(0xabcdef1234567800);uint64_t saved[15];memcpy(saved,g,sizeof g);
 struct fcr_state pristine=s;struct fcrv_guard b={0};uint8_t op=0xee;
 struct fat_exit e={.rip=0x1000,.reason=30,.qualification=0x710000,.rflags=0x10046,.dr7=0x400};struct cb c={.b=&b,.s=&s,.g=g,.e=&e};
 assert(fcrv_write(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_COMMITTED&&c.calls==2&&!memcmp(&s,&pristine,sizeof s)&&!memcmp(g,saved,sizeof g));
 for(unsigned k=0;k<13;k++){struct fcr_state bad=s;struct fat_exit be=e;uint8_t code=op;unsigned len=1,cpl=0;c.calls=0;
  switch(k){case 0:bad.rtc.index=10;break;case 1:bad.rtc.index=15;break;case 2:g[0]=1;break;
  case 3:code=0xe6;break;case 4:len=2;break;case 5:cpl=3;break;case 6:g[3]=0x70;break;
  case 7:be.qualification=0x710008;break;case 8:be.rflags|=0x100;break;
  case 9:be.pending_debug=1;break;case 10:be.entry_event=0x80000020;break;case 11:bad.failed=true;break;case 12:be.blocking=1;break;}
  struct fcr_state before=bad;uint64_t old[15];memcpy(old,g,sizeof g);
  assert(fcrv_write(&b,&bad,g,&be,len,cpl,&code,1,true,&c,wr)==FAT_REJECTED&&!c.calls&&!memcmp(&bad,&before,sizeof bad)&&!memcmp(g,old,sizeof g));memcpy(g,saved,sizeof g);
 }
 for(unsigned k=1;k<=3;k++){s=pristine;b=(struct fcrv_guard){0};c.calls=0;c.fail=k<=2?k:0;c.recursive=k==3;
  assert(fcrv_write(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_FATAL&&s.failed&&b.failed&&!memcmp(g,saved,sizeof g));
  s.failed=false;assert(!memcmp(&s,&pristine,sizeof s));s.failed=true;unsigned n=c.calls;
  assert(fcrv_write(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_REJECTED&&c.calls==n);
 }
 /* Existing fcr rejects D0; no shared default policy was modified. */
 s=pristine;assert(!fcr_io(&s,g,0x1000,0,30,0x710000,1,7,100,0,oldwrite));
 s=pristine;b=(struct fcrv_guard){0};c.calls=0;c.fail=0;c.recursive=false;e.rflags=0x46;
 assert(fcrv_write(&b,&s,g,&e,1,0,&op,1,true,&c,wr)==FAT_COMMITTED&&c.calls==1&&!memcmp(&s,&pristine,sizeof s));
 /* RO VRT read remains80 after ignored zero write; no status mutation. */
 g[3]=0x71;g[0]=UINT64_MAX;
 assert(fcr_io(&s,g,0x2000,0,30,0x710008,1,7,100,0,oldwrite));
 assert((g[0]&255)==0x80&&s.rtc.index==13&&s.rtc.status_c==0x50);
 puts("firmware_cmos_valid_write: PASS (readonly D ignores zero, VRT retained)");
}
