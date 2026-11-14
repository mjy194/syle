#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "firmware_piix_pm.h"
struct ctx {bool fail;unsigned n;uint64_t rip;};
static bool wr(void*v,uint32_t f,uint64_t val){struct ctx*c=v;assert(f==0x681e);c->n++;c->rip=val;return !c->fail;}
static struct fpp_state ready(void){struct fpp_state s={0};assert(fpp_init(&s,1,100,100000));uint32_t v=0xb001;assert(fpm_config(&s.pm,0x40,4,true,&v));v=1;assert(fpm_config(&s.pm,0x80,1,true,&v));return s;}
int main(void){uint8_t in[]={0x66,0xed},out[]={0x66,0xef};uint64_t g[15]={0};g[3]=0xb004;struct ctx c={0};struct fpp_state s=ready();g[0]=0xabcdef123456beef;
#ifdef SYLE_FPM_SCI_CONTROL
 assert(fpp_io(&s,g,0x1000,0,0xb0040009,2,1,101,in,2,true,&c,wr));assert(g[0]==0xabcdef1234560000&&c.rip==0x1002);
 g[0]=0xdeadbeef00000001;assert(fpp_io(&s,g,0x1002,0,0xb0040001,2,1,102,out,2,true,&c,wr));assert(g[0]==0xdeadbeef00000001&&s.pm.sci_enabled);
 g[0]=0xaabbccddeeffaaaa;assert(fpp_io(&s,g,0x1004,0,0xb0040009,2,1,103,in,2,true,&c,wr));assert(g[0]==0xaabbccddeeff0001);
 for(unsigned value=2;value<65536;value++){s=ready();g[0]=value;c=(struct ctx){0};assert(!fpp_io(&s,g,0x1000,0,0xb0040001,2,1,101,out,2,true,&c,wr));assert(s.failed&&!s.pm.sci_enabled&&!c.n&&g[0]==value);}
 s=ready();g[0]=1;c=(struct ctx){.fail=true};assert(!fpp_io(&s,g,0x1000,0,0xb0040001,2,1,101,out,2,true,&c,wr));assert(s.failed&&!s.pm.sci_enabled&&g[0]==1&&c.n==1);assert(!fpp_io(&s,g,0x1000,0,0xb0040001,2,1,102,out,2,true,&c,wr));assert(c.n==1);
 s=ready();s.pm.enable=0;g[0]=0;c=(struct ctx){0};assert(!fpp_io(&s,g,0x1000,0,0xb0040009,2,1,101,in,2,true,&c,wr));assert(!c.n);
 s=ready();g[3]=0xb005;assert(!fpp_io(&s,g,0x1000,0,0xb0040009,2,1,101,in,2,true,&c,wr));assert(!c.n);
 s=ready();g[3]=0xb004;uint8_t bad[]={0xed,0x66};assert(!fpp_io(&s,g,0x1000,0,0xb0040009,2,1,101,bad,2,true,&c,wr));assert(!c.n);
 for(unsigned width=1;width<=4;width++){if(width==2)continue;s=ready();uint32_t v=0;assert(!fpm_io(&s.pm,1,101,0xb004,width,true,&v));}
 s=ready();g[0]=1;c=(struct ctx){0};assert(fpp_io(&s,g,0x1000,0,0xb0040001,2,1,101,out,2,true,&c,wr));g[0]=0;assert(fpp_io(&s,g,0x1002,0,0xb0040001,2,1,102,out,2,true,&c,wr));assert(!s.pm.sci_enabled);
#else
 assert(!fpp_io(&s,g,0x1000,0,0xb0040009,2,1,101,in,2,true,&c,wr));assert(s.failed&&!c.n);(void)out;
#endif
 s=ready();uint32_t t=0;assert(fpm_io(&s.pm,1,110,0xb008,4,true,&t));assert(t==3);puts("PM1 control profile PASS");}
