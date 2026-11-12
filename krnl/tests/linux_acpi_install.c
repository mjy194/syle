#include "../linux_acpi_install.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char mem[0x100000];
struct fake{unsigned calls,fail,writes;bool corrupt;uint64_t last;};
static bool rd(void*p,uint64_t a,void*b,size_t n){struct fake*f=p;assert(a<=sizeof mem&&n<=sizeof mem-a);if(++f->calls==f->fail)return false;memcpy(b,mem+a,n);if(f->corrupt&&a==LAT_GPA)((unsigned char*)b)[0]^=1;return true;}
static bool wr(void*p,uint64_t a,const void*b,size_t n){struct fake*f=p;assert((a==LAT_GPA&&n==LAT_SIZE)||(a==0x90070&&n==8));f->writes++;f->last=a;memcpy(mem+a,b,n);return ++f->calls!=f->fail;}
static void put(unsigned char*p,uint64_t n,unsigned bytes){for(unsigned i=0;i<bytes;i++)p[i]=n>>(i*8);}
static void setup(void){memset(mem,0,sizeof mem);unsigned char*b=mem+0x90000;put(b+0x202,0x53726448,4);put(b+0x206,0x20f,2);b[0x1e8]=1;put(b+0x2d0,0x9f000,8);put(b+0x2d8,0x61000,8);put(b+0x2e0,2,4);}
int main(void){for(unsigned fail=0;fail<=5;fail++){setup();enum lai_state s=LAI_FRESH;struct fake f={.fail=fail};struct lai_ops o={&f,rd,wr};bool ok=lai_install(&s,&o,0x8000000,0x90000);assert(ok==(fail==0));assert(s==(fail==0?LAI_INSTALLED:fail==1?LAI_FRESH:LAI_DIRTY));if(fail==0)assert(f.last==0x90070&&f.writes==2);if(s!=LAI_FRESH){unsigned old=f.calls;assert(!lai_install(&s,&o,0x8000000,0x90000)&&f.calls==old);}}
 for(unsigned mode=0;mode<6;mode++){setup();enum lai_state s=LAI_FRESH;struct fake f={0};struct lai_ops o={&f,rd,wr};unsigned char*b=mem+0x90000;if(mode==0)b[0x70]=1;if(mode==1)b[0x1e8]=129;if(mode==2)b[0x2e0]=1;if(mode==3)put(b+0x2d8,UINT64_MAX,8);if(mode==4)put(b+0x2d0,0xe0001,8);if(mode==5){b[0x1e8]=2;memcpy(b+0x2e4,b+0x2d0,20);}assert(!lai_install(&s,&o,0x8000000,0x90000)&&s==LAI_FRESH&&!f.writes);}
 setup();enum lai_state s=LAI_FRESH;struct fake f={.corrupt=true};struct lai_ops o={&f,rd,wr};assert(!lai_install(&s,&o,0x8000000,0x90000)&&s==LAI_DIRTY&&f.writes==1);assert(!mem[0x90070]);
 puts("ACPI fragment install host tests PASS");}
