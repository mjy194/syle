#include "lease_model.h"
#include <assert.h>
#include <stdio.h>
static uint64_t next;static bool release_fail;static unsigned frees,allocs;
static uint64_t allocate(void *x){(void)x;allocs++;return next;}
static bool release(void *x,uint64_t pa){(void)x;(void)pa;frees++;return !release_fail;}
/* Each test reset starts a new mock allocator world, NOT a production reset. */
static void init(struct vlm *s){*s=(struct vlm){0};struct vlm_ops o={0,allocate,release};next=4096;release_fail=false;frees=allocs=0;assert(vlm_init(s,o));}
int main(void){struct vlm s={0};uint64_t pa=99;struct vlm_handle a,b;struct vlm_allocation p,old;
 init(&s);assert(!vlm_pin(&s,1,(struct vlm_allocation){0,1,&s},&a));assert(vlm_allocate(&s,1,&p));assert(vlm_allocation_page(&s,1,p,&pa)&&pa==4096);assert(!vlm_pin(&s,2,p,&a));assert(vlm_pin(&s,1,p,&a));assert(!vlm_pin(&s,1,p,&b));assert(!vlm_release(&s,1,p)&&!frees);assert(!vlm_init(&s,s.ops));vlm_begin_drain(&s);assert(!vlm_drained(&s));assert(!vlm_allocate(&s,1,&old));assert(vlm_unpin(&s,a)&&vlm_drained(&s));assert(!vlm_pin(&s,1,p,&b));release_fail=true;assert(!vlm_release(&s,1,p)&&vlm_allocation_page(&s,1,p,&pa));release_fail=false;assert(vlm_release(&s,1,p));assert(!vlm_release(&s,1,p));assert(!vlm_unpin(&s,a));assert(!vlm_init(&s,s.ops));
 init(&s);assert(vlm_allocate(&s,1,&p)&&vlm_pin(&s,1,p,&a)&&vlm_unpin(&s,a));assert(vlm_pin(&s,1,p,&b)&&b.serial!=a.serial);assert(!vlm_unpin(&s,a));assert(vlm_unpin(&s,b)&&vlm_release(&s,1,p));old=p;assert(vlm_allocate(&s,1,&p)&&p.generation!=old.generation);assert(!vlm_release(&s,1,old));assert(!vlm_pin(&s,1,old,&a));pa=99;assert(!vlm_allocation_page(&s,1,old,&pa)&&pa==99);assert(vlm_pin(&s,1,p,&b));
 init(&s);next=0;assert(!vlm_allocate(&s,1,&p)&&!s.failed);
 init(&s);assert(vlm_allocate(&s,1,&p));assert(!vlm_allocate(&s,2,&old)&&s.failed&&!frees);
 init(&s);next=4097;assert(!vlm_allocate(&s,1,&p)&&s.failed&&!frees);
 init(&s);for(unsigned i=0;i<VLM_SLOTS;i++){next=(i+1)*4096;assert(vlm_allocate(&s,1,&p));}unsigned calls=allocs;assert(!vlm_allocate(&s,1,&old)&&allocs==calls);s.serial=UINT64_MAX;assert(!vlm_pin(&s,1,p,&a));
 init(&s);s.serial=UINT64_MAX;assert(!vlm_allocate(&s,1,&p)&&!allocs);s.serial=UINT64_MAX-1;assert(vlm_allocate(&s,1,&p)&&p.generation==UINT64_MAX);assert(!vlm_pin(&s,1,p,&a));assert(vlm_release(&s,1,p));assert(!vlm_allocate(&s,1,&p)&&allocs==1);
 struct vlm other={0};init(&s);assert(vlm_allocate(&s,1,&p)&&vlm_pin(&s,1,p,&a));assert(vlm_init(&other,s.ops));assert(!vlm_release(&other,1,p));assert(!vlm_pin(&other,1,p,&b));assert(!vlm_unpin(&other,a));
 struct vlm dirty={0};dirty.slot[0].pa=4096;assert(!vlm_init(&dirty,s.ops));dirty=(struct vlm){0};dirty.serial=1;assert(!vlm_init(&dirty,s.ops));
 puts("allocator generation/lease interface model: PASS (not native PMM)");return 0;
}
