#include "create_context.h"
#include <assert.h>
#include <stdio.h>
static bool held,release_fail,page_fail;static unsigned releases;static uint64_t serial;
bool pna_start(uint64_t *e){*e=1;return true;}
bool pna_alloc_typed(uint64_t e,enum vlm_kind k,struct vlm_allocation *a){(void)e;(void)k;assert(!held);held=true;*a=(struct vlm_allocation){0,++serial,0};return true;}
bool pna_page(uint64_t e,struct vlm_allocation a,uint64_t *p){(void)e;(void)a;if(page_fail)return false;*p=4096;return true;}
bool pna_kind(uint64_t e,struct vlm_allocation a,enum vlm_kind *k){(void)e;(void)a;*k=VLM_GUEST_RAM;return true;}
bool pna_pin(uint64_t e,struct vlm_allocation a,struct vlm_handle *h){(void)e;(void)a;(void)h;return false;}
bool pna_unpin(uint64_t e,struct vlm_handle h){(void)e;(void)h;return false;}
bool pna_release(uint64_t e,struct vlm_allocation a){(void)e;(void)a;releases++;if(release_fail)return false;assert(held);held=false;return true;}
bool pna_drain(uint64_t e){(void)e;return true;}
bool pna_end(uint64_t e){(void)e;return !held;}
int main(void){struct vtc c={0},d={0};struct vtc_entry a[3]={0},b[2]={0};uint64_t pa=99;assert(vtc_open(&c,a,3)&&vtc_allocate(&c,VLM_GUEST_RAM,&pa)&&vtc_release(&c,pa));release_fail=true;assert(!vtc_allocate(&c,VLM_GUEST_RAM,&pa)&&held&&c.failed&&c.count==2&&a[1].quarantined);unsigned n=releases;assert(!vtc_release(&c,pa)&&releases==n);assert(!vtc_close(&c)&&held&&!c.ended);release_fail=false;assert(vtc_close(&c)&&!held);
 assert(vtc_open(&d,b,2));page_fail=true;pa=99;assert(!vtc_allocate(&d,VLM_GUEST_RAM,&pa)&&pa==99&&held&&d.failed&&b[0].quarantined);release_fail=true;assert(!vtc_close(&d)&&held);release_fail=false;assert(vtc_close(&d)&&!held);
 puts("per-CREATE partial admission/rollback retention: PASS (failure mocks)");return 0;}
