#include "../linux_boot.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *ram;static unsigned calls,fail;
static void put(unsigned char*p,unsigned long long v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=v>>(i*8);}
static unsigned long long get(const unsigned char*p,unsigned n){unsigned long long v=0;for(unsigned i=0;i<n;i++)v|=(unsigned long long)p[i]<<(i*8);return v;}
static bool wr(void*c,uint64_t a,const void*p,size_t n){(void)c;if(++calls==fail)return false;assert(a<=LB_RAM_SIZE&&n<=LB_RAM_SIZE-a);memcpy(ram+a,p,n);return true;}
static bool zr(void*c,uint64_t a,size_t n){(void)c;if(++calls==fail)return false;assert(a<=LB_RAM_SIZE&&n<=LB_RAM_SIZE-a);memset(ram+a,0,n);return true;}
int main(void){
 unsigned char b[8192]={0},rd[5000];memset(rd,0x5a,sizeof rd);
 b[0x1f1]=4;put(b+0x1f4,256,4);put(b+0x1fe,0xaa55,2);b[0x200]=0xeb;b[0x201]=0x6a;
 put(b+0x202,0x53726448,4);put(b+0x206,0x20c,2);b[0x211]=1;b[0x234]=1;put(b+0x236,1,2);put(b+0x230,0x200000,4);put(b+0x238,2048,4);put(b+0x22c,0x7fffffff,4);put(b+0x258,0x1000000,8);put(b+0x260,0x400000,4);
 memset(b+2560,0x42,4096);
 struct lb_plan p;struct lb_entry_state e;struct lb_writer w={0,wr,zr};
 assert(lb_inspect(b,sizeof b,sizeof rd,"console=ttyS0",&p));assert(p.kernel_addr==0x1000000&&p.entry_addr==0x1000200&&p.initrd_addr==0x7ffe000);
 assert(!lb_inspect(b,0x267,sizeof rd,"x",&p));assert(!lb_inspect(b,0x268,sizeof rd,"x",&p));
 b[0x234]=0;assert(!lb_inspect(b,sizeof b,sizeof rd,"x",&p));b[0x234]=1;
 put(b+0x1f4,0xffffffff,4);assert(!lb_inspect(b,sizeof b,sizeof rd,"x",&p));put(b+0x1f4,256,4);
 put(b+0x260,0xffffffff,4);assert(!lb_inspect(b,sizeof b,sizeof rd,"x",&p));put(b+0x260,0x400000,4);
 assert(!lb_inspect(b,sizeof b,LB_RAM_SIZE,"x",&p));
 ram=malloc(LB_RAM_SIZE);assert(ram);memset(ram,0xcc,LB_RAM_SIZE);
 assert(lb_load(b,sizeof b,rd,sizeof rd,"console=ttyS0",&w,&p));assert(!memcmp(ram+p.kernel_addr,b+2560,4096));assert(ram[p.kernel_addr+4096]==0);assert(!memcmp(ram+p.initrd_addr,rd,sizeof rd));assert(!strcmp((char*)ram+0x91000,"console=ttyS0"));assert(ram[0x901e8]==11);assert(get(ram+0x902d8,8)==0x1000);assert(get(ram+0x903a0,8)==LB_RAM_SIZE-0x100000);
 assert(lb_entry_prepare(&p,&w,&e));assert(e.cs==0x10&&e.ds==0x18&&e.rsi==0x90000&&e.rip==0x1000200);assert(get(ram+0x1000,8)==0x2003);assert(get(ram+0x2000,8)==0x3003);
 for(unsigned i=0;i<512;i++)assert(get(ram+0x3000+i*8,8)==(i<64?((uint64_t)i<<21)|0x83:0));
 assert(get(ram+0x4010,8)==0x00af9a000000ffffull);
 for(unsigned i=1;i<=5;i++){calls=0;fail=i;assert(!lb_load(b,sizeof b,rd,sizeof rd,"x",&w,&p));}
 for(unsigned i=1;i<=69;i++){calls=0;fail=i;assert(!lb_entry_prepare(&p,&w,&e));}
 free(ram);puts("PASS reconstructed Linux loader: bounds/layout/E820/identity paging/GDT; 74 writer failure positions; no VM executed");
}
