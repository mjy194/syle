#define _POSIX_C_SOURCE 200112L
#include "../linux_guest_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COUNT (LGM_PAGE_COUNT+LGM_TABLE_COUNT)
#define BASE UINT64_C(0x100000000)
struct heap {unsigned char*bytes;unsigned allocs,live,fail_alloc,fail_map,fail_wb,free_fail;bool used[COUNT];};
static unsigned indexof(uint64_t p){assert(p>=BASE&&p<BASE+(uint64_t)COUNT*4096&&!(p&4095));return (p-BASE)/4096;}
static uint64_t alloc(void*x){struct heap*h=x;unsigned n=++h->allocs;if(n==h->fail_alloc)return 0;assert(n<=COUNT);h->used[n-1]=true;h->live++;memset(h->bytes+(n-1)*4096,0xcc,4096);return BASE+(n-1)*4096;}
static bool release(void*x,uint64_t p){struct heap*h=x;unsigned i=indexof(p);assert(h->used[i]);if(i+1==h->free_fail)return false;h->used[i]=false;h->live--;return true;}
static void*map(void*x,uint64_t p){struct heap*h=x;unsigned i=indexof(p);assert(h->used[i]);return i+1==h->fail_map?0:h->bytes+i*4096;}
static bool wb(void*x,uint64_t p){struct heap*h=x;return indexof(p)+1!=h->fail_wb;}
static void put(unsigned char*p,unsigned n,uint64_t v){while(n--){*p++=v;v>>=8;}}
static void reset(struct heap*h){assert(!h->live);h->allocs=h->fail_alloc=h->fail_map=h->fail_wb=h->free_fail=0;}
static bool flatwrite(void*x,uint64_t a,const void*p,size_t n){if(a>LGM_RAM_SIZE||n>LGM_RAM_SIZE-a)return false;memcpy((char*)x+a,p,n);return true;}
static bool flatzero(void*x,uint64_t a,size_t n){if(a>LGM_RAM_SIZE||n>LGM_RAM_SIZE-a)return false;memset((char*)x+a,0,n);return true;}
static void*file(const char*path,size_t*n){FILE*f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long z=ftell(f);assert(z>0);*n=(size_t)z;rewind(f);void*p=malloc(*n);assert(p&&fread(p,1,*n,f)==*n);fclose(f);return p;}
int main(int argc,char**argv){
 struct heap*h=calloc(1,sizeof*h);struct linux_guest_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char image[8192]={0},initrd[131]={1,2,3};
 image[0x200]=0xeb;image[0x201]=0x6a;image[0x1f1]=4;put(image+0x1f4,4,(sizeof image-2560)/16);put(image+0x1fe,2,0xaa55);put(image+0x202,4,0x53726448);put(image+0x206,2,0x20f);image[0x211]=1;put(image+0x22c,4,0x7fffffff);put(image+0x230,4,0x200000);image[0x234]=1;put(image+0x236,2,1);put(image+0x238,4,2048);put(image+0x258,8,0x1000000);put(image+0x260,4,0x100000);
 for(unsigned i=2560;i<sizeof image;i++)image[i]=(unsigned char)i;
 const char*cmd="console=ttyS0 rdinit=/init";struct sg_memory_ops o={h,alloc,release,map,wb};
 assert(lgm_prepare(m,&o,image,sizeof image,initrd,sizeof initrd,cmd));assert(h->live==COUNT);
 unsigned char*expected=calloc(1,LGM_RAM_SIZE);assert(expected);struct lb_plan plan;struct lb_entry_state entry;struct lb_writer writer={expected,flatwrite,flatzero};
 assert(lb_load(image,sizeof image,initrd,sizeof initrd,cmd,&writer,&plan));assert(lb_entry_prepare(&plan,&writer,&entry));
 assert(!memcmp(expected,h->bytes,LGM_RAM_SIZE));free(expected);
 for(unsigned t=0;t<LGM_PT_COUNT;t++)for(unsigned j=0;j<512;j++)assert(((uint64_t*)map(h,m->tables[3+t]))[j]==(m->pages[t*512+j]|0x37));
 for(unsigned t=0;t<3;t++){uint64_t*p=map(h,m->tables[t]);unsigned used=t==2?64:1;for(unsigned j=used;j<512;j++)assert(p[j]==0);}
 assert(!lgm_prepare(m,&o,image,sizeof image,initrd,sizeof initrd,cmd));assert(lgm_release(m));assert(!h->live);
 unsigned boundaries[]={1,2,512,LGM_PAGE_COUNT,LGM_PAGE_COUNT+1,COUNT};
 for(unsigned type=0;type<3;type++)for(unsigned b=0;b<sizeof boundaries/sizeof*boundaries;b++){
  reset(h);if(type==0)h->fail_alloc=boundaries[b];if(type==1)h->fail_map=boundaries[b];if(type==2)h->fail_wb=boundaries[b];
  assert(!lgm_prepare(m,&o,image,sizeof image,initrd,sizeof initrd,cmd));assert(!h->live);assert(!m->prepared&&!m->eptp);
 }
 reset(h);assert(lgm_prepare(m,&o,image,sizeof image,initrd,sizeof initrd,cmd));h->free_fail=1;assert(!lgm_release(m));assert(h->live==1&&m->pages[0]);assert(!lgm_prepare(m,&o,image,sizeof image,initrd,sizeof initrd,cmd));h->free_fail=0;assert(lgm_release(m));assert(!h->live);
 reset(h);image[0x202]=0;assert(!lgm_prepare(m,&o,image,sizeof image,initrd,sizeof initrd,cmd));assert(h->allocs==0);
 if(argc==3){size_t kn,rn;void*k=file(argv[1],&kn),*r=file(argv[2],&rn);reset(h);
  assert(lgm_prepare(m,&o,k,kn,r,rn,cmd));assert(h->live==COUNT);
  assert(!memcmp(h->bytes+m->plan.initrd_addr,r,rn));
  assert(!memcmp(h->bytes+m->plan.kernel_addr,(char*)k+m->plan.payload_offset,m->plan.kernel_size));
  assert(m->entry.rip==m->plan.entry_addr&&m->entry.rsi==m->plan.params_addr);
  assert(lgm_release(m)&&!h->live);free(k);free(r);
  puts("PASS reconstructed adapter loading original-hash real Linux bzImage+initramfs; no guest execution");
 }
 free(h->bytes);free(h);free(m);puts("PASS reconstructed memory: all128MiB bytes, 32768 private EPT leaves,18 failure boundaries, retained-free retry; synthetic image only");
}
