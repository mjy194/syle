#include "pmm_authority.h"
/* Host build renames memory.c's free entry point. Only this translation unit
 * may call it. Production adoption must hide raw symbol inside PMM source. */
extern int pmm_raw_free_page(uint64_t);
static struct vlm authority;
static bool initialized;
static uint64_t allocate(void *ctx){(void)ctx;return pmm_alloc_page();}
static bool release(void *ctx,uint64_t pa){(void)ctx;return pmm_raw_free_page(pa)!=0;}
static bool init(void){if(initialized)return true;struct vlm_ops o={0,allocate,release};initialized=vlm_init(&authority,o);return initialized;}
int pmm_free_page(uint64_t pa){
 /* Ordinary PMM clients cannot free a protected page, even after unpin. */
 for(unsigned i=0;i<VLM_SLOTS;i++)if(authority.slot[i].pa==pa)return 0;
 return pmm_raw_free_page(pa);
}
bool pmm_owned_alloc(uint64_t owner,struct vlm_allocation *a){return init()&&vlm_allocate(&authority,owner,a);}
bool pmm_owned_allocation_page(uint64_t owner,struct vlm_allocation a,uint64_t *pa){return initialized&&vlm_allocation_page(&authority,owner,a,pa);}
bool pmm_owned_pin(uint64_t owner,struct vlm_allocation a,struct vlm_handle *h){return initialized&&vlm_pin(&authority,owner,a,h);}
bool pmm_owned_unpin(struct vlm_handle h){return initialized&&vlm_unpin(&authority,h);}
bool pmm_owned_release(uint64_t owner,struct vlm_allocation a){return initialized&&vlm_release(&authority,owner,a);}
bool pmm_owned_lease_page(struct vlm_handle h,uint64_t *pa){
 if(!initialized||authority.failed||!pa||h.issuer!=&authority||h.slot>=VLM_SLOTS||!h.serial||authority.slot[h.slot].lease!=h.serial)return false;
 *pa=authority.slot[h.slot].pa;return true;
}
void pmm_owned_begin_drain(void){if(init())vlm_begin_drain(&authority);}
bool pmm_owned_drained(void){return initialized&&vlm_drained(&authority);}

bool pmm_owned_alloc_typed(uint64_t owner,enum vlm_kind kind,struct vlm_allocation *a){return init()&&vlm_allocate_typed(&authority,owner,kind,a);}
bool pmm_owned_kind(uint64_t owner,struct vlm_allocation a,enum vlm_kind *kind){return initialized&&vlm_kind_of(&authority,owner,a,kind);}
