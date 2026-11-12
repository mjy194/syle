#include "cache_lease_refs.h"
#include <stddef.h>
_Static_assert(sizeof(struct cache_lease_token)<=128,"token fits context attachment");
/* Validate the trusted intrusive list against its count. The explicit count
 * bounds a cycle's traversal; metadata pointers themselves must remain live.
 * This is not a parser for attacker-supplied arbitrary pointers. */
static bool list_valid(const struct cache_lease_refs *r)
{
    if(!r || !r->generation ||
       (r->phase!=CACHE_LEASE_REFS_ACTIVE && r->phase!=CACHE_LEASE_REFS_POISON))return false;
    const struct cache_lease_token *p=r->head;
    uint64_t n=0;
    while(p){
        if(n==r->refs || !p->linked || p->ledger!=r || !p->owner || p->generation!=r->generation)return false;
        ++n;p=p->next;
    }
    return n==r->refs;
}
bool cache_lease_refs_init(struct cache_lease_refs *r,uint64_t generation)
{
    if(!r || !generation)return false;
    *r=(struct cache_lease_refs){.generation=generation,.phase=CACHE_LEASE_REFS_ACTIVE};
    return true;
}
bool cache_lease_refs_acquire(struct cache_lease_refs *r,struct cache_lease_token *t,
    const void *owner,uint64_t generation)
{
    if(!r || !t || !owner || r->phase!=CACHE_LEASE_REFS_ACTIVE ||
       r->generation!=generation || !generation || r->refs==UINT64_MAX ||
       t->linked || t->ledger || t->next || t->owner || t->generation || !list_valid(r))return false;
    for(const struct cache_lease_token *p=r->head;p;p=p->next)
        if(p==t || p->owner==owner)return false;
    *t=(struct cache_lease_token){.ledger=r,.next=r->head,.owner=owner,
        .generation=generation,.linked=true};
    r->head=t;++r->refs;
    return true;
}
static bool belongs(const struct cache_lease_refs *r,const struct cache_lease_token *t,
    const void *owner,uint64_t generation)
{
    if(!r || !t || !owner || !generation || generation!=r->generation ||
       !t->linked || t->ledger!=r || t->owner!=owner || t->generation!=generation ||
       !list_valid(r))return false;
    for(const struct cache_lease_token *p=r->head;p;p=p->next)if(p==t)return true;
    return false;
}
bool cache_lease_refs_admit(const struct cache_lease_refs *r,const struct cache_lease_token *t,
    const void *owner,uint64_t generation)
{
    return r && r->phase==CACHE_LEASE_REFS_ACTIVE && belongs(r,t,owner,generation);
}
bool cache_lease_refs_release(struct cache_lease_refs *r,struct cache_lease_token *t,
    const void *owner,uint64_t generation)
{
    if(!belongs(r,t,owner,generation))return false;
    struct cache_lease_token **link=&r->head;
    while(*link!=t)link=&(*link)->next;
    *link=t->next;--r->refs;*t=(struct cache_lease_token){0};
    return true;
}
bool cache_lease_refs_advance(struct cache_lease_refs *r,uint64_t expected,uint64_t next)
{
    if(!r || r->phase!=CACHE_LEASE_REFS_ACTIVE || !list_valid(r) ||
       r->refs || r->head || r->generation!=expected || next<=expected)return false;
    r->generation=next;return true;
}
void cache_lease_refs_poison(struct cache_lease_refs *r)
{if(r)r->phase=CACHE_LEASE_REFS_POISON;}
