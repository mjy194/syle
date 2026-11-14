#define _POSIX_C_SOURCE 200112L
#include "firmware_ram_adapter.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
static bool out(struct vrt*t,unsigned off,unsigned n,uint32_t v){return vrt_io(t,(uint16_t)(t->bar+off),n,true,&v);}
static void init(struct firmware_memory*m,struct vfr*a,struct vrt*t,const unsigned char*disk,size_t size){
 vfr_abort(a); /* fixture synchronously destroys prior device before rebinding */
 unsigned char zero[8192]={0};assert(fwm_write_ram(m,4096,zero,sizeof zero));
 struct vro_io io;assert(vfr_bind(a,m,&io)&&vrt_init(t,io,disk,size));uint32_t b=0xd001;assert(vrt_config(t,16,4,true,&b));b=5;assert(vrt_config(t,4,2,true,&b));assert(out(t,18,1,3)&&out(t,8,4,1));
}
static void submit(struct firmware_memory*m,unsigned type,uint64_t sector){
 put(m,6144,1,2);put(m,6146,1,2);put(m,16384,type,4);put(m,16392,sector,8);
 desc(m,0,16384,16,1,1);desc(m,1,24570,512,type==1?1:3,2);desc(m,2,28672,1,2,0);put(m,28672,99,1);
 unsigned char b[512];memset(b,0xcc,sizeof b);assert(fwm_write_ram(m,24570,b,sizeof b));
}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[4096]={0},vars[4096]={0};unsigned char*disk=malloc(8192);assert(disk);for(unsigned i=0;i<8192;i++)disk[i]=(unsigned char)(i*17+(i/512));disk[510]=0x55;disk[511]=0xaa;
 struct sg_memory_ops ops={h,alloc,release,map,wb};assert(fwm_prepare(m,&ops,code,sizeof code,vars,sizeof vars,36));struct vfr a;struct vrt t;
 init(m,&a,&t,disk,8192);submit(m,0,0);assert(out(&t,16,2,0));unsigned char result[512];assert(fwm_read_ram(m,24570,result,512));assert(!memcmp(result,disk,512)&&get(m,28672,1)==0&&get(m,8194,2)==1&&get(m,8200,4)==513&&a.publications==1);
 /* Offered RO applies even without a guest feature write. */
 unsigned char saved[8192];memcpy(saved,disk,8192);init(m,&a,&t,disk,8192);submit(m,1,0);assert(out(&t,16,2,0)&&get(m,28672,1)==1&&!memcmp(saved,disk,8192));
 init(m,&a,&t,disk,8192);submit(m,0,16);assert(out(&t,16,2,0)&&get(m,28672,1)==1&&get(m,24570,1)==0xcc);
 init(m,&a,&t,disk,8192);submit(m,0,15);assert(out(&t,16,2,0));assert(fwm_read_ram(m,24570,result,512)&&!memcmp(result,disk+15*512,512));
 /* Real fwm page copier writes first six bytes then its next map fails. */
 init(m,&a,&t,disk,8192);submit(m,0,0);h->fail_pa=m->ram[6];assert(!out(&t,16,2,0)&&!a.active&&t.failed);h->fail_pa=0;
 assert(fwm_read_ram(m,24570,result,512));assert(!memcmp(result,disk,6));for(unsigned i=6;i<512;i++)assert(result[i]==0xcc);
 assert(get(m,28672,1)==99&&get(m,8194,2)==0&&a.publications==0);assert(!out(&t,16,2,0));
 init(m,&a,&t,disk,8192);assert(!t.io.admit(&a,FWM_RAM_SIZE-1,2,true)&&!t.io.admit(&a,m->layout.vars.base,1,true)&&!t.io.admit(&a,UINT64_MAX,1,true));
 submit(m,0,0);desc(m,1,m->layout.vars.base,512,3,2);assert(!out(&t,16,2,0)&&get(m,28672,1)==99);
 init(m,&a,&t,disk,8192);submit(m,0,0);assert(out(&t,16,2,0));vfr_abort(&a);assert(!out(&t,16,2,0)); /* failed outer instruction commit: no replay */
 assert(fwm_release(m)&&!h->live);assert(!t.io.admit(&a,0,1,true));free(disk);free(h->bytes);free(h);free(m);puts("actual firmware RAM + legacy virtio disk: host-only PASS");
}
