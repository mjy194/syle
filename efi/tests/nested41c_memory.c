/* Production fixture authority tests. Host paging/MTRR probes are mocks here;
 * no claim of actual hardware mapping verification is made by this test. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../krnl/nested_atomic_memory.h"
static _Alignas(4096) uint64_t ram[NAM_OWNED_PAGES][512];
static uint64_t denied_host,denied_wb;
void *paging_phys_to_virt(uint64_t p){return p>=4096&&p<(NAM_OWNED_PAGES+1)*4096&&!(p&4095)?ram[p/4096-1]:NULL;}
int paging_page_rw_wb(uint64_t p){return p!=denied_host;}
int vmx_memory_region_wb(uint64_t p){return p!=denied_wb;}
static unsigned allocated;
uint64_t pmm_alloc_page_below(uint64_t limit){
 assert(limit==UINT64_C(0x100000000)&&allocated<NAM_OWNED_PAGES);
 return (uint64_t)(++allocated)*4096;
}
int pmm_free_page(uint64_t p){(void)p;assert(!"No backing release allowed");return 0;}
_Noreturn void panic(const char *message){(void)message;assert(!"Unexpected panic");__builtin_trap();}
void ept_fixture_owned_release(struct ept_fixture *f){(void)f;assert(!"No fixture release allowed");}
/* Allocation/payload boundary mock follows the real ownership flag semantics.
 * The shared domain construction itself below is production code. */
bool ept_fixture_prepare(struct ept_fixture *f){
 memset(f,0,sizeof(*f));f->started=true;
 for(unsigned n=0;n<63;n++)f->pages[n]=pmm_alloc_page_below(UINT64_C(0x100000000));
 for(unsigned l=0;l<4;l++)f->tables[l]=pmm_alloc_page_below(UINT64_C(0x100000000));
 for(unsigned l=0;l<3;l++)((uint64_t *)paging_phys_to_virt(f->tables[l]))[0]=f->tables[l+1]|7;
 for(unsigned n=1;n<64;n++)((uint64_t *)paging_phys_to_virt(f->tables[3]))[n]=f->pages[n-1]|0x30|(n==6?5:3);
 f->eptp=f->tables[0]|0x1e;return true;
}
static void base(struct nested_domain_memory *d){
 memset(ram,0,sizeof(ram));denied_host=denied_wb=0;allocated=0;
 assert(nested_domain_memory_prepare(d));assert(d->backing.started);
 assert(allocated==NAM_OWNED_PAGES);
}
static struct nested_domain_memory d;
static struct nested_atomic_memory m;
static const struct nrc_identity id={1,2,3,4};
int main(void){
 for(unsigned fault=0;fault<7;fault++){
  base(&d);memset(&m,0,sizeof(m));
  if(fault==0)denied_host=d.backing.pages[15];
  if(fault==1)denied_wb=d.backing.pages[15];
  if(fault==2)d.backing.pages[20]=d.backing.pages[15];
  if(fault==3)ram[d.root01[1][3]/4096-1][80]=d.backing.pages[15]|0x33;
  if(fault==4)ram[18][16]|=2;
  if(fault==6)d.backing.started=false;
  if(fault==5)ram[d.backing.tables[3]/4096-1][16]^=0x10;
  static uint64_t saved[NAM_OWNED_PAGES][512];memcpy(saved,ram,sizeof(ram));
  assert(!nested_atomic_memory_prepare(&m,&d,id));assert(!memcmp(saved,ram,sizeof(ram)));
 }
 base(&d);memset(&m,0,sizeof(m));assert(nested_atomic_memory_prepare(&m,&d,id));
 assert(nested_atomic_memory_ready(&m));
 d.atomic_catalog_retained=false;assert(!nested_atomic_memory_ready(&m));d.atomic_catalog_retained=true;
 assert(m.catalog.span_count==63&&m.catalog.root_count==3);
 assert(ram[15][0]==ram[19][0]&&ram[15][0]==0x11007&&ram[26][15]==0x1c073);
 assert(m.spans[0].permissions==3&&m.spans[32].permissions==3&&m.spans[5].permissions==5);
 struct nrc_session s={0};uint64_t ticket=0;struct nrc_atomic_page out;
 assert(nrc_begin(&m.catalog,m.roots[0],&m,&s,&ticket));
 assert(nrc_resolve_atomic(&s,ticket,0x10000,1,&out));assert(out.atomic_view==ram[15]);
 __atomic_fetch_or(&ram[15][0],0x100,__ATOMIC_RELAXED);assert(nested_atomic_memory_ready(&m));
 denied_host=d.backing.pages[15];assert(!nrc_resolve_atomic(&s,ticket,0x10000,1,&out));denied_host=0;
 m.spans[15].lifetime++;assert(!nrc_resolve_atomic(&s,ticket,0x10000,1,&out));m.spans[15].lifetime--;
 ram[d.root01[1][3]/4096-1][16]|=2;assert(!nrc_resolve_atomic(&s,ticket,0x10000,1,&out));ram[d.root01[1][3]/4096-1][16]&=~UINT64_C(2);
 __atomic_fetch_or(&ram[15][0],0x200,__ATOMIC_RELAXED);assert(!nested_atomic_memory_ready(&m));
 assert(nrc_end(&s,ticket));puts("N41C_MEMORY_OK");return 0;
}
