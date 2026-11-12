#include "cache_ept_alias.h"
#include <stddef.h>
#define ADDR UINT64_C(0x0000fffffffff000)
static bool page(uint64_t x) {return !(x&4095)&&x<(UINT64_C(1)<<48);}
static bool alias(const struct cache_ept_alias_plan *p,unsigned i) {
    return (p->aliases[i/64]>>(i%64))&1;
}
bool cache_ept_alias_prepare(struct cache_ept_alias_plan *out,
    const struct cache_ept_alias_view *v,uint64_t target,bool to_uc) {
    if(!out||!v||!page(target))return false;
    for(unsigned d=0;d<4;d++) {
        if(!page(v->hpa[d])||!v->table[d]||((uintptr_t)v->table[d]&4095)||
           target==v->hpa[d])return false;
        for(unsigned j=0;j<d;j++)
            if(v->hpa[d]==v->hpa[j]||v->table[d]==v->table[j])return false;
    }
    if(v->eptp!=(v->hpa[0]|0x1e))return false;
    for(unsigned d=0;d<3;d++)for(unsigned i=0;i<512;i++) {
        uint64_t x=v->table[d][i];
        if(i==0) {if(x!=(v->hpa[d+1]|7))return false;}
        else if(x&7)return false;
    }
    uint64_t expected=target|(to_uc?0x31:0x41);
    unsigned count=0;
    for(unsigned i=0;i<512;i++) {
        uint64_t x=v->table[3][i];
        if((x&7)&&(x&ADDR)==target) {
            if(x!=expected)return false;
            count++;
        }
    }
    if(!count)return false;
    /* Validation precedes all output writes. Caller keeps inputs stable. */
    out->view=*v;out->target=target;out->to_uc=to_uc;out->count=count;
    for(unsigned i=0;i<8;i++)out->aliases[i]=0;
    for(unsigned d=0;d<4;d++)for(unsigned i=0;i<512;i++) {
        uint64_t x=v->table[d][i];out->original[d][i]=x;
        if(d==3&&(x&7)&&(x&ADDR)==target)
            out->aliases[i/64]|=UINT64_C(1)<<(i%64);
    }
    out->phase=CACHE_EPT_ALIAS_READY;
    return true;
}
static bool unchanged(const struct cache_ept_alias_plan *p,bool detached) {
    for(unsigned d=0;d<4;d++)for(unsigned i=0;i<512;i++) {
        uint64_t expected=(detached&&d==3&&alias(p,i))?0:p->original[d][i];
        if(p->view.table[d][i]!=expected)return false;
    }
    return true;
}
bool cache_ept_alias_detach(struct cache_ept_alias_plan *p) {
    if(!p||p->phase!=CACHE_EPT_ALIAS_READY||!unchanged(p,false))return false;
    for(unsigned i=0;i<512;i++)if(alias(p,i))p->view.table[3][i]=0;
    p->phase=CACHE_EPT_ALIAS_DETACHED;
    return true;
}
bool cache_ept_alias_publish(struct cache_ept_alias_plan *p) {
    if(!p||p->phase!=CACHE_EPT_ALIAS_DETACHED||!unchanged(p,true))return false;
    uint64_t entry=p->target|(p->to_uc?0x41:0x31);
    for(unsigned i=0;i<512;i++)if(alias(p,i))p->view.table[3][i]=entry;
    p->phase=CACHE_EPT_ALIAS_PUBLISHED;
    return true;
}
