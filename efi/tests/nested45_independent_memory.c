#define main legacy_memory_main
#include "efi/tests/nested41c_memory.c"
#undef main
#include "nested_atomic_catalog.h"
static bool cache(void *c,struct nac_cache_identity id){return !memcmp(c,&id,sizeof id);}
static bool source(unsigned p){return (p>=16&&p<=22)||(p>=24&&p<=27);}
int main(void){
 for(unsigned c=0;c<2;c++)for(unsigned slot=0;slot<3;slot++){
  base(&d);memset(&m,0,sizeof m);ram[c*32+3][slot==0?15:slot==1?48:49]^=0x1000;
  static uint64_t snapshot[NAM_OWNED_PAGES+10][512];memcpy(snapshot,ram,sizeof ram);
  assert(!nested_atomic_memory_prepare_1g(&m,&d,id)&&!memcmp(snapshot,ram,sizeof ram));
 }
 base(&d);memset(&m,0,sizeof m);assert(nested_atomic_memory_prepare_1g(&m,&d,id)&&nested_atomic_memory_ready(&m));
 assert(allocated==NAM_OWNED_PAGES+10&&m.catalog.span_count==63&&m.catalog.atomic_1g&&m.catalog.atomic_2m);
 assert(ram[18][29]==0x200073&&ram[18][31]==0x3ffff073&&ram[26][29]==0x200073&&ram[26][31]==0x3ffff073);
 for(unsigned c=0;c<2;c++)assert(ram[c*32+3][15]==0x4000f063&&ram[c*32+3][48]==0x40200063&&ram[c*32+3][49]==0x7ffff063);
 struct ad_entry_stamp stamp={1,2,3,5,4};struct ad_updater_gate gate;assert(ad_updater_gate_init(&gate,stamp));struct nac_cache_identity ci={0,0x900000,0x90005e,66,1};
 for(unsigned root=0;root<3;root++){
  struct nac_permit p={0};struct nac_ticket ticket;assert(nac_begin(&m.catalog,m.roots[root],&m,&gate,stamp,ci,cache,&ci,&p,&ticket));
  for(unsigned n=1;n<64;n++){
   struct nac_trace t;struct nested_ept_atomic_result r=nac_walk(&p,ticket,n*4096+4095,1,&t);unsigned actual=root==2&&n==15?28:n;
   uint64_t expected=actual==29?0x200000:actual==31?0x3ffff000:actual*4096;
   assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&r.translation.l1_gpa==expected+4095&&r.translation.hpa==d.backing.pages[actual-1]+4095&&t.path.count==4&&nac_trace_guard(&p,ticket,t.gpa,&t));
   if(source(n)){r=nac_walk(&p,ticket,n*4096,2,&t);assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed);}
   if(n==6)assert(r.translation.effective02==5);
  }
  const uint64_t far[]={0x4000f000,0x40200000,0x7ffff000};
  for(unsigned n=0;n<3;n++){
   struct nac_trace t;struct nested_ept_atomic_result r=nac_walk(&p,ticket,far[n]+4095,1,&t);unsigned actual=n==0?(root==2?28:15):n==1?29:31;
   assert(r.status==NEA_DONE&&r.translation.hpa==d.backing.pages[actual-1]+4095&&t.path.count==(root==2?4u:2u)&&nac_trace_guard(&p,ticket,t.gpa,&t));
  }
  if(root<2){
   for(unsigned n=1;n<64;n++){
    struct nac_trace t;struct nested_ept_atomic_result r=nac_walk(&p,ticket,0x40000000+n*4096,1,&t);
    if(n==29||n==31)assert(r.status==NEA_BACKING&&!t.complete);
    else {assert(r.status==NEA_DONE&&r.translation.hpa==d.backing.pages[n-1]&&t.path.count==2);if(source(n)){r=nac_walk(&p,ticket,0x40000000+n*4096,2,&t);assert(r.status==NEA_BACKING&&!r.changed);}}
   }
   const uint64_t holes[]={0x40000000,0x40040000,0x401ff000,0x40201000,0x7fffe000};
   for(unsigned n=0;n<sizeof holes/sizeof *holes;n++){struct nac_trace t;struct nested_ept_atomic_result r=nac_walk(&p,ticket,holes[n],1,&t);assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed&&!t.complete);}
  }
  assert(nac_complete(&p,ticket));
 }
 assert(nested_atomic_memory_ready(&m));
 for(unsigned r=0;r<5;r++){
  const uint64_t *roots=r==0?d.backing.tables:(r&1?d.root01[(r-1)/2]:d.root02[(r-1)/2]);uint64_t *pt=paging_phys_to_virt(roots[3]);
  for(unsigned n=1;n<64;n++)if(source(n)){pt[n]|=2;assert(!nested_atomic_memory_ready(&m));pt[n]&=~UINT64_C(2);}
  for(unsigned extra=0;extra<2;extra++){
   uint64_t *v=paging_phys_to_virt(m.sparse_tables[r*2+extra]);unsigned idx=extra?511:0;
   uint64_t saved=v[idx];v[idx]=d.backing.pages[15]|0x73;assert(!nested_atomic_memory_ready(&m));v[idx]=saved;
   v[42]=d.backing.pages[15]|0x73;assert(!nested_atomic_memory_ready(&m));v[42]=0;
  }
 }
 for(unsigned n=0;n<10;n++){uint64_t saved=m.sparse_tables[n];m.sparse_tables[n]=m.owned[15];assert(!nested_atomic_memory_ready(&m));m.sparse_tables[n]=saved;}
 assert(nested_atomic_memory_ready(&m));
 puts("PASS N45 independent actual fixture low/far source paths, relocated29/31 coverage, all source aliases RO, holes and physical ownership audit");return 0;
}
