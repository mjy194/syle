#define _POSIX_C_SOURCE 200112L
#include "../virtio_blk/firmware_ram_adapter.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define COUNT (FWM_RAM_PAGES+FWM_FLASH_PAGES+FWM_TABLE_PAGES)
#define BASE UINT64_C(0x100000000)
struct heap {unsigned char*bytes;unsigned next,live;bool used[COUNT];uint64_t fail_pa;};
static unsigned idx(uint64_t p){assert(p>=BASE&&!(p&4095)&&(p-BASE)/4096<COUNT);return (p-BASE)/4096;}
static uint64_t alloc(void*x){struct heap*h=x;assert(h->next<COUNT);unsigned i=h->next++;h->used[i]=true;h->live++;return BASE+(uint64_t)i*4096;}
static bool release(void*x,uint64_t p){struct heap*h=x;assert(h->used[idx(p)]);h->used[idx(p)]=false;h->live--;return true;}
static void*map(void*x,uint64_t p){struct heap*h=x;assert(h->used[idx(p)]);return p==h->fail_pa?NULL:h->bytes+p-BASE;}
static bool wb(void*x,uint64_t p){return ((struct heap*)x)->used[idx(p)];}
static void put(struct firmware_memory*m,uint64_t a,uint64_t v,unsigned n){uint8_t b[8];for(unsigned i=0;i<n;i++)b[i]=v>>(8*i);assert(fwm_write_ram(m,a,b,n));}
static uint64_t get(struct firmware_memory*m,uint64_t a,unsigned n){uint8_t b[8];assert(fwm_read_ram(m,a,b,n));uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)b[i]<<(8*i);return v;}
static void desc(struct firmware_memory*m,unsigned i,uint64_t a,unsigned n,unsigned flags,unsigned next){uint64_t d=4096+16*i;put(m,d,a,8);put(m,d+8,n,4);put(m,d+12,flags,2);put(m,d+14,next,2);}
static bool out(struct vrt*t,unsigned off,unsigned width,uint32_t v){return vrt_io(t,t->bar+off,width,true,&v);}
static void init(struct firmware_memory*m,struct vfr*a,struct vrt*t,uint8_t*disk){
 vfr_abort(a);memset(t,0,sizeof *t);uint8_t zero[8192]={0};assert(fwm_write_ram(m,4096,zero,sizeof zero));struct vro_io io;assert(vfr_bind(a,m,&io)&&vrt_init(t,io,disk,8192));
 uint32_t v=UINT32_MAX;assert(vrt_config(t,16,4,true,&v));v=0;assert(vrt_config(t,16,4,false,&v)&&v==0xffffffc1);v=0xc041;assert(vrt_config(t,16,4,true,&v));v=5;assert(vrt_config(t,4,2,true,&v));assert(out(t,18,1,3)&&out(t,8,4,1)&&out(t,18,1,7));
}
static void submit(struct firmware_memory*m,unsigned kind,uint64_t sector){
 put(m,6144,1,2);put(m,6146,1,2);put(m,16384,kind,4);put(m,16392,sector,8);
 desc(m,0,16384,16,1,1);desc(m,1,24570,512,kind==1?1:3,2);desc(m,2,28672,1,2,0);put(m,28672,99,1);
 uint8_t b[512];memset(b,0xcc,sizeof b);assert(fwm_write_ram(m,24570,b,sizeof b));
}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 uint8_t code[4096]={0},vars[4096]={0},disk[8192];for(unsigned i=0;i<sizeof disk;i++)disk[i]=i*17+i/512;
 struct sg_memory_ops ops={h,alloc,release,map,wb};assert(fwm_prepare(m,&ops,code,sizeof code,vars,sizeof vars,36));struct vfr a={0};struct vrt t={0};uint8_t result[512],saved[8192];memcpy(saved,disk,sizeof disk);
 init(m,&a,&t,disk);submit(m,0,15);assert(out(&t,16,2,0));assert(fwm_read_ram(m,24570,result,512)&&!memcmp(result,disk+15*512,512));assert(get(m,28672,1)==0&&get(m,8194,2)==1&&get(m,8200,4)==513&&a.publications==1);
 init(m,&a,&t,disk);submit(m,1,0);assert(out(&t,16,2,0)&&get(m,28672,1)==1&&!memcmp(saved,disk,sizeof disk));
 init(m,&a,&t,disk);submit(m,0,16);assert(out(&t,16,2,0)&&get(m,28672,1)==1&&get(m,24570,1)==0xcc);
 init(m,&a,&t,disk);submit(m,0,0);desc(m,1,4096,512,3,2);assert(!out(&t,16,2,0)&&t.failed&&get(m,28672,1)==99);
 init(m,&a,&t,disk);submit(m,0,0);desc(m,1,24570,512,3,0);assert(!out(&t,16,2,0)&&get(m,8194,2)==0);
 init(m,&a,&t,disk);submit(m,0,0);h->fail_pa=m->ram[6];assert(!out(&t,16,2,0)&&t.failed&&!a.active);h->fail_pa=0;assert(fwm_read_ram(m,24570,result,512)&&!memcmp(result,disk,6));assert(result[6]==0xcc&&get(m,8194,2)==0&&a.publications==0);assert(!out(&t,16,2,0));
 init(m,&a,&t,disk);submit(m,0,0);put(m,6144,0,2);assert(!out(&t,16,2,0)&&t.failed); /* no IRQ may be discarded */
 init(m,&a,&t,disk);submit(m,0,0);put(m,6146,2,2);assert(!out(&t,16,2,0)); /* pilot refuses batches */
 init(m,&a,&t,disk);submit(m,0,0);desc(m,1,m->layout.vars.base,512,3,2);assert(!out(&t,16,2,0));
 vfr_abort(&a);assert(fwm_release(m)&&!h->live);assert(!t.io.admit(&a,0,1,true));free(h->bytes);free(h);free(m);puts("virtio block actual owned RAM / PCI transport / RO reads / boundary and partial-store failure PASS (host)");
}
