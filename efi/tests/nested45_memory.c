/* Physical mapping probes mocked; production constructor/catalog/walker. */
#define main legacy_memory_main
#include "nested41c_memory.c"
#undef main
#include "../../krnl/nested_ept_atomic.h"
#include "../../krnl/ad_updater_gate.h"
static struct nrc_session session;
static uint64_t source_nonce,updater_nonce;
static struct ad_updater_gate gate;
static struct ad_updater_token token;
static const struct ad_entry_stamp stamp={1,2,3,1,4};
static bool guarded(void *p,struct ad_entry_stamp s){(void)p;return nrc_guard(&session,source_nonce)&&ad_updater_gate_guard(&gate,&token,s,updater_nonce);}
static bool table(void *p,uint64_t g,uint64_t *h,uint64_t **v){(void)p;struct nrc_atomic_page a;if(!nrc_resolve_atomic(&session,source_nonce,g,1,&a)||!a.pinned_writable)return false;*h=a.backing.hpa;*v=a.atomic_view;return true;}
static bool data(void *p,uint64_t g,uint64_t *h,uint8_t *permissions){(void)p;struct nrc_atomic_page a;if(!nrc_resolve_atomic(&session,source_nonce,g,1,&a))return false;*h=a.backing.hpa;*permissions=a.backing.permissions;return true;}
static struct nested_ept_atomic_result walk(uint64_t g,uint8_t need,struct nested_ept_atomic_trace *trace){
 const struct nested_ept_atomic_ops ops={guarded,table,data,0,0};
 return nested_ept_atomic_walk_wb_ipat_1g(session.root.full_eptp,g,need,48,&ops,0,stamp,trace);
}
int main(void){
 assert(!legacy_memory_main());
 base(&d);memset(&m,0,sizeof(m));
 assert(nested_atomic_memory_prepare_1g(&m,&d,id)&&nested_atomic_memory_ready(&m));
 assert(m.catalog.atomic_1g&&m.catalog.published);
 assert(ram[16][1]==0xf7&&ram[24][1]==0x1a007);
 assert(ad_updater_gate_init(&gate,stamp));assert(ad_updater_gate_acquire(&gate,stamp,&token,&updater_nonce));
 const uint64_t gpas[]={0x4000f000,0x40200000,0x7ffff000};
 for(unsigned root=0;root<3;root++){
  memset(&session,0,sizeof session);assert(nrc_begin(&m.catalog,m.roots[root],&m,&session,&source_nonce));
  for(unsigned k=0;k<3;k++){
   struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r=walk(gpas[k],2,&t);
   assert(r.status==NEA_DONE&&r.translation.hpa==d.backing.pages[k==0?(root==2?27:14):k==1?28:30]);
   assert(t.count==(root==2?4u:2u)&&t.entry[t.count-1].leaf_shift==(root==2?12u:30u));
  }
  for(unsigned page=16;page<=27;page++)if(page!=23){
   struct nested_ept_atomic_trace t;
   struct nested_ept_atomic_result r=walk((uint64_t)page<<12,2,&t);
   assert(r.translation.status==NESTED_EPT_L0_BACKING_DENIED);
   if(root!=2){r=walk(((uint64_t)page<<12)+0x40000000,2,&t);assert(r.translation.status==NESTED_EPT_L0_BACKING_DENIED);}
  }
  assert(nested_atomic_memory_ready(&m));assert(nrc_end(&session,source_nonce));
 }
 assert(ad_updater_gate_release(&gate,&token,stamp,updater_nonce));
 for(unsigned r=0;r<5;r++){
  const uint64_t *roots=r==0?d.backing.tables:(r&1?d.root01[(r-1)/2]:d.root02[(r-1)/2]);
  uint64_t *low=paging_phys_to_virt(roots[3]);
  for(unsigned page=16;page<=27;page++)if(page!=23){low[page]|=2;assert(!nested_atomic_memory_ready(&m));low[page]&=~UINT64_C(2);}
  for(unsigned k=0;k<2;k++){
   uint64_t *pt=paging_phys_to_virt(m.sparse_tables[r*2+k]);unsigned index=k?511:0;
   assert(pt[index]==(d.backing.pages[k?30:28]|0x73));
   pt[index]^=0x1000;assert(!nested_atomic_memory_ready(&m));pt[index]^=0x1000;
   pt[42]=d.backing.pages[15]|0x73;assert(!nested_atomic_memory_ready(&m));pt[42]=0;
   denied_host=m.sparse_tables[r*2+k];assert(!nested_atomic_memory_ready(&m));denied_host=0;
  }
 }
 assert(nested_atomic_memory_ready(&m));
 puts("PASS N45 sparse physical maps, all-alias source RO, retained WB inventory, three sparse 1GiB offsets and legacy constructor");return 0;
}
