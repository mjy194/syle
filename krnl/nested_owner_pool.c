#include "nested_owner_pool.h"
static bool identity(const struct nested_owner_pool *p,uint64_t owner,uint64_t incarnation)
{return p&&p->active&&owner==p->owner&&incarnation==p->incarnation;}
static bool page_valid(struct nested_sparse_page page,unsigned bits)
{return page.hpa&&!(page.hpa&4095)&&page.hpa<(UINT64_C(1)<<bits)&&
    page.view&&!((uintptr_t)page.view&4095)&&(uintptr_t)page.view<=UINTPTR_MAX-4095;}
bool nested_owner_pool_init(struct nested_owner_pool *p,uint64_t owner,uint64_t incarnation,
    const struct nested_sparse_page *pages,struct nested_owner_slot *slots,size_t count,unsigned bits)
{
    if(!p||!incarnation||!pages||!slots||!count||bits<12||bits>48||
       count>SIZE_MAX/sizeof(*pages)||count>SIZE_MAX/sizeof(*slots))return false;
    for(size_t i=0;i<count;i++){
        if(!page_valid(pages[i],bits))return false;
        for(size_t j=0;j<i;j++)if(pages[i].hpa==pages[j].hpa||pages[i].view==pages[j].view)return false;
    }
    for(size_t i=0;i<count;i++)slots[i]=(struct nested_owner_slot){0};
    *p=(struct nested_owner_pool){.owner=owner,.incarnation=incarnation,.pages=pages,
        .slots=slots,.count=count,.maxphys=bits,.active=true};
    return true;
}
bool nested_owner_pool_take(struct nested_owner_pool *p,uint64_t owner,uint64_t incarnation,
    struct nested_owner_token *out)
{
    if(!out||!identity(p,owner,incarnation)||p->fault||p->live>=p->count)return false;
    for(size_t i=0;i<p->count;i++)if(!p->slots[i].live&&p->slots[i].generation!=UINT64_MAX){
        uint64_t generation=p->slots[i].generation+1;
        struct nested_owner_token token={owner,incarnation,generation,i,p->pages[i]};
        p->slots[i].generation=generation;p->slots[i].live=true;p->live++;*out=token;return true;
    }
    return false;
}
bool nested_owner_pool_give(struct nested_owner_pool *p,struct nested_owner_token t)
{
    if(!identity(p,t.owner,t.incarnation)||t.slot>=p->count||!p->live)return false;
    struct nested_owner_slot *s=&p->slots[t.slot];
    if(!s->live||s->generation!=t.generation||p->pages[t.slot].hpa!=t.page.hpa||
       p->pages[t.slot].view!=t.page.view)return false;
    s->live=false;p->live--;return true;
}
bool nested_owner_pool_release_all(struct nested_owner_pool *p,uint64_t owner,uint64_t incarnation,bool quiet)
{
    if(!identity(p,owner,incarnation)||!quiet||p->fault||p->live)return false;
    for(size_t i=0;i<p->count;i++)if(p->slots[i].live)return false;
    p->active=false;return true;
}
bool nested_owner_pool_alloc(void *context,struct nested_sparse_page *out)
{
    struct nested_owner_pool *p=context;struct nested_owner_token token;
    if(!p||!out||!nested_owner_pool_take(p,p->owner,p->incarnation,&token))return false;
    *out=token.page;return true;
}
void nested_owner_pool_free(void *context,struct nested_sparse_page page)
{
    struct nested_owner_pool *p=context;if(!p)return;
    if(p->active)for(size_t i=0;i<p->count;i++)if(p->pages[i].hpa==page.hpa&&p->pages[i].view==page.view){
        struct nested_owner_token token={p->owner,p->incarnation,p->slots[i].generation,i,page};
        if(nested_owner_pool_give(p,token))return;
        break;
    }
    p->fault=true;
}
