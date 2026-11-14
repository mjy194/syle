/* OPTIONAL amalgam, never compiled by default kernel build. No duplicate PMM
 * object may be linked with it. Build script localizes all raw/internal symbols. */
#ifndef VLM_SLOTS
#define VLM_SLOTS (32768u+67u+2u)
#endif
#define pmm_init pmm_raw_init
#define pmm_alloc_page pmm_raw_alloc_page
#define pmm_alloc_page_below pmm_raw_alloc_page_below
#define pmm_alloc_page_above pmm_raw_alloc_page_above
#define pmm_free_page pmm_raw_free_page
#define pmm_address_limit pmm_raw_address_limit
#define pmm_get_stats pmm_raw_get_stats
#include "../../krnl/memory.c"
#undef pmm_init
#undef pmm_alloc_page
#undef pmm_alloc_page_below
#undef pmm_alloc_page_above
#undef pmm_free_page
#undef pmm_address_limit
#undef pmm_get_stats
#include "native_candidate.h"
uint64_t pmm_alloc_page(void);
#include "lease_model.c"
#define pmm_free_page pmm_owned_internal_free
#include "pmm_authority.c"
#undef pmm_free_page
#ifdef PNA_HOST_TEST
static bool allowed=true;
void pna_test_guard(bool v){allowed=v;}
static bool guard(void){return allowed;}
#else
extern int paging_ap_start_attempted(void);
static bool guard(void){
 uint16_t cs;__asm__ volatile("mov %%cs,%0":"=r"(cs));
 if((cs&3)||paging_ap_start_attempted())return false;
 uint64_t flags;uint32_t lo,hi;
 __asm__ volatile("pushfq; popq %0":"=r"(flags)::"memory");
 if(flags&(UINT64_C(1)<<9))return false;
 /* IA32_APIC_BASE.BSP. Privileged kernel-only path, never host-executed. */
 __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0x1bu):"memory");
 (void)hi;return (lo&(1u<<8))!=0;
}
#endif
int pmm_init(const struct syle_boot_info *b){return guard()?pmm_raw_init(b):0;}
uint64_t pmm_alloc_page(void){return guard()?pmm_raw_alloc_page():0;}
uint64_t pmm_alloc_page_below(uint64_t n){return guard()?pmm_raw_alloc_page_below(n):0;}
uint64_t pmm_alloc_page_above(uint64_t n){return guard()?pmm_raw_alloc_page_above(n):0;}
int pmm_free_page(uint64_t p){return guard()?pmm_owned_internal_free(p):0;}
uint64_t pmm_address_limit(void){return guard()?pmm_raw_address_limit():0;}
struct pmm_stats pmm_get_stats(void){return guard()?pmm_raw_get_stats():(struct pmm_stats){0};}
static uint64_t epoch;
static bool session;
static bool valid_session(uint64_t e){return guard()&&session&&e&&e==epoch;}
bool pna_start(uint64_t *out){
 if(!guard()||!out||session||epoch==UINT64_MAX||!ready||!init())return false;
 for(unsigned i=0;i<VLM_SLOTS;i++)if(authority.slot[i].pa)return false;
 if(authority.failed)return false;
 authority.draining=false;session=true;*out=++epoch;return true;
}
bool pna_alloc(uint64_t e,struct vlm_allocation *a){return valid_session(e)&&pmm_owned_alloc(e,a);}
bool pna_page(uint64_t e,struct vlm_allocation a,uint64_t *p){return valid_session(e)&&pmm_owned_allocation_page(e,a,p);}
bool pna_pin(uint64_t e,struct vlm_allocation a,struct vlm_handle *h){return valid_session(e)&&pmm_owned_pin(e,a,h);}
bool pna_unpin(uint64_t e,struct vlm_handle h){return valid_session(e)&&pmm_owned_unpin(h);}
bool pna_release(uint64_t e,struct vlm_allocation a){return valid_session(e)&&pmm_owned_release(e,a);}
bool pna_drain(uint64_t e){if(!valid_session(e))return false;pmm_owned_begin_drain();return true;}
bool pna_end(uint64_t e){
 if(!valid_session(e)||!pmm_owned_drained())return false;
 for(unsigned i=0;i<VLM_SLOTS;i++)if(authority.slot[i].pa)return false;
 session=false;return true;
}

#ifdef PNA_HOST_TEST
bool pna_test_exhaust_epoch(void){if(session)return false;epoch=UINT64_MAX;return true;}
#endif

bool pna_alloc_typed(uint64_t e,enum vlm_kind kind,struct vlm_allocation *a){return valid_session(e)&&pmm_owned_alloc_typed(e,kind,a);}
bool pna_kind(uint64_t e,struct vlm_allocation a,enum vlm_kind *kind){return valid_session(e)&&pmm_owned_kind(e,a,kind);}

unsigned pna_capacity(void){return guard()?VLM_SLOTS:0;}
