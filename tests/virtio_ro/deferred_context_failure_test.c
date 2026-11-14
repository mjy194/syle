#include "deferred_context.h"
#include <assert.h>
#include <stdio.h>
static bool held[8],page_fail;static uint64_t serial,fail_generation;static unsigned releases[8];
unsigned pna_capacity(void){return 8;}
bool pna_start(uint64_t *e){*e=1;return true;}
bool pna_alloc_typed(uint64_t e,enum vlm_kind k,struct vlm_allocation *a){(void)e;(void)k;assert(serial+1<8);held[++serial]=true;*a=(struct vlm_allocation){0,serial,0};return true;}
bool pna_page(uint64_t e,struct vlm_allocation a,uint64_t *p){(void)e;if(page_fail)return false;*p=a.generation*4096;return true;}
bool pna_kind(uint64_t e,struct vlm_allocation a,enum vlm_kind *k){(void)e;(void)a;*k=VLM_GUEST_RAM;return true;}
bool pna_pin(uint64_t e,struct vlm_allocation a,struct vlm_handle *h){(void)e;(void)a;(void)h;return false;}
bool pna_unpin(uint64_t e,struct vlm_handle h){(void)e;(void)h;return false;}
bool pna_release(uint64_t e,struct vlm_allocation a){(void)e;unsigned n=(unsigned)a.generation;assert(n<8&&held[n]);releases[n]++;if(a.generation==fail_generation)return false;held[n]=false;return true;}
bool pna_drain(uint64_t e){(void)e;return true;}
bool pna_end(uint64_t e){(void)e;for(unsigned i=0;i<8;i++)if(held[i])return false;return true;}
int main(void){struct vdc c={0},d={0};struct vdc_entry a[3]={0},b[2]={0};uint64_t pa,pb;assert(vdc_open(&c,a,3)&&vdc_allocate(&c,VLM_GUEST_RAM,&pa)&&vdc_allocate(&c,VLM_GUEST_RAM,&pb));assert(vdc_retire(&c,pa)&&vdc_retire(&c,pb)&&!releases[1]&&!releases[2]);fail_generation=2;assert(!vdc_finalize(&c)&&!held[1]&&held[2]&&!a[0].present&&a[1].present);assert(!vdc_finalize(&c)&&releases[1]==1&&held[2]);fail_generation=0;assert(vdc_finalize(&c)&&!held[2]);
 assert(vdc_open(&d,b,2));page_fail=true;pa=99;assert(!vdc_allocate(&d,VLM_GUEST_RAM,&pa)&&pa==99&&held[3]&&d.failed);fail_generation=3;assert(!vdc_finalize(&d)&&held[3]);fail_generation=0;assert(vdc_finalize(&d)&&!held[3]);puts("deferred context mixed finalize/retry retention: PASS (failure mocks)");return 0;}
