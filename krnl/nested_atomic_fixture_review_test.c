/* Independent integration of real EPT allocator fixture, domain builder and
 * atomic source publisher. Only PMM/host mapping/invalidation are mocked. */
#include "nested_atomic_memory.h"
#include "memory.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint64_t pages[96][512];
static bool used[96];
static unsigned allocations,frees;
static unsigned ix(uint64_t p){assert(p>=0x100000&&p<0x160000&&!(p&4095));return (p-0x100000)/4096;}
uint64_t pmm_alloc_page(void){assert(allocations<96);unsigned i=allocations++;used[i]=true;memset(pages[i],0,4096);return 0x100000+i*4096;}
uint64_t pmm_alloc_page_below(uint64_t limit){assert(limit==UINT64_C(0x100000000));return pmm_alloc_page();}
int pmm_free_page(uint64_t p){unsigned i=ix(p);assert(used[i]);used[i]=false;frees++;return 1;}
struct pmm_stats pmm_get_stats(void){return (struct pmm_stats){96,96-allocations+frees,allocations-frees};}
void *paging_phys_to_virt(uint64_t p){unsigned i=ix(p);assert(used[i]);return pages[i];}
int paging_page_rw_wb(uint64_t p){return used[ix(p)];}
int vmx_memory_region_wb(uint64_t p){return used[ix(p)];}
bool ept_test_invalidate(uint64_t eptp){return (eptp&4095)==0x1e;}
_Noreturn void panic(const char *p){fprintf(stderr,"unexpected panic: %s\n",p);abort();}
__asm__(".pushsection .rodata\n.global ept_payload_start,ept_payload_end,ept_exec_start,ept_exec_end\nept_payload_start:\n.byte 0x90\nept_payload_end:\nept_exec_start:\n.byte 0x90\nept_exec_end:\n.popsection\n");
int main(void){
 struct nested_domain_memory d={0};struct nested_atomic_memory m={0};
 assert(nested_domain_memory_prepare(&d));
 assert(d.ready&&d.backing.started&&allocations==NAM_OWNED_PAGES&&!frees);
 assert(nested_atomic_memory_prepare(&m,&d,(struct nrc_identity){1,2,3,4}));
 assert(nested_atomic_memory_ready(&m));
 assert(m.catalog.published&&m.catalog.atomic_wb&&m.catalog.root_count==3);
 assert(d.backing.started&&allocations==NAM_OWNED_PAGES&&!frees);
 for(unsigned cpu=0;cpu<2;cpu++) {
  struct ept_fixture f;assert(nested_domain_memory_view(&d,cpu,&f));
  assert(f.started&&(f.eptp&4095)==0x1e);
 }
 /* No source free: this profile intentionally has no such authority. */
 puts("PASS review N41C real ept_fixture_prepare to domain to atomic publication, owned-started state, AD0 borrowed views, retained backing");
 return 0;
}
