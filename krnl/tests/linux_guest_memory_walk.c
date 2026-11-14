/* Independent four-level walk of the actual adapter output, with scattered
 * trusted backing. This is a host structural test, not a hardware EPT walk. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
/* Reuse fixture callbacks only. Renamed original main is never called. */
#define main existing_memory_contract
#include "linux_guest_memory.c"
#undef main
#pragma GCC diagnostic pop
static uint64_t scattered_alloc(void*x){
 struct heap*h=x;unsigned n=h->allocs++;assert(n<COUNT);
 /* 17 is coprime to COUNT: full permutation, not an identity/contiguous map. */
 unsigned i=(n*17u)%COUNT;assert(!h->used[i]);h->used[i]=true;h->live++;
 memset(h->bytes+(size_t)i*4096,0xcc,4096);return BASE+(uint64_t)i*4096;
}
static bool walk(struct heap*h,uint64_t root,uint64_t gpa,uint64_t*out){
 if(gpa>>48)return false;
 const unsigned shifts[]={39,30,21,12};uint64_t table=root&UINT64_C(0x000ffffffffff000);
 for(unsigned level=0;level<4;level++){
  uint64_t*entries=map(h,table);uint64_t entry=entries[(gpa>>shifts[level])&511];
  if(!(entry&7))return false;
  assert((entry&7)==7);assert(!(entry&(1u<<7))); /* no large pages */
  table=entry&UINT64_C(0x000ffffffffff000);
  if(level==3){assert((entry&0xfff)==0x37);*out=table|(gpa&4095);return true;}
 }
 return false;
}
int main(int argc,char**argv){
 (void)argc;(void)argv;
 struct heap*h=calloc(1,sizeof*h);struct linux_guest_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char image[8192]={0},initrd[131]={1,2,3};
 image[0x200]=0xeb;image[0x201]=0x6a;image[0x1f1]=4;put(image+0x1f4,4,(sizeof image-2560)/16);put(image+0x1fe,2,0xaa55);put(image+0x202,4,0x53726448);put(image+0x206,2,0x20f);image[0x211]=1;put(image+0x22c,4,0x7fffffff);put(image+0x230,4,0x200000);image[0x234]=1;put(image+0x236,2,1);put(image+0x238,4,2048);put(image+0x258,8,0x1000000);put(image+0x260,4,0x100000);
 struct sg_memory_ops o={h,scattered_alloc,release,map,wb};
 assert(lgm_prepare(m,&o,image,sizeof image,initrd,sizeof initrd,"console=ttyS0 rdinit=/init"));
 assert(h->live==COUNT&&m->eptp==(m->tables[0]|0x1e));
 bool seen[COUNT]={0};
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++){
  unsigned index=indexof(m->pages[i]);assert(!seen[index]);seen[index]=true;
  const unsigned offsets[]={0,1,2047,4095};
  for(unsigned j=0;j<sizeof offsets/sizeof*offsets;j++){
   uint64_t pa=0,gpa=(uint64_t)i*4096+offsets[j];assert(walk(h,m->eptp,gpa,&pa));
   assert(pa==m->pages[i]+offsets[j]);
  }
 }
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++){unsigned index=indexof(m->tables[i]);assert(!seen[index]);seen[index]=true;}
 for(unsigned i=0;i<COUNT;i++)assert(seen[i]);
 const uint64_t outside[]={LGM_RAM_SIZE,LGM_RAM_SIZE+4095,UINT64_C(1)<<30,
   UINT64_C(1)<<39,(UINT64_C(1)<<48)-1,UINT64_C(1)<<48,UINT64_MAX};
 for(unsigned i=0;i<sizeof outside/sizeof*outside;i++){uint64_t untouched=0x1234;assert(!walk(h,m->eptp,outside[i],&untouched));assert(untouched==0x1234);}
 assert(lgm_release(m)&&!h->live&&!m->prepared&&!m->eptp);
 free(h->bytes);free(h);free(m);
 puts("PASS actual Linux EPT structure: scattered backing, all page offsets, distinct trusted ownership, out-of-RAM holes; host only");return 0;
}
