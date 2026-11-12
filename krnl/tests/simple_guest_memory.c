#include "../simple_guest.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define main image_tests_unused
#include "simple_guest_image.c"
#undef main
struct allocator { unsigned attempts, live, maps, wbs, releases; unsigned fail_alloc,fail_map,fail_wb,fail_free,bad_address,bad_map; void *blocks[68]; };
static uint64_t alloc(void *ctx) {
 struct allocator *a=ctx;unsigned n=++a->attempts;
 if(n==a->fail_alloc) return 0;
 assert(n<68);a->blocks[n]=aligned_alloc(4096,4096);assert(a->blocks[n]);memset(a->blocks[n],0xcc,4096);++a->live;
 return (uint64_t)n*4096+(n==a->bad_address?1:0);
}
static bool release(void *ctx,uint64_t p) { struct allocator *a=ctx;++a->releases;if(a->fail_free==p/4096)return false;assert(a->blocks[p/4096]);free(a->blocks[p/4096]);a->blocks[p/4096]=0;--a->live;return true; }
static void *map(void *ctx,uint64_t p) { struct allocator *a=ctx;if(++a->maps==a->fail_map)return 0;return (unsigned char *)a->blocks[p/4096]+(a->maps==a->bad_map?1:0); }
static bool wb(void *ctx,uint64_t p) { struct allocator *a=ctx;(void)p;return ++a->wbs!=a->fail_wb; }
static struct sg_memory_ops ops(struct allocator *a) { return (struct sg_memory_ops){a,alloc,release,map,wb}; }
static unsigned char *ram(struct allocator *a,struct sg_memory *m,unsigned i) {return a->blocks[m->pages[i]/4096];}
static void verify(void) {
 struct allocator a={0};struct sg_memory m={0};struct sg_memory_ops o=ops(&a);fixture();
 /* Add an RW file segment and a read-only zero-filled segment. */
 put(56,3,2);memcpy(elf+120,elf+64,56);put(124,6,4);put(128,12288,8);put(136,0x20000,8);put(144,0x20000,8);put(152,100,8);put(160,5000,8);
 memcpy(elf+176,elf+120,56);put(180,4,4);put(192,0x30000,8);put(200,0x30000,8);put(208,0,8);
 assert(sg_memory_prepare(&m,&o,elf,sizeof elf));assert(a.live==67 && m.prepared && !m.pages[0]);assert((m.eptp&4095)==0x1e);
 for(unsigned i=0;i<4098;++i)assert(ram(&a,&m,(0x10000+i)/4096)[i%4096]==elf[4096+i]);
 for(unsigned i=4098;i<8193;++i)assert(!ram(&a,&m,(0x10000+i)/4096)[i%4096]);
 uint64_t *pte=(uint64_t *)ram(&a,&m,4),*ept=a.blocks[m.tables[3]/4096];assert(!pte[0]&&!ept[0]);
 for(unsigned i=1;i<64;++i) {
 unsigned perm=(i>=16&&i<=18)?5:(i==6 || (i>=48&&i<=49))?1:3;
 assert(m.permissions[i]==perm);assert((ept[i]&4095)==(0x30|perm));assert((ept[i]&UINT64_C(0x000ffffffffff000))==m.pages[i]);assert(!!(pte[i]>>63)==!(perm&4));assert(!!(pte[i]&2)==!!(perm&2));
 for(unsigned j=0;j<4;++j)assert(m.pages[i]!=m.tables[j]);
 }
 for(unsigned i=1;i<4;++i)assert(((uint64_t *)ram(&a,&m,i))[0]==((uint64_t)(i+1)*4096|3));
 for(unsigned i=0;i<3;++i)assert(((uint64_t *)a.blocks[m.tables[i]/4096])[0]==(m.tables[i+1]|7));
 struct sg_boot_record *b=(void *)ram(&a,&m,6);assert(b->magic==SG_BOOT_MAGIC&&b->version==1&&b->size==sizeof(*b)&&b->image_entry==0x10000&&b->stack_top==0x8000&&b->ram_bytes==SG_RAM_SIZE);
 assert(ram(&a,&m,5)[0x166]==104);assert(((uint64_t *)ram(&a,&m,5))[3]==UINT64_C(0x00008b0051000067));for(unsigned i=0x800;i<4096;++i)assert(!ram(&a,&m,5)[i]);
 assert(!sg_memory_prepare(&m,&o,elf,sizeof elf));assert(a.live==67);
 a.fail_free=2;assert(!sg_memory_release(&m,&o));assert(a.live==1&&m.pages[2]&&!m.prepared&&!m.eptp);a.fail_free=0;assert(sg_memory_release(&m,&o));assert(!a.live);
}
int main(int argc, char **argv) {
 if(argc>1) {
  FILE *f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));long len=ftell(f);assert(len>0&&len<=SG_MAX_IMAGE);rewind(f);
  unsigned char *input=malloc((size_t)len);assert(input);assert(fread(input,1,(size_t)len,f)==(size_t)len);assert(!fclose(f));
  struct allocator a={0};struct sg_memory m={0};struct sg_memory_ops o=ops(&a);
  assert(sg_memory_prepare(&m,&o,input,(size_t)len));assert(m.prepared&&sg_image_executable(&m.image,m.image.entry,1));
  for(unsigned i=0;i<m.image.count;++i) {
   const struct sg_segment *s=&m.image.segments[i];
   for(uint64_t j=0;j<s->memsz;++j) {
    uint64_t address=s->address+j;
    assert(ram(&a,&m,(unsigned)(address/4096))[address%4096]==(j<s->filesz?input[s->offset+j]:0));
   }
  }
  assert(sg_memory_release(&m,&o));assert(!a.live);free(input);puts("real ELF inspected and loaded: PASS");
 }
 verify();fixture();
 for(unsigned mode=0;mode<5;++mode)for(unsigned n=1;n<=67;++n) {
 struct allocator a={0};struct sg_memory m={0};struct sg_memory_ops o=ops(&a);
 if(mode==0)a.fail_alloc=n;
 if(mode==1)a.fail_map=n;
 if(mode==2)a.fail_wb=n;
 if(mode==3)a.bad_address=n;
 if(mode==4)a.bad_map=n;
 assert(!sg_memory_prepare(&m,&o,elf,sizeof elf));assert(!a.live);assert(!m.prepared);
 }
 for(unsigned n=1;n<=67;++n) {struct allocator a={0};struct sg_memory m={0};struct sg_memory_ops o=ops(&a);a.fail_map=67;a.fail_free=n;assert(!sg_memory_prepare(&m,&o,elf,sizeof elf));assert(a.live==1);a.fail_free=0;assert(sg_memory_release(&m,&o));assert(!a.live);}
 struct allocator a={0};struct sg_memory m={0};struct sg_memory_ops o=ops(&a);fixture();put(56,2,2);put(120,2,4);assert(!sg_memory_prepare(&m,&o,elf,sizeof elf));assert(!a.attempts);
 puts("simple guest memory: PASS (335 injected allocation/map/WB/address failures, 67 retained-free retries)");return 0;
}
