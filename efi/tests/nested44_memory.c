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
 return nested_ept_atomic_walk_wb_ipat_2m(session.root.full_eptp,g,need,48,&ops,0,stamp,trace);
}
int main(void){
 assert(!legacy_memory_main());
 for(unsigned c=0;c<2;c++)for(unsigned p=0;p<2;p++){
  base(&d);memset(&m,0,sizeof(m));ram[c*32+3][p?48:15]^=0x1000;
  static uint64_t before[NAM_OWNED_PAGES][512];memcpy(before,ram,sizeof ram);
  assert(!nested_atomic_memory_prepare_2m(&m,&d,id));assert(!memcmp(before,ram,sizeof ram));
 }
 base(&d);memset(&m,0,sizeof(m));ram[28][123]=0xabcdef;
 static uint64_t snapshot[NAM_OWNED_PAGES][512];memcpy(snapshot,ram,sizeof ram);
 assert(nested_atomic_memory_prepare_2m(&m,&d,id)&&nested_atomic_memory_ready(&m));
 for(unsigned page=0;page<NAM_OWNED_PAGES;page++)for(unsigned word=0;word<512;word++){
  unsigned g=page+1;bool changed=(g>=16&&g<=20)||(g>=24&&g<=27)||(g==28&&word==0)||
   ((g==4||g==36)&&(word==15||word==48));
  for(unsigned r=0;r<5;r++){
   uint64_t h=r==0?d.backing.tables[3]:(r&1?d.root01[(r-1)/2][3]:d.root02[(r-1)/2][3]);
   if((uint64_t)g*4096==h&&word>0&&word<64)changed=true;
  }
  if(!changed)assert(ram[page][word]==snapshot[page][word]);
 }
 assert(m.catalog.atomic_2m&&m.catalog.published&&ram[28][123]==0xabcdef);
 assert(ram[17][0]==0x13007&&ram[17][1]==0xf7&&ram[25][0]==0x1b007&&ram[25][1]==0x1b007);
 for(unsigned c=0;c<2;c++)assert(ram[c*32+3][15]==0x20f063&&ram[c*32+3][48]==0x21d063);
 assert(ad_updater_gate_init(&gate,stamp));assert(ad_updater_gate_acquire(&gate,stamp,&token,&updater_nonce));
 for(unsigned root=0;root<3;root++){
  memset(&session,0,sizeof session);assert(nrc_begin(&m.catalog,m.roots[root],&m,&session,&source_nonce));
  struct nested_ept_atomic_trace t;
  struct nested_ept_atomic_result r=walk(0x20f000,1,&t);
  assert(r.status==NEA_DONE&&r.translation.hpa==d.backing.pages[root==2?27:14]);
  assert(t.count==(root==2?4u:3u)&&t.entry[t.count-1].leaf_shift==(root==2?12u:21u));
  r=walk(0x21d000,2,&t);assert(r.status==NEA_DONE&&r.translation.hpa==d.backing.pages[28]);
  for(unsigned page=16;page<=27;page++)if(page<=20||page>=24){
   for(unsigned high=0;high<2;high++){
    r=walk(((uint64_t)page<<12)+(high?0x200000:0),2,&t);
    assert(r.translation.status==NESTED_EPT_L0_BACKING_DENIED);
   }
  }
  assert(nested_atomic_memory_ready(&m));assert(nrc_end(&session,source_nonce));
 }
 assert(ad_updater_gate_release(&gate,&token,stamp,updater_nonce));
 /* Explicit corruption tests only, after all updater/source leases end. */
 for(unsigned root=0;root<5;root++){
  uint64_t *pt=paging_phys_to_virt(root==0?d.backing.tables[3]:(root&1?d.root01[(root-1)/2][3]:d.root02[(root-1)/2][3]));
  for(unsigned page=16;page<=27;page++)if(page<=20||page>=24){pt[page]|=2;assert(!nested_atomic_memory_ready(&m));pt[page]&=~UINT64_C(2);}
 }
 assert(nested_atomic_memory_ready(&m));
 puts("PASS N44 production memory constructor, legacy regression, PS-aware source audit, all source aliases read-only, exact remaps and independent backing");return 0;
}
