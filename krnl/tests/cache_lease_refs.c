#include "../cache_lease_refs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define COUNT 257
static struct cache_lease_token tokens[COUNT];
static unsigned owners[COUNT];
#define NOCHANGE(call) do {struct cache_lease_refs old=r;struct cache_lease_token was=t;assert(!(call));assert(!memcmp(&r,&old,sizeof r));assert(!memcmp(&t,&was,sizeof t));}while(0)
int main(void)
{
    struct cache_lease_refs r={0},other={0};struct cache_lease_token t={0},u={0};unsigned a,b;
    NOCHANGE(cache_lease_refs_init(&r,0));assert(!cache_lease_refs_init(0,1));
    assert(cache_lease_refs_init(&r,9));assert(cache_lease_refs_init(&other,9));
    NOCHANGE(cache_lease_refs_acquire(&r,&t,&a,8));
    NOCHANGE(cache_lease_refs_acquire(&r,&t,0,9));
    assert(cache_lease_refs_acquire(&r,&t,&a,9));assert(r.refs==1&&r.head==&t);
    assert(cache_lease_refs_admit(&r,&t,&a,9));
    NOCHANGE(cache_lease_refs_acquire(&r,&t,&a,9));
    NOCHANGE(cache_lease_refs_acquire(&r,&u,&a,9));assert(!u.linked);
    assert(!cache_lease_refs_acquire(&other,&t,&a,9)&&other.refs==0);
    NOCHANGE(cache_lease_refs_release(&r,&t,&b,9));
    NOCHANGE(cache_lease_refs_release(&r,&t,&a,8));
    assert(!cache_lease_refs_release(&other,&t,&a,9));
    NOCHANGE(cache_lease_refs_advance(&r,9,10));
    /* Forged copy cannot release/admit without intrusive identity. */
    struct cache_lease_token copy=t;
    assert(!cache_lease_refs_admit(&r,&copy,&a,9));
    assert(!cache_lease_refs_release(&r,&copy,&a,9));assert(r.refs==1);
    assert(cache_lease_refs_release(&r,&t,&a,9));assert(!r.refs&&!r.head&&!t.linked);
    NOCHANGE(cache_lease_refs_release(&r,&t,&a,9));
    NOCHANGE(cache_lease_refs_advance(&r,8,10));NOCHANGE(cache_lease_refs_advance(&r,9,9));
    assert(cache_lease_refs_advance(&r,9,10));
    NOCHANGE(cache_lease_refs_acquire(&r,&t,&a,9));
    assert(cache_lease_refs_acquire(&r,&t,&a,10));
    cache_lease_refs_poison(&r);assert(r.refs==1);
    assert(!cache_lease_refs_admit(&r,&t,&a,10));
    NOCHANGE(cache_lease_refs_acquire(&r,&u,&b,10));
    NOCHANGE(cache_lease_refs_advance(&r,10,11));
    assert(cache_lease_refs_release(&r,&t,&a,10));assert(r.phase==CACHE_LEASE_REFS_POISON);
    NOCHANGE(cache_lease_refs_acquire(&r,&t,&a,10));
    /* Fresh ledger only, demonstrate no fixed context-capacity limit. */
    assert(cache_lease_refs_init(&r,1));
    for(unsigned i=0;i<COUNT;i++)assert(cache_lease_refs_acquire(&r,&tokens[i],&owners[i],1));
    assert(r.refs==COUNT);
    /* Interior removals and end/head unlinking all preserve membership. */
    for(unsigned i=0;i<COUNT;i+=2)assert(cache_lease_refs_release(&r,&tokens[i],&owners[i],1));
    for(unsigned i=1;i<COUNT;i+=2){assert(cache_lease_refs_admit(&r,&tokens[i],&owners[i],1));assert(cache_lease_refs_release(&r,&tokens[i],&owners[i],1));}
    assert(!r.refs&&!r.head);
    r.refs=UINT64_MAX;NOCHANGE(cache_lease_refs_acquire(&r,&t,&a,1));r.refs=0;
    assert(cache_lease_refs_advance(&r,1,UINT64_MAX));
    NOCHANGE(cache_lease_refs_advance(&r,UINT64_MAX,0));
    NOCHANGE(cache_lease_refs_advance(&r,UINT64_MAX,UINT64_MAX));
    t.owner=&a;NOCHANGE(cache_lease_refs_acquire(&r,&t,&a,UINT64_MAX));t=(struct cache_lease_token){0};
    assert(!cache_lease_refs_acquire(0,&t,&a,1));assert(!cache_lease_refs_acquire(&r,0,&a,1));
    assert(!cache_lease_refs_release(0,&t,&a,1));assert(!cache_lease_refs_admit(0,&t,&a,1));
    cache_lease_refs_poison(0);
    puts("cache_lease_refs: PASS");
}
