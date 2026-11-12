#include "../nested_domain.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define NO(call) do {struct nested_domain old=d;assert(!(call));assert(!memcmp(&d,&old,sizeof d));}while(0)
static void entry(struct nested_domain *d,unsigned cpu,struct nested_domain_token t,bool resume,enum nested_domain_outcome o)
{
    assert(nested_domain_prepare(d,cpu,t,resume));assert(nested_domain_start(d,cpu,t));
    struct nested_domain_cpu_token a={d->id,d->incarnation,cpu};
    assert(nested_domain_guest_enter(d,a));assert(nested_domain_guest_leave(d,a));
    assert(nested_domain_finish(d,cpu,t,o));
}
static struct nested_domain_reclaim_ticket stale(struct nested_domain_reclaim_ticket t)
{--t.epoch;return t;}
int main(void)
{
    struct nested_domain d={0};struct nested_domain_token a,b,c;
    struct nested_domain_reclaim_ticket epoch={99,99,123};
    NO(nested_domain_init(&d,0,1,3));NO(nested_domain_init(&d,1,0,3));NO(nested_domain_init(&d,1,1,256));
    assert(nested_domain_init(&d,10,20,7));
    struct nested_domain_cpu_token actor={10,20,2};
    assert(nested_domain_guest_enter(&d,actor)); /* L1 consumer without virtual VMXON */
    NO(nested_domain_reclaim_begin(&d,&epoch));assert(epoch.epoch==123);
    NO(nested_domain_guest_enter(&d,actor));
    assert(nested_domain_guest_leave(&d,actor));NO(nested_domain_guest_leave(&d,actor));
    actor.incarnation++;NO(nested_domain_guest_enter(&d,actor));actor.incarnation--;
    assert(nested_domain_vmxon(&d,0,0x1000));assert(nested_domain_vmxon(&d,1,0x2000));
    NO(nested_domain_vmxon(&d,1,0x3000));NO(nested_domain_vmxon(&d,3,0x3000));
    NO(nested_domain_alloc(&d,0x1000,&a));NO(nested_domain_alloc(&d,0x3001,&a));
    assert(nested_domain_alloc(&d,0x3000,&a));assert(nested_domain_alloc(&d,0x4000,&b));
    assert(nested_domain_load(&d,0,a));assert(nested_domain_load(&d,0,b));
    assert(d.objects[a.slot].active&&d.objects[a.slot].owner==0);
    NO(nested_domain_load(&d,1,a));NO(nested_domain_clear(&d,1,a));NO(nested_domain_vmxoff(&d,0));
    NO(nested_domain_delete(&d,a));NO(nested_domain_prepare(&d,0,a,false));
    assert(nested_domain_load(&d,0,a));NO(nested_domain_prepare(&d,0,a,true));
    assert(nested_domain_prepare(&d,0,a,false));
    NO(nested_domain_load(&d,0,b));NO(nested_domain_clear(&d,0,a));NO(nested_domain_reclaim_begin(&d,&epoch));
    NO(nested_domain_finish(&d,0,a,NESTED_DOMAIN_EXIT));
    struct nested_domain_cpu_token cpu0={10,20,0};
    NO(nested_domain_guest_enter(&d,cpu0)); /* prepared must start first */
    assert(nested_domain_finish(&d,0,a,NESTED_DOMAIN_EARLY_FAIL));assert(!d.objects[a.slot].launched);
    assert(nested_domain_prepare(&d,0,a,false));assert(nested_domain_start(&d,0,a));
    NO(nested_domain_finish(&d,0,a,NESTED_DOMAIN_EARLY_FAIL));
    assert(nested_domain_finish(&d,0,a,NESTED_DOMAIN_FAILED_ENTRY));
    entry(&d,0,a,false,NESTED_DOMAIN_FAILED_ENTRY);assert(!d.objects[a.slot].launched);
    entry(&d,0,a,false,NESTED_DOMAIN_EXIT);assert(d.objects[a.slot].launched);
    NO(nested_domain_prepare(&d,0,a,false));entry(&d,0,a,true,NESTED_DOMAIN_FAILED_ENTRY);
    assert(d.objects[a.slot].launched);NO(nested_domain_finish(&d,0,a,NESTED_DOMAIN_EXIT));
    assert(nested_domain_clear(&d,0,a));assert(!d.objects[a.slot].launched);
    assert(nested_domain_load(&d,1,a));entry(&d,1,a,false,NESTED_DOMAIN_EXIT);
    assert(nested_domain_clear(&d,1,a));assert(nested_domain_delete(&d,a));
    assert(nested_domain_alloc(&d,0x3000,&c));assert(c.slot==a.slot&&c.generation>a.generation);
    NO(nested_domain_load(&d,1,a));NO(nested_domain_delete(&d,a));
    struct nested_domain_token foreign=c;foreign.incarnation++;
    NO(nested_domain_load(&d,1,foreign));foreign=c;foreign.domain_id++;NO(nested_domain_delete(&d,foreign));
    struct nested_domain_invept_ticket tick,wrong;
    assert(nested_domain_invept_begin(&d,0,1,0x8000,&tick));
    assert(d.cpus[0].cache_generation==1&&d.cpus[1].cache_generation==1);
    NO(nested_domain_invept_begin(&d,0,2,0,&wrong));NO(nested_domain_reclaim_begin(&d,&epoch));
    wrong=tick;wrong.cpu=1;NO(nested_domain_invept_complete(&d,wrong));
    wrong=tick;wrong.incarnation++;NO(nested_domain_invept_complete(&d,wrong));
    wrong=tick;wrong.domain_id++;NO(nested_domain_invept_complete(&d,wrong));
    wrong=tick;wrong.root+=4096;NO(nested_domain_invept_complete(&d,wrong));
    wrong=tick;wrong.type=2;NO(nested_domain_invept_complete(&d,wrong));
    wrong=tick;wrong.sequence++;NO(nested_domain_invept_complete(&d,wrong));
    assert(nested_domain_invept_complete(&d,tick));NO(nested_domain_invept_complete(&d,tick));
    assert(d.cpus[0].cache_generation==2&&d.cpus[1].cache_generation==1);
    assert(nested_domain_invept_begin(&d,1,2,UINT64_MAX,&tick));assert(tick.root==0);
    assert(nested_domain_invept_complete(&d,tick));
    NO(nested_domain_invept_begin(&d,1,3,0,&tick));NO(nested_domain_invept_begin(&d,1,1,1,&tick));
    assert(nested_domain_clear(&d,0,b));assert(nested_domain_vmxoff(&d,0));assert(nested_domain_vmxoff(&d,1));
    assert(d.residency==7); /* includes independent L1 consumer CPU2 and OFF CPU0/1 */
    assert(nested_domain_reclaim_begin(&d,&epoch));assert(d.targets==7);
    NO(nested_domain_alloc(&d,0x9000,&a));NO(nested_domain_guest_enter(&d,actor));
    NO(nested_domain_reclaim_ack(&d,0,epoch));NO(nested_domain_reclaim_complete(&d,epoch));
    struct nested_domain_reclaim_ticket foreign_revoke=epoch;
    foreign_revoke.domain_id++;
    NO(nested_domain_reclaim_publish(&d,foreign_revoke));
    NO(nested_domain_reclaim_publish(&d,stale(epoch)));assert(nested_domain_reclaim_publish(&d,epoch));
    NO(nested_domain_reclaim_ack(&d,0,foreign_revoke));NO(nested_domain_reclaim_complete(&d,foreign_revoke));
    foreign_revoke=epoch;foreign_revoke.incarnation++;
    NO(nested_domain_reclaim_ack(&d,0,foreign_revoke));NO(nested_domain_reclaim_complete(&d,foreign_revoke));
    NO(nested_domain_reclaim_publish(&d,epoch));NO(nested_domain_reclaim_ack(&d,3,epoch));
    NO(nested_domain_reclaim_ack(&d,0,stale(epoch)));assert(nested_domain_reclaim_ack(&d,0,epoch));
    NO(nested_domain_reclaim_ack(&d,0,epoch));assert(nested_domain_reclaim_ack(&d,1,epoch));
    NO(nested_domain_reclaim_complete(&d,epoch));assert(nested_domain_reclaim_ack(&d,2,epoch));
    assert(d.gate==NESTED_DOMAIN_REVOKING);assert(nested_domain_reclaim_complete(&d,epoch));
    assert(nested_domain_reclaim_begin(&d,&epoch));assert(d.targets==7); /* historical set never shrinks */
    nested_domain_fail(&d);assert(d.gate==NESTED_DOMAIN_POISON&&d.targets==7);
    NO(nested_domain_reclaim_publish(&d,epoch));NO(nested_domain_reclaim_complete(&d,epoch));
    NO(nested_domain_vmxon(&d,0,0x1000));NO(nested_domain_delete(&d,c));
    /* Capacity and every slot's exhausted generation are failure-atomic. */
    assert(nested_domain_init(&d,11,21,1));
    struct nested_domain_token list[NESTED_DOMAIN_OBJECTS];
    for(unsigned i=0;i<NESTED_DOMAIN_OBJECTS;i++)assert(nested_domain_alloc(&d,0x1000+(uint64_t)i*4096,&list[i]));
    a=(struct nested_domain_token){99,99,99,99};
    NO(nested_domain_alloc(&d,0x20000,&a));assert(a.domain_id==99&&a.slot==99);
    for(unsigned i=0;i<NESTED_DOMAIN_OBJECTS;i++){assert(nested_domain_delete(&d,list[i]));d.objects[i].generation=UINT64_MAX;}
    NO(nested_domain_alloc(&d,0x20000,&a));
    d.revoke_epoch=UINT64_MAX;epoch=(struct nested_domain_reclaim_ticket){99,99,123};NO(nested_domain_reclaim_begin(&d,&epoch));assert(epoch.epoch==123);
    assert(nested_domain_vmxon(&d,0,0x30000));d.cpus[0].cache_generation=UINT64_MAX;
    NO(nested_domain_invept_begin(&d,0,2,0,&tick));
    /* Empty historical set still requires publication + explicit completion. */
    assert(nested_domain_init(&d,11,22,1));assert(nested_domain_reclaim_begin(&d,&epoch));
    NO(nested_domain_reclaim_complete(&d,epoch));assert(nested_domain_reclaim_publish(&d,epoch));
    NO(nested_domain_reclaim_ack(&d,0,epoch));assert(nested_domain_reclaim_complete(&d,epoch));
    puts("nested_domain: PASS");
}
