#include "../ad_entry_merge.h"
#include <assert.h>
#include <stdio.h>
struct fake {
    uint64_t word,epoch,lifetime;
    unsigned loads,guards,cas;
    unsigned stale_guard,stale_after_cas;
    uint64_t interfere;
    bool interference,spurious;
};
static bool guard(void *p,struct ad_entry_stamp s) {
    struct fake *f=p;
    if(++f->guards==f->stale_guard)++f->epoch;
    return s.domain_id==1&&s.incarnation==2&&s.context_id==3&&f->epoch==s.epoch&&f->lifetime==s.lifetime;
}
static uint64_t load(void *p){struct fake *f=p;++f->loads;return f->word;}
static bool cas(void *p,uint64_t expected,uint64_t desired) {
    struct fake *f=p;++f->cas;
    if(f->interference){f->word=f->interfere;f->interference=false;}
    bool ok=!f->spurious&&f->word==expected;
    if(ok)f->word=desired;
    if(f->cas==f->stale_after_cas)++f->epoch;
    return ok;
}
static const struct ad_entry_backend ops={guard,load,cas};
static struct fake fresh(uint64_t word){return (struct fake){.word=word,.epoch=7,.lifetime=11};}
int main(void) {
    struct ad_entry_request r={{1,2,3,7,11},0x8037,AD_ENTRY_A|AD_ENTRY_D,AD_ENTRY_A,4};
    struct fake f=fresh(0x8037);struct ad_entry_result z=ad_entry_merge(&ops,&f,&r);
    assert(z.status==AD_ENTRY_DONE&&z.changed&&f.word==0x8137&&z.comparisons==1);
    f=fresh(0x8337);z=ad_entry_merge(&ops,&f,&r);
    assert(z.status==AD_ENTRY_DONE&&!z.changed&&!f.cas&&f.word==0x8337);
    /* No stale trace D in the request: a newly cleared D is not resurrected. */
    f=fresh(0x8037);z=ad_entry_merge(&ops,&f,&r);assert(!(f.word&AD_ENTRY_D));
    /* Competing writer sets D: failed comparison then merge retains both. */
    f=fresh(0x8037);f.interference=true;f.interfere=0x8237;
    z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_DONE&&z.changed&&f.word==0x8337&&f.cas==2);
    /* Remap/permission/type identity change cannot get our A/D bits. */
    uint64_t identities[]={0x9037,0x8035,0x8007,0};
    for(unsigned i=0;i<4;i++) {
        f=fresh(0x8037);f.interference=true;f.interfere=identities[i];
        z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_REWALK&&!z.changed&&f.word==identities[i]);
    }
    f=fresh(0x8037);f.stale_guard=1;z=ad_entry_merge(&ops,&f,&r);
    assert(z.status==AD_ENTRY_STALE&&!z.changed&&!f.loads&&!f.cas);
    f=fresh(0x8037);f.lifetime++;z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_STALE&&!f.loads);
    f=fresh(0x8037);f.stale_guard=2;z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_STALE&&!f.cas);
    r.stamp.domain_id=2;f=fresh(0x8037);z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_STALE&&!f.loads);r.stamp.domain_id=1;
    r.stamp.incarnation=3;f=fresh(0x8037);z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_STALE&&!f.loads);r.stamp.incarnation=2;
    r.stamp.context_id=4;f=fresh(0x8037);z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_STALE&&!f.loads);r.stamp.context_id=3;
    /* CAS success followed by new epoch is NOT rollback/no-side-effect. */
    f=fresh(0x8037);f.stale_after_cas=1;z=ad_entry_merge(&ops,&f,&r);
    assert(z.status==AD_ENTRY_APPLIED_BUT_STALE&&z.changed&&f.word==0x8137);
    f=fresh(0x8037);f.interference=true;f.interfere=0x8237;f.stale_after_cas=1;
    z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_STALE&&!z.changed&&f.word==0x8237);
    f=fresh(0x8137);f.stale_guard=2;z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_STALE&&!z.changed&&!f.cas);
    f=fresh(0x8037);f.spurious=true;z=ad_entry_merge(&ops,&f,&r);
    assert(z.status==AD_ENTRY_RETRY&&!z.changed&&f.cas==4);
    /* Prefix uses A only; leaf dirty uses A|D. */
    r.mutable_ad=AD_ENTRY_A;r.requested_bits=AD_ENTRY_A;
    f=fresh(0x8037);z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_DONE&&f.word==0x8137);
    r.requested_bits=AD_ENTRY_D;f=fresh(0x8037);z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_INVALID&&!f.loads);
    r.mutable_ad=AD_ENTRY_A|AD_ENTRY_D;r.requested_bits=AD_ENTRY_A|AD_ENTRY_D;
    f=fresh(0x8037);z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_DONE&&f.word==0x8337);
    r.expected_identity|=AD_ENTRY_D;f=fresh(0x8037);z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_INVALID);
    r.expected_identity=0x8037;r.mutable_ad|=1;z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_INVALID);
    r.mutable_ad=AD_ENTRY_A;r.requested_bits=AD_ENTRY_A;r.attempts=0;z=ad_entry_merge(&ops,&f,&r);assert(z.status==AD_ENTRY_INVALID);
    puts("ad_entry_merge: PASS (injected backend only)");
}
