#define _POSIX_C_SOURCE 200112L
#include "../firmware_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COUNT (FWM_RAM_PAGES+FWM_FLASH_PAGES+FWM_TABLE_PAGES)
#define BASE UINT64_C(0x100000000)
struct heap {unsigned char*bytes;bool used[COUNT];unsigned allocs,live,maps,wbs,releases;
 unsigned fail_alloc,fail_map,fail_wb,fail_free,duplicate_at,bad_at,unaligned_map;
 uint64_t bad_value;};
static unsigned idx(uint64_t p){assert(p>=BASE&&p<BASE+(uint64_t)COUNT*4096&&!(p&4095));return (unsigned)((p-BASE)/4096);}
static uint64_t alloc(void*x){struct heap*h=x;unsigned n=++h->allocs;
 if(n==h->fail_alloc)return 0;
 if(n==h->duplicate_at)return BASE;
 if(n==h->bad_at)return h->bad_value;
 assert(n<=COUNT&&!h->used[n-1]);h->used[n-1]=true;h->live++;return BASE+(uint64_t)(n-1)*4096;
}
static bool release(void*x,uint64_t p){struct heap*h=x;unsigned i=idx(p);assert(h->used[i]);h->releases++;if(i+1==h->fail_free)return false;h->used[i]=false;h->live--;return true;}
static void*map(void*x,uint64_t p){struct heap*h=x;unsigned i=idx(p);assert(h->used[i]);h->maps++;
 if(h->maps==h->fail_map)return 0;
 return h->bytes+(uint64_t)i*4096+(h->maps==h->unaligned_map?1:0);
}
static bool wb(void*x,uint64_t p){struct heap*h=x;assert(h->used[idx(p)]);return ++h->wbs!=h->fail_wb;}
static void reset(struct heap*h){assert(!h->live);unsigned char*b=h->bytes;memset(h,0,sizeof*h);h->bytes=b;}
static uint64_t leaf(struct firmware_memory*m,struct heap*h,uint64_t a){
 uint64_t t=m->tables[0];unsigned shifts[]={39,30,21,12};
 for(unsigned i=0;i<4;i++){uint64_t e=((uint64_t*)(h->bytes+(t-BASE)))[(a>>shifts[i])&511];if(!(e&7))return 0;if(i==3)return e;assert(!(e&128));t=e&~UINT64_C(4095);}assert(0);return 0;
}
static void verify(struct firmware_memory*m,struct heap*h,const unsigned char*code,size_t cn,const unsigned char*vars,size_t vn){
 assert(m->prepared&&!m->quarantined&&m->eptp==(m->tables[0]|0x1e));
 assert(m->flash_pages==(cn+vn)/4096&&h->live==FWM_RAM_PAGES+m->flash_pages+FWM_TABLE_PAGES);
 for(unsigned i=0;i<FWM_RAM_PAGES;i++){
  assert(leaf(m,h,(uint64_t)i*4096)==(m->ram[i]|0x37));assert(fwm_owned_page(m,m->ram[i]));
  unsigned char*p=h->bytes+(m->ram[i]-BASE);for(unsigned j=0;j<4096;j++)assert(!p[j]);
 }
 for(uint64_t a=FWM_RAM_SIZE;a<m->layout.vars.base;a+=4096)assert(!leaf(m,h,a));
 for(unsigned i=0;i<m->flash_pages;i++){
  uint64_t a=m->layout.vars.base+(uint64_t)i*4096;bool v=a<m->layout.code.base;
  assert(leaf(m,h,a)==(m->flash[i]|(v?0x31:0x35)));
  const unsigned char*src=v?vars+(uint64_t)i*4096:code+(uint64_t)i*4096-vn;
  assert(!memcmp(h->bytes+(m->flash[i]-BASE),src,4096));assert(fwm_owned_page(m,m->flash[i]));
 }
 assert(!leaf(m,h,FWL_TOP));
 for(unsigned i=0;i<FWM_TABLE_PAGES;i++)assert(fwm_owned_page(m,m->tables[i]));
 assert(!fwm_owned_page(m,0)&&!fwm_owned_page(m,m->ram[0]+1)&&!fwm_owned_page(m,BASE+(uint64_t)COUNT*4096));
}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=malloc(FWM_FLASH_MAX),*vars=malloc(FWM_FLASH_MAX);assert(code&&vars);
 for(unsigned i=0;i<FWM_FLASH_MAX;i++){code[i]=(unsigned char)(i*3+17);vars[i]=(unsigned char)(i*7+9);}
 struct sg_memory_ops o={h,alloc,release,map,wb};
 size_t cn=65536,vn=4096;assert(fwm_prepare(m,&o,code,cn,vars,vn,36));verify(m,h,code,cn,vars,vn);
 unsigned char in[32],out[32];memset(in,0x72,sizeof in);assert(fwm_write_ram(m,4090,in,sizeof in));assert(fwm_read_ram(m,4090,out,sizeof out));assert(!memcmp(in,out,sizeof in));
 assert(!fwm_write_ram(m,FWM_RAM_SIZE-1,in,2));assert(!fwm_read_ram(m,UINT64_MAX,out,1));assert(!fwm_write_ram(m,m->layout.vars.base,in,1));
 assert(!fwm_read_ram(m,0,out,0));assert(!fwm_write_ram(m,0,0,1));
 assert(!fwm_prepare(m,&o,code,cn,vars,vn,36));assert(fwm_release(m)&&!h->live);
 assert(!fwm_owned_page(m,BASE)&&!fwm_read_ram(m,0,out,1));assert(fwm_release(m));
 reset(h);cn=3653632;vn=540672;assert(fwm_prepare(m,&o,code,cn,vars,vn,36));verify(m,h,code,cn,vars,vn);assert(fwm_release(m)&&!h->live);
 cn=65536;vn=4096;unsigned total=FWM_RAM_PAGES+17+FWM_TABLE_PAGES;
 unsigned boundary[]={1,2,512,FWM_RAM_PAGES,FWM_RAM_PAGES+1,FWM_RAM_PAGES+17,FWM_RAM_PAGES+18,total};
 for(unsigned type=0;type<3;type++)for(unsigned j=0;j<sizeof boundary/sizeof*boundary;j++){
  reset(h);if(type==0)h->fail_alloc=boundary[j];if(type==1)h->fail_map=boundary[j];if(type==2)h->fail_wb=boundary[j];
  assert(!fwm_prepare(m,&o,code,cn,vars,vn,36));assert(!h->live&&!m->prepared&&!m->eptp&&!m->quarantined);
 }
 /* Fail table construction maps, after all allocation/zeroing maps succeeded. */
 for(unsigned j=1;j<=5;j++){reset(h);h->fail_map=total+j;assert(!fwm_prepare(m,&o,code,cn,vars,vn,36));assert(!h->live);}
 reset(h);h->unaligned_map=1;assert(!fwm_prepare(m,&o,code,cn,vars,vn,36));assert(!h->live);
 reset(h);assert(fwm_prepare(m,&o,code,cn,vars,vn,36));h->fail_free=1;
 assert(!fwm_release(m)&&h->live==1&&m->ram[0]==BASE&&!m->prepared&&!m->eptp);
 assert(fwm_owned_page(m,BASE)&&!fwm_owned_page(m,BASE+4096));
 assert(!fwm_prepare(m,&o,code,cn,vars,vn,36));h->fail_free=0;assert(fwm_release(m)&&!h->live);
 /* Failure during prepare plus failed unwind retains the unique live token. */
 reset(h);h->fail_alloc=2;h->fail_free=1;assert(!fwm_prepare(m,&o,code,cn,vars,vn,36));assert(h->live==1&&fwm_owned_page(m,BASE));h->fail_free=0;assert(fwm_release(m));
 /* Allocator violations quarantine before touching the returned page. */
 unsigned aliases[]={2,FWM_RAM_PAGES+1,FWM_RAM_PAGES+18};
 for(unsigned j=0;j<3;j++){reset(h);h->duplicate_at=aliases[j];assert(!fwm_prepare(m,&o,code,cn,vars,vn,36));assert(m->quarantined&&m->rejected_token==BASE&&h->maps==aliases[j]-1&&h->releases==0);assert(!fwm_release(m));
  /* Trusted mock teardown only; production cannot clear quarantine this way. */
  memset(m,0,sizeof*m);memset(h->used,0,sizeof h->used);h->live=0;
 }
 uint64_t invalid[]={BASE+1,UINT64_C(1)<<36,UINT64_MAX};
 for(unsigned j=0;j<3;j++){reset(h);h->bad_at=1;h->bad_value=invalid[j];assert(!fwm_prepare(m,&o,code,cn,vars,vn,36));assert(m->quarantined&&m->rejected_token==invalid[j]&&!h->maps&&!h->wbs&&!h->releases);memset(m,0,sizeof*m);}
 reset(h);assert(!fwm_prepare(m,&o,code,0,vars,vn,36));assert(!fwm_prepare(m,&o,code,cn+1,vars,vn,36));assert(!fwm_prepare(m,&o,code,FWM_FLASH_MAX,vars,vn,36));assert(!fwm_prepare(m,&o,code,cn,vars,vn,31));assert(!h->allocs);
 assert(!fwm_prepare(0,&o,code,cn,vars,vn,36)&&!fwm_prepare(m,0,code,cn,vars,vn,36));assert(!fwm_release(0));
 free(code);free(vars);free(h->bytes);free(h);free(m);
 puts("firmware_memory: low RAM/high flash EPT, permissions, holes, fault rollback, alias quarantine PASS (host only)");
}
