#include "../linux_boot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char low[0xa0000];static unsigned calls,fail;
static bool wr(void*c,uint64_t a,const void*p,size_t n){(void)c;if(++calls==fail)return false;assert(a<=sizeof low&&n<=sizeof low-a);memcpy(low+a,p,n);return true;}
static bool zr(void*c,uint64_t a,size_t n){(void)c;if(++calls==fail)return false;assert(a<=sizeof low&&n<=sizeof low-a);memset(low+a,0,n);return true;}
static uint64_t get(unsigned a,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)low[a+i]<<(8*i);return v;}
int main(void){
 struct lb_plan p={.ram_size=LB_RAM_SIZE,.kernel_addr=0x1000000,.kernel_size=4096,.kernel_envelope=0x400000,.params_addr=0x90000,.entry_addr=0x1000200};struct lb_entry_state e;struct lb_writer w={0,wr,zr};
 memset(low,0xcc,sizeof low);assert(lb_entry_prepare(&p,&w,&e));assert(calls==73&&e.gdt_limit==47);
 assert(get(0x4020,8)==UINT64_C(0x00008b0060000067));assert(get(0x4028,8)==0);assert(get(0x6004,8)==0x80000);assert(get(0x6066,2)==104);
 for(unsigned i=0;i<4096;i++)if(!(i>=4&&i<12)&&!(i>=102&&i<104))assert(low[0x6000+i]==0);
 for(unsigned i=70;i<=73;i++){calls=0;fail=i;assert(!lb_entry_prepare(&p,&w,&e));assert(calls==i);}
 puts("PASS NEW guest TSS: busy64 descriptor selector0x20, limit103/base6000, zero upper descriptor and IST, RSP0=80000, iomap104, GDT limit47; four new write failures; no VM");
}
