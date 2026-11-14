#include "transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t mem[32768],disk[1024];
static bool admit(void *p,uint64_t a,size_t n,bool w){(void)p;(void)w;return a<=sizeof(mem)&&n<=sizeof(mem)-a;}
static bool rd(void *p,uint64_t a,void *d,size_t n){if(!admit(p,a,n,false))return false;memcpy(d,mem+a,n);return true;}
static bool wr(void *p,uint64_t a,const void *d,size_t n){if(!admit(p,a,n,true))return false;memcpy(mem+a,d,n);return true;}
static void barrier(void *p){(void)p;}
static struct vrt init(void){struct vrt s;struct vro_io io={0,admit,rd,wr,barrier};assert(vrt_init(&s,io,disk,sizeof(disk)));uint32_t v=0xc001;assert(vrt_config(&s,16,4,true,&v));v=5;assert(vrt_config(&s,4,2,true,&v));return s;}
static bool out(struct vrt *s,unsigned o,unsigned w,uint32_t v){return vrt_io(s,(uint16_t)(0xc000+o),w,true,&v);}
static uint32_t in(struct vrt *s,unsigned o,unsigned w){uint32_t v;assert(vrt_io(s,(uint16_t)(0xc000+o),w,false,&v));return v;}
static void ready(struct vrt *s){assert(out(s,18,1,1));assert(out(s,18,1,3));assert(out(s,4,4,0));assert(out(s,8,4,1));assert(out(s,18,1,7));}
static void put(unsigned a,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)mem[a+i]=(uint8_t)(v>>(8*i));}
static void prepare(void){memset(mem,0,sizeof(mem));put(6144,1,2);put(6146,1,2);put(4096,16384,8);put(4104,16,4);put(4108,1,2);put(4110,1,2);put(4112,17408,8);put(4120,512,4);put(4124,3,2);put(4126,2,2);put(4128,19456,8);put(4136,1,4);put(4140,2,2);mem[19456]=99;}
int main(void){struct vrt s=init();uint32_t v=0;assert(vrt_config(&s,0,4,false,&v)&&v==0x10011af4);assert(vrt_config(&s,8,4,false,&v)&&v==0x01000000);assert(vrt_config(&s,44,4,false,&v)&&v==0x00021af4);
 v=UINT32_MAX;assert(vrt_config(&s,16,4,true,&v));assert(vrt_config(&s,16,4,false,&v)&&v==0xffffffc1);v=0xc001;assert(vrt_config(&s,16,4,true,&v));assert(in(&s,0,4)==32&&in(&s,12,2)==128&&in(&s,20,4)==2&&in(&s,24,4)==0);
 assert(out(&s,14,2,1));assert(in(&s,12,2)==0&&in(&s,8,4)==0);assert(out(&s,14,2,0));ready(&s);
 prepare();assert(out(&s,16,2,0));assert(mem[19456]==0&&mem[8194]==1&&s.backend.index==1);assert(in(&s,19,1)==0);assert(out(&s,18,1,0));assert(!s.pfn&&!s.features&&!s.status&&s.command==5&&s.bar==0xc000);
 s=init();assert(!out(&s,18,1,7)&&s.failed);assert(!out(&s,18,1,0));
 s=init();assert(out(&s,18,1,3));assert(!out(&s,4,4,1));
 s=init();assert(out(&s,18,1,3));assert(out(&s,4,4,32));assert(out(&s,8,4,1));assert(out(&s,18,1,7));assert(!out(&s,4,4,0));
 s=init();ready(&s);v=1;assert(vrt_config(&s,4,2,true,&v));assert(!out(&s,16,2,0));
 s=init();ready(&s);assert(out(&s,18,1,135));assert(!out(&s,16,2,0));
 s=init();assert(!out(&s,14,4,0));
 s=init();v=0x10001;assert(!vrt_config(&s,16,4,true,&v));
 s=init();v=0;assert(vrt_config(&s,4,2,true,&v));assert(!vrt_io(&s,0xc000,4,false,&v));
 s=init();prepare();assert(out(&s,18,1,3));assert(out(&s,8,4,1));assert(out(&s,16,2,0)&&s.started&&s.features==0&&s.status==3);assert(out(&s,18,1,7));assert(!out(&s,4,4,0));
 s=init();prepare();put(16384,1,4);put(4124,1,2);assert(out(&s,18,1,3));assert(out(&s,8,4,1));assert(out(&s,16,2,0)&&mem[19456]==1);assert(!out(&s,8,4,1));
 s=init();prepare();ready(&s);assert(out(&s,16,2,0));assert(out(&s,18,1,0)&&!s.started);prepare();ready(&s);assert(out(&s,16,2,0)&&s.backend.index==1);
 s=init();assert(out(&s,18,1,131));assert(out(&s,18,1,0)&&!s.failed);ready(&s);
 s=init();assert(out(&s,18,1,3));assert(!out(&s,8,4,UINT32_MAX));
 s=init();v=0xd001;assert(vrt_config(&s,16,4,true,&v));assert(vrt_io(&s,0xd000,4,false,&v)&&v==32);
 s=init();v=0xffc1;assert(vrt_config(&s,16,4,true,&v));assert(vrt_io(&s,0xffd4,4,false,&v)&&v==2);
 s=init();v=0xff;assert(vrt_config(&s,5,1,true,&v));assert(s.command==0x405);v=0xff;assert(vrt_config(&s,4,1,true,&v));assert(s.command==0x405);
 s=init();assert(in(&s,20,1)==2&&in(&s,21,2)==0&&in(&s,21,4)==0&&in(&s,27,1)==0);assert(!vrt_io(&s,0xc01b,2,false,&v));
 s=init();v=0xc001;assert(!vrt_config(&s,16,2,true,&v));
 puts("virtio legacy transport host pilot: PASS (no PCI/guest integration)");return 0;
}
