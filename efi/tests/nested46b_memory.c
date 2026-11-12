/* Actual staged physical fixture adapter. Host/PAT/cache/VMX are mocks. */
#define main legacy_memory_main
#include "nested41c_memory.c"
#undef main
#include <sys/mman.h>
static unsigned host_type46=6,guards46;
static bool uc_guard46(void *context,uint64_t hpa,uint64_t epoch){
 assert(context==&m);guards46++;
 return hpa==m.owned[14]&&epoch==23&&host_type46==0;
}
static void stage46(void){
 base(&d);memset(&m,0,sizeof m);host_type46=6;guards46=0;
 assert(nested_atomic_memory_uc_stage(&m,&d,id,23));
 assert(d.atomic_catalog_retained&&!d.atomic_ad_prepared&&!m.catalog.published&&m.catalog.atomic_uc);
 assert(m.catalog.uc_gpa==0xf000&&m.catalog.uc_hpa==d.backing.pages[14]&&m.catalog.type_epoch==23);
 assert(nested_atomic_memory_uc_staged(&m)&&!nested_atomic_memory_ready(&m));
 struct nrc_session session={0};uint64_t ticket=0;
 assert(!nrc_begin(&m.catalog,m.roots[0],&m,&session,&ticket)&&!m.catalog.references);
}
static const uint64_t *roots46(unsigned r){return r==0?d.backing.tables:(r&1?d.root01[(r-1)/2]:d.root02[(r-1)/2]);}
static void positive46(void){
 stage46();uint64_t sentinel=UINT64_C(0x4655535441524745);ram[14][9]=sentinel;
 assert(!nested_atomic_memory_uc_publish(&m)&&!m.catalog.published);
 assert(nested_atomic_memory_uc_detach(&m)&&!nested_atomic_memory_uc_staged(&m));
 for(unsigned r=0;r<5;r++)assert(!((uint64_t *)paging_phys_to_virt(roots46(r)[3]))[15]);
 /* Unmapped host target must never be read by an unsuccessful staged call. */
 host_type46=8;assert(!mprotect(ram[14],4096,PROT_NONE));
 assert(!nested_atomic_memory_uc_install(&m,uc_guard46,&m));
 assert(!nested_atomic_memory_uc_publish(&m));
 assert(!mprotect(ram[14],4096,PROT_READ|PROT_WRITE));
 host_type46=0;denied_host=d.backing.pages[14]; /* old all-WB host test must not run on target */
 assert(nested_atomic_memory_uc_install(&m,uc_guard46,&m));
 assert(nested_atomic_memory_uc_installed(&m)&&!m.catalog.published&&!m.prepared);
 assert(ram[14][9]==sentinel&&ram[18][15]==0xf043&&ram[26][15]==0x1c073);
 for(unsigned r=0;r<5;r++)assert(((uint64_t *)paging_phys_to_virt(roots46(r)[3]))[15]==(d.backing.pages[14]|0x43));
 assert(nested_atomic_memory_uc_publish(&m)&&nested_atomic_memory_ready(&m));
 assert(!nested_atomic_memory_uc_publish(&m)&&!nested_atomic_memory_uc_install(&m,uc_guard46,&m));
 struct nrc_session session={0};uint64_t ticket=0;struct nrc_atomic_page out;
 assert(nrc_begin(&m.catalog,m.roots[0],&m,&session,&ticket));
 assert(nrc_resolve_atomic(&session,ticket,0xf000,1,&out)&&out.backing.memory_type==0&&out.backing.hpa==d.backing.pages[14]&&!out.atomic_view);
 assert(nrc_resolve_atomic(&session,ticket,0x10000,1,&out)&&out.backing.memory_type==6&&out.pinned_writable&&out.atomic_view==ram[15]);
 __atomic_fetch_or(&ram[18][15],0x300,__ATOMIC_ACQ_REL);assert(nested_atomic_memory_ready(&m));
 host_type46=6;assert(!nested_atomic_memory_ready(&m)&&!nrc_resolve_atomic(&session,ticket,0xf000,1,&out));host_type46=0;
 assert(nrc_end(&session,ticket)&&guards46);puts("PASS N46B staged immutable owned fixture and no absent target access");
}
static void failures46(void){
 for(unsigned bad=0;bad<14;bad++){
  stage46();assert(nested_atomic_memory_uc_detach(&m));host_type46=0;
  switch(bad){
  case 0:host_type46=6;break;case 1:d.backing.pages[14]+=4096;break;
  case 2:ram[d.root01[1][3]/4096-1][80]=m.owned[14]|0x33;break;
  case 3:ram[d.backing.tables[3]/4096-1][15]=m.owned[14]|0x33;break;
  case 4:ram[d.root02[1][2]/4096-1][1]=0xb3;break;
  case 5:ram[18][16]|=2;break;case 6:denied_host=m.owned[15];break;
  case 7:denied_wb=m.owned[15];break;case 8:d.atomic_catalog_retained=false;break;
  case 9:m.catalog.published=true;break;case 10:m.catalog.uc_hpa+=4096;break;
  case 11:m.catalog.type_epoch++;break;case 12:m.catalog.references=1;break;case 13:m.uc_detached=false;break;
  }
  static uint64_t before[NAM_OWNED_PAGES][512];memcpy(before,ram,sizeof ram);
  assert(!nested_atomic_memory_uc_install(&m,uc_guard46,&m)&&!memcmp(before,ram,sizeof ram));
  assert(!m.prepared);printf("PASS N46B stage/host/alias failure %u\n",bad);
 }
 for(unsigned r=0;r<5;r++){
  stage46();assert(nested_atomic_memory_uc_detach(&m));host_type46=0;
  assert(nested_atomic_memory_uc_install(&m,uc_guard46,&m));
  uint64_t *pt=paging_phys_to_virt(roots46(r)[3]);pt[15]^=0x30;
  assert(!nested_atomic_memory_uc_publish(&m)&&!m.catalog.published&&d.atomic_catalog_retained);
  pt[15]^=0x30;pt[16]|=2;
  assert(!nested_atomic_memory_uc_publish(&m));printf("PASS N46B final target/table alias failure %u\n",r);
 }
}
int main(void){positive46();failures46();}
