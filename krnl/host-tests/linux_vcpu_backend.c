#include "../linux_vcpu_backend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put(uint8_t*p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(8*i));}
static void reject(const uint8_t*g,size_t n,uint16_t lim,uint16_t sel){
 uint32_t ar=1,l=2;uint64_t b=3;assert(!lvb_descriptor(g,n,lim,sel,&ar,&l,&b));assert(ar==1&&l==2&&b==3);
}
int main(void){
 uint8_t g[48]={0};uint32_t a,l;uint64_t b;
 put(g+8,0x00af9b000000ffffull);put(g+16,0x00cf93000000ffffull);
 put(g+24,0x00008b3456780067ull);put(g+32,0x1234);
 assert(lvb_descriptor(g,sizeof g,47,8,&a,&l,&b)&&a==0xa09b&&l==UINT32_MAX&&b==0);
 assert(lvb_descriptor(g,sizeof g,47,16,&a,&l,&b)&&a==0xc093&&l==UINT32_MAX&&b==0);
 assert(lvb_descriptor(g,sizeof g,47,24,&a,&l,&b)&&a==0x8b&&l==103&&b==0x123400345678ull);
 reject(g,sizeof g,47,0);reject(g,sizeof g,47,9);reject(g,sizeof g,47,12);
 reject(g,sizeof g,14,8);reject(g,15,47,8);reject(g,sizeof g,38,24);reject(g,39,47,24);
 put(g+32,1ull<<32);reject(g,sizeof g,47,24);reject(NULL,48,47,8);
 for(unsigned n=0;n<48;n++){
  if(n<16)reject(g,n,47,8);
  if(n<24)reject(g,n,47,16);
 }
 puts("linux_vcpu_backend: descriptor bounds/layout/atomic rejection PASS (no VM)");
}
