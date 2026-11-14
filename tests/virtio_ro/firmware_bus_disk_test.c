#define _POSIX_C_SOURCE 200112L
#include "firmware_ram_adapter.h"
#include "firmware_pci_bus.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef BUS_INIT
#define BUS_INIT vfb_init
#endif
#define COUNT (FWM_RAM_PAGES+FWM_FLASH_PAGES+FWM_TABLE_PAGES)
#define BASE UINT64_C(0x100000000)
struct heap {unsigned char*bytes;unsigned next,live;bool used[COUNT];uint64_t fail_pa;};
static unsigned idx(uint64_t p){assert(p>=BASE&&!(p&4095)&&(p-BASE)/4096<COUNT);return (unsigned)((p-BASE)/4096);}
static uint64_t alloc(void*x){struct heap*h=x;assert(h->next<COUNT);unsigned i=h->next++;assert(!h->used[i]);h->used[i]=true;h->live++;return BASE+(uint64_t)i*4096;}
static bool release(void*x,uint64_t p){struct heap*h=x;unsigned i=idx(p);assert(h->used[i]);h->used[i]=false;h->live--;return true;}
static void*map(void*x,uint64_t p){struct heap*h=x;assert(h->used[idx(p)]);return p==h->fail_pa?NULL:h->bytes+p-BASE;}
static bool wb(void*x,uint64_t p){struct heap*h=x;return h->used[idx(p)];}
static void put(struct firmware_memory*m,uint64_t a,uint64_t v,unsigned n){unsigned char b[8];for(unsigned i=0;i<n;i++)b[i]=(unsigned char)(v>>(8*i));assert(fwm_write_ram(m,a,b,n));}
static uint64_t get(struct firmware_memory*m,uint64_t a,unsigned n){unsigned char b[8];assert(fwm_read_ram(m,a,b,n));uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)b[i]<<(i*8);return v;}
static void desc(struct firmware_memory*m,unsigned i,uint64_t a,unsigned n,unsigned flags,unsigned next){uint64_t d=4096+16*i;put(m,d,a,8);put(m,d+8,n,4);put(m,d+12,flags,2);put(m,d+14,next,2);}
static bool commit_fail;static unsigned commits;static uint64_t expected;
static bool commit(void*x,uint32_t f,uint64_t v){(void)x;assert(f==0x681e&&v==expected);commits++;return !commit_fail;}
static bool port(struct vfb*t,unsigned p,unsigned n,bool input,uint64_t*v){
 unsigned z=n==4?3:n==2?1:0,len=0;uint8_t bytes[2];if(n==2)bytes[len++]=0x66;bytes[len++]=(uint8_t)(0xec+(input?0:2)+(n==1?0:1));uint64_t g[15]={0};g[0]=*v;g[3]=p;expected=0x1000+len;
 bool ok=vfb_io(t,g,0x1000,0,((uint64_t)p<<16)|z|(input?8:0),len,1,0,bytes,len,true,0,commit);if(ok)*v=g[0];return ok;
}
static bool out(struct vfb*t,unsigned off,unsigned n,uint32_t v){uint64_t x=v;return port(t,t->disk.bar+off,n,false,&x);}
static void config(struct vfb*t,unsigned off,unsigned n,uint32_t v){uint64_t x=0x80001000|(off&~3u);assert(port(t,0xcf8,4,false,&x));x=v;assert(port(t,0xcfc+(off&3),n,false,&x));}
static void init(struct firmware_memory*m,struct vfr*a,struct vfb*t,const unsigned char*disk,size_t size){
 vfr_abort(a);vfb_abort(t); /* synchronous fixture controller tears old device down */
 memset(t,0,sizeof*t);commit_fail=false;
 unsigned char zero[8192]={0};assert(fwm_write_ram(m,4096,zero,sizeof zero));struct vro_io io;assert(vfr_bind(a,m,&io)&&BUS_INIT(t,io,disk,size,1,0,UINT64_MAX));
 uint64_t x=0x80000000;assert(port(t,0xcf8,4,false,&x));assert(port(t,0xcfc,4,true,&x)&&x==0x12378086);
 x=0x80001000;assert(port(t,0xcf8,4,false,&x));assert(port(t,0xcfc,4,true,&x)&&x==0x10011af4);
 config(t,16,4,0xd001);config(t,4,2,5);assert(out(t,18,1,3)&&out(t,8,4,1));
 x=0x80000b40;assert(port(t,0xcf8,4,false,&x));assert(port(t,0xcfc,4,true,&x)&&x==1); /* same actual PIIX latch */
}
static void submit(struct firmware_memory*m,unsigned type,uint64_t sector){
 put(m,6144,1,2);put(m,6146,1,2);put(m,16384,type,4);put(m,16392,sector,8);
 desc(m,0,16384,16,1,1);desc(m,1,24570,512,type==1?1:3,2);desc(m,2,28672,1,2,0);put(m,28672,99,1);
 unsigned char b[512];memset(b,0xcc,sizeof b);assert(fwm_write_ram(m,24570,b,sizeof b));
}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[4096]={0},vars[4096]={0};unsigned char*disk=malloc(8192);assert(disk);for(unsigned i=0;i<8192;i++)disk[i]=(unsigned char)(i*17+(i/512));disk[510]=0x55;disk[511]=0xaa;
 struct sg_memory_ops ops={h,alloc,release,map,wb};assert(fwm_prepare(m,&ops,code,sizeof code,vars,sizeof vars,36));struct vfr a;struct vfb t;
 init(m,&a,&t,disk,8192);submit(m,0,0);assert(out(&t,16,2,0));unsigned char result[512];assert(fwm_read_ram(m,24570,result,512));assert(!memcmp(result,disk,512)&&get(m,28672,1)==0&&get(m,8194,2)==1&&get(m,8200,4)==513&&a.publications==1);
 /* Offered RO applies even without a guest feature write. */
 unsigned char saved[8192];memcpy(saved,disk,8192);init(m,&a,&t,disk,8192);submit(m,1,0);assert(out(&t,16,2,0)&&get(m,28672,1)==1&&!memcmp(saved,disk,8192));
 init(m,&a,&t,disk,8192);submit(m,0,16);assert(out(&t,16,2,0)&&get(m,28672,1)==1&&get(m,24570,1)==0xcc);
 init(m,&a,&t,disk,8192);submit(m,0,15);assert(out(&t,16,2,0));assert(fwm_read_ram(m,24570,result,512)&&!memcmp(result,disk+15*512,512));
 /* Real fwm page copier writes first six bytes then its next map fails. */
 init(m,&a,&t,disk,8192);submit(m,0,0);h->fail_pa=m->ram[6];assert(!out(&t,16,2,0)&&!a.active&&t.failed);h->fail_pa=0;
 assert(fwm_read_ram(m,24570,result,512));assert(!memcmp(result,disk,6));for(unsigned i=6;i<512;i++)assert(result[i]==0xcc);
 assert(get(m,28672,1)==99&&get(m,8194,2)==0&&a.publications==0);assert(!out(&t,16,2,0));
 init(m,&a,&t,disk,8192);assert(!t.disk.io.admit(&a,FWM_RAM_SIZE-1,2,true)&&!t.disk.io.admit(&a,m->layout.vars.base,1,true)&&!t.disk.io.admit(&a,UINT64_MAX,1,true));
 submit(m,0,0);desc(m,1,m->layout.vars.base,512,3,2);assert(!out(&t,16,2,0)&&get(m,28672,1)==99);
 init(m,&a,&t,disk,8192);submit(m,0,0);commit_fail=true;
 unsigned before=commits;assert(!out(&t,16,2,0)&&t.failed&&t.disk.failed&&commits==before+1);
 assert(get(m,28672,1)==0&&get(m,8194,2)==1&&a.publications==1);
 commit_fail=false;before=commits;assert(!out(&t,16,2,0)&&commits==before&&a.publications==1);
 vfr_abort(&a); /* controller revokes RAM after whole bus terminal */
 assert(fwm_release(m)&&!h->live);assert(!t.disk.io.admit(&a,0,1,true));free(disk);free(h->bytes);free(h);free(m);puts("actual firmware RAM + single CF8 bus + virtio disk: host-only PASS");
}
