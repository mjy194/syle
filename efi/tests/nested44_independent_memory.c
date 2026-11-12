#define main old_memory_fixture_main
#include "efi/tests/nested41c_memory.c"
#undef main
#include "nested_atomic_catalog.h"
static bool cache(void *c,struct nac_cache_identity id){return !memcmp(c,&id,sizeof id);}
int main(void){base(&d);memset(&m,0,sizeof m);assert(nested_atomic_memory_prepare_2m(&m,&d,id)&&nested_atomic_memory_ready(&m)&&m.large_2m&&m.catalog.atomic_2m);
 for(unsigned c=0;c<2;c++){assert(ram[c*32+3][15]==0x20f063&&ram[c*32+3][48]==0x21d063);}
 assert(ram[17][1]==0xf7&&ram[25][1]==0x1b007);assert(ram[13][0]==0&&ram[45][0]==0); /* VMCS12 storage untouched here. */
 struct ad_entry_stamp stamp={1,2,3,5,4};struct ad_updater_gate gate;assert(ad_updater_gate_init(&gate,stamp));
 struct nac_cache_identity ci={0,0x900000,0x90005e,66,1};
 for(unsigned root=0;root<3;root++){struct nac_permit p={0};struct nac_ticket t;assert(nac_begin(&m.catalog,m.roots[root],&m,&gate,stamp,ci,cache,&ci,&p,&t));
  for(unsigned n=1;n<64;n++){struct nac_trace trace;uint64_t gpa=0x200000+n*4096+0xfff;struct nested_ept_atomic_result r=nac_walk(&p,t,gpa,1,&trace);unsigned actual=(root==2&&n==15)?28:n;
   assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&trace.complete&&trace.path.count==(root==2?4u:3u));
   assert(r.translation.l1_gpa==actual*4096+0xfff&&r.translation.hpa==d.backing.pages[actual-1]+0xfff);
   assert(r.translation.effective02==m.spans[actual-1].permissions&&nac_trace_guard(&p,t,gpa,&trace));
   if(m.spans[actual-1].role==NRC_TABLE){r=nac_walk(&p,t,gpa,2,&trace);assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed);}}
  struct nac_trace trace;assert(nac_walk(&p,t,0x200000,1,&trace).status==(root==2?NEA_DONE:NEA_BACKING));
  struct nested_ept_atomic_result r=nac_walk(&p,t,0x240000,1,&trace);assert(r.status==(root==2?NEA_DONE:NEA_BACKING));
  assert(nac_complete(&p,t));}
 assert(nested_atomic_memory_ready(&m));
 __atomic_fetch_or(&ram[17][1],0x200,__ATOMIC_RELAXED);assert(nested_atomic_memory_ready(&m));
 __atomic_fetch_or(&ram[17][0],0x200,__ATOMIC_RELAXED);assert(!nested_atomic_memory_ready(&m));__atomic_fetch_and(&ram[17][0],~UINT64_C(0x200),__ATOMIC_RELAXED);
 __atomic_fetch_or(&ram[17][1],0x1000,__ATOMIC_RELAXED);assert(!nested_atomic_memory_ready(&m));__atomic_fetch_and(&ram[17][1],~UINT64_C(0x1000),__ATOMIC_RELAXED);
 ram[d.root01[1][3]/4096-1][18]|=2;assert(!nested_atomic_memory_ready(&m));
 puts("PASS N44 independent actual fixture 189 offsets, source alias R-only, code RX, backing holes, separate secondary data and PS-aware inventory");return 0;}
