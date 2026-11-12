#include "../nested_owner_pool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) unsigned char memory[3][4096];
static struct nested_sparse_page pages[3];
#define SAME_REJECT(expr) do {struct nested_owner_pool old=p;struct nested_owner_slot oldslots[3];memcpy(oldslots,slots,sizeof slots);assert(!(expr));assert(!memcmp(&p,&old,sizeof p));assert(!memcmp(slots,oldslots,sizeof slots));}while(0)
int main(void)
{
    for(unsigned i=0;i<3;i++)pages[i]=(struct nested_sparse_page){0x1000+i*4096,memory[i]};
    struct nested_owner_pool p={0};struct nested_owner_slot slots[3]={{0}};
    struct nested_owner_token a,b,c,unchanged={.owner=999};
    assert(nested_owner_pool_init(&p,1,2,pages,slots,3,48));
    SAME_REJECT(nested_owner_pool_take(&p,2,2,&unchanged));assert(unchanged.owner==999);
    SAME_REJECT(nested_owner_pool_take(&p,1,3,&unchanged));
    assert(nested_owner_pool_take(&p,1,2,&a));assert(nested_owner_pool_take(&p,1,2,&b));assert(nested_owner_pool_take(&p,1,2,&c));
    SAME_REJECT(nested_owner_pool_take(&p,1,2,&unchanged));assert(unchanged.owner==999);
    SAME_REJECT(nested_owner_pool_release_all(&p,1,2,true));
    struct nested_owner_token bad=a;bad.owner++;SAME_REJECT(nested_owner_pool_give(&p,bad));
    bad=a;bad.incarnation++;SAME_REJECT(nested_owner_pool_give(&p,bad));
    bad=a;bad.page.view=memory[1];SAME_REJECT(nested_owner_pool_give(&p,bad));
    bad=a;bad.slot=3;SAME_REJECT(nested_owner_pool_give(&p,bad));
    assert(nested_owner_pool_give(&p,a));SAME_REJECT(nested_owner_pool_give(&p,a));
    struct nested_owner_token reused;assert(nested_owner_pool_take(&p,1,2,&reused));
    assert(reused.slot==a.slot&&reused.generation==a.generation+1);
    SAME_REJECT(nested_owner_pool_give(&p,a));
    assert(nested_owner_pool_give(&p,reused));assert(nested_owner_pool_give(&p,b));assert(nested_owner_pool_give(&p,c));
    SAME_REJECT(nested_owner_pool_release_all(&p,1,2,false));
    SAME_REJECT(nested_owner_pool_release_all(&p,1,3,true));
    assert(nested_owner_pool_release_all(&p,1,2,true));
    SAME_REJECT(nested_owner_pool_take(&p,1,2,&a));
    /* All validation before pool/slot initialization. */
    for(unsigned test=0;test<8;test++){
        struct nested_sparse_page invalid[3];memcpy(invalid,pages,sizeof invalid);
        unsigned bits=48;
        switch(test){
        case 0:invalid[1].hpa=invalid[0].hpa;break;
        case 1:invalid[1].view=invalid[0].view;break;
        case 2:invalid[1].hpa++;break;
        case 3:invalid[1].view=(char*)invalid[1].view+1;break;
        case 4:invalid[1].hpa=1ull<<48;break;
        case 5:invalid[1].hpa=1ull<<32;bits=32;break;
        case 6:bits=49;break;
        case 7:invalid[1].view=0;break;
        }
        SAME_REJECT(nested_owner_pool_init(&p,1,3,invalid,slots,3,bits));
    }
    SAME_REJECT(nested_owner_pool_init(&p,1,3,pages,slots,SIZE_MAX,48));
    SAME_REJECT(nested_owner_pool_init(&p,1,0,pages,slots,3,48));
    assert(nested_owner_pool_init(&p,1,3,pages,slots,3,32));
    for(unsigned i=0;i<3;i++)slots[i].generation=UINT64_MAX;
    SAME_REJECT(nested_owner_pool_take(&p,1,3,&a));
    slots[1].generation=UINT64_MAX-1;
    assert(nested_owner_pool_take(&p,1,3,&a)&&a.slot==1&&a.generation==UINT64_MAX);
    assert(nested_owner_pool_give(&p,a));SAME_REJECT(nested_owner_pool_take(&p,1,3,&a));
    assert(nested_owner_pool_release_all(&p,1,3,true));
    assert(nested_owner_pool_init(&p,1,4,pages,slots,3,48));
    struct nested_sparse_page page;assert(nested_owner_pool_alloc(&p,&page));
    assert(nested_owner_pool_take(&p,1,4,&b));
    nested_owner_pool_free(&p,page);assert(p.live==1&&!p.fault);
    nested_owner_pool_free(&p,page);assert(p.live==1&&p.fault); /* duplicate tuple */
    SAME_REJECT(nested_owner_pool_take(&p,1,4,&a));
    assert(nested_owner_pool_give(&p,b));assert(p.live==0);
    SAME_REJECT(nested_owner_pool_release_all(&p,1,4,true)); /* retain after violation */
    struct nested_owner_pool zero;struct nested_owner_slot zero_slots[3];
    assert(nested_owner_pool_init(&zero,0,5,pages,zero_slots,3,48));
    assert(nested_owner_pool_take(&zero,0,5,&a));assert(nested_owner_pool_give(&zero,a));
    assert(nested_owner_pool_release_all(&zero,0,5,true));
    puts("nested_owner_pool: PASS");
}
