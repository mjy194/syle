/* Targeted regression tests for reconstructed loader reservations/entry gate.
 * Deliberately separate from the prior unchanged full-suite run. */
#include "../linux_boot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char params[4096];static unsigned calls;
static void put(unsigned char*p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=v>>(8*i);}
static uint64_t get(const unsigned char*p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static bool write_cb(void*c,uint64_t a,const void*p,size_t n){(void)c;calls++;if(a==0x90000){assert(n==4096);memcpy(params,p,n);}return true;}
static bool zero_cb(void*c,uint64_t a,size_t n){(void)c;(void)a;(void)n;calls++;return true;}
int main(void){
 unsigned char image[8192]={0},rd[16]={0};
 image[0x1f1]=4;put(image+0x1f4,256,4);put(image+0x1fe,0xaa55,2);image[0x200]=0xeb;image[0x201]=0x6a;
 put(image+0x202,0x53726448,4);put(image+0x206,0x20c,2);image[0x211]=1;image[0x234]=1;put(image+0x236,1,2);put(image+0x230,0x200000,4);put(image+0x238,2048,4);put(image+0x22c,0x7fffffff,4);put(image+0x258,0x1000000,8);put(image+0x260,0x400000,4);
 struct lb_plan p;struct lb_entry_state e;struct lb_writer w={0,write_cb,zero_cb};
 assert(lb_load(image,sizeof image,rd,sizeof rd,"x",&w,&p));
 const uint64_t bounds[]={0,0x1000,0x5000,0x6000,0x7000,0x7f000,0x80000,0x90000,0x92000,0x9f000,0x100000,LB_RAM_SIZE};
 assert(params[0x1e8]==11);
 uint64_t total=0;
 for(unsigned i=0;i<11;i++){const unsigned char*r=params+0x2d0+20*i;assert(get(r,8)==bounds[i]);assert(get(r+8,8)==bounds[i+1]-bounds[i]);assert(get(r+16,4)==((i&1)?2:1));total+=get(r+8,8);}
 assert(total==LB_RAM_SIZE);assert(lb_entry_prepare(&p,&w,&e));assert(e.rsp==0x80000);
 struct lb_plan bad=p;
 for(unsigned i=0;i<7;i++){
  bad=p;switch(i){case 0:bad.kernel_envelope=0;break;case 1:bad.kernel_size=0;break;case 2:bad.kernel_size=0x200;break;case 3:bad.kernel_envelope=bad.kernel_size-1;break;case 4:bad.entry_addr++;break;case 5:bad.kernel_envelope=LB_RAM_SIZE;break;case 6:bad.kernel_addr=UINT64_MAX;break;}
  calls=0;assert(!lb_entry_prepare(&bad,&w,&e));assert(!calls);
 }
 puts("PASS NEW loader regression: 11 contiguous E820 ranges preserve low RAM and reserve bootstrap/stack/params; 7 invalid entry plans rejected before writes; no guest execution");
}
