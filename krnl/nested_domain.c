#include "nested_domain.h"
#include <stddef.h>
static bool open(const struct nested_domain *d)
{return d&&d->id&&d->incarnation&&d->gate==NESTED_DOMAIN_OPEN;}
static bool cpu_ok(const struct nested_domain *d,unsigned cpu)
{return d&&cpu<NESTED_DOMAIN_CPUS&&(d->participants&(UINT64_C(1)<<cpu));}
static bool page(uint64_t p){return p&&!(p&4095)&&p<(UINT64_C(1)<<48);}
static struct nested_domain_vmcs *object(struct nested_domain *d,struct nested_domain_token t)
{
    if(!d||t.domain_id!=d->id||t.incarnation!=d->incarnation||t.slot>=NESTED_DOMAIN_OBJECTS)return NULL;
    struct nested_domain_vmcs *o=&d->objects[t.slot];
    return o->used&&o->generation==t.generation?o:NULL;
}
static bool occupied(const struct nested_domain *d,uint64_t gpa)
{
    for(unsigned i=0;i<NESTED_DOMAIN_CPUS;i++)if(d->cpus[i].root&&d->cpus[i].vmxon==gpa)return true;
    for(unsigned i=0;i<NESTED_DOMAIN_OBJECTS;i++)if(d->objects[i].used&&d->objects[i].gpa==gpa)return true;
    return false;
}
static bool pending(const struct nested_domain *d,unsigned cpu)
{
    for(unsigned i=0;i<NESTED_DOMAIN_OBJECTS;i++){
        const struct nested_domain_vmcs *o=&d->objects[i];
        if(o->active&&o->owner==cpu&&(o->prepared||o->running))return true;
    }
    return false;
}
static bool local(const struct nested_domain *d,unsigned cpu)
{return open(d)&&cpu_ok(d,cpu)&&d->cpus[cpu].root&&!d->cpus[cpu].pending_ticket&&!d->cpus[cpu].guest_running;}
bool nested_domain_init(struct nested_domain *d,uint64_t id,uint64_t incarnation,uint64_t mask)
{
    if(!d||!id||!incarnation||!mask||(mask&~UINT64_C(255)))return false;
    *d=(struct nested_domain){.id=id,.incarnation=incarnation,.participants=mask,.revoke_epoch=1,.gate=NESTED_DOMAIN_OPEN};
    for(unsigned i=0;i<NESTED_DOMAIN_CPUS;i++){d->cpus[i].current=NESTED_DOMAIN_NONE;d->cpus[i].cache_generation=1;}
    return true;
}
bool nested_domain_vmxon(struct nested_domain *d,unsigned cpu,uint64_t gpa)
{
    if(!open(d)||!cpu_ok(d,cpu)||d->cpus[cpu].root||d->cpus[cpu].guest_running||!page(gpa)||occupied(d,gpa))return false;
    d->cpus[cpu].root=true;d->cpus[cpu].vmxon=gpa;return true;
}
bool nested_domain_vmxoff(struct nested_domain *d,unsigned cpu)
{
    if(!local(d,cpu)||pending(d,cpu))return false;
    for(unsigned i=0;i<NESTED_DOMAIN_OBJECTS;i++)if(d->objects[i].active&&d->objects[i].owner==cpu)return false;
    if(d->cpus[cpu].current!=NESTED_DOMAIN_NONE)return false;
    d->cpus[cpu].root=false;d->cpus[cpu].vmxon=0;return true;
}
bool nested_domain_alloc(struct nested_domain *d,uint64_t gpa,struct nested_domain_token *out)
{
    if(!open(d)||!out||!page(gpa)||occupied(d,gpa))return false;
    for(unsigned i=0;i<NESTED_DOMAIN_OBJECTS;i++)if(!d->objects[i].used&&d->objects[i].generation!=UINT64_MAX){
        uint64_t gen=d->objects[i].generation+1;
        d->objects[i]=(struct nested_domain_vmcs){.used=true,.gpa=gpa,.generation=gen,.owner=NESTED_DOMAIN_NONE};
        *out=(struct nested_domain_token){d->id,d->incarnation,gen,i};return true;
    }
    return false;
}
bool nested_domain_delete(struct nested_domain *d,struct nested_domain_token t)
{
    struct nested_domain_vmcs *o=object(d,t);
    if(!open(d)||!o||o->active||o->launched||o->prepared||o->running)return false;
    for(unsigned i=0;i<NESTED_DOMAIN_CPUS;i++)if(d->cpus[i].current==t.slot)return false;
    uint64_t gen=o->generation;*o=(struct nested_domain_vmcs){.generation=gen,.owner=NESTED_DOMAIN_NONE};return true;
}
bool nested_domain_load(struct nested_domain *d,unsigned cpu,struct nested_domain_token t)
{
    struct nested_domain_vmcs *o=object(d,t);
    if(!local(d,cpu)||!o||pending(d,cpu)||o->prepared||o->running||(o->active&&o->owner!=cpu))return false;
    o->active=true;o->owner=cpu;d->cpus[cpu].current=t.slot;return true;
}
bool nested_domain_clear(struct nested_domain *d,unsigned cpu,struct nested_domain_token t)
{
    struct nested_domain_vmcs *o=object(d,t);
    if(!local(d,cpu)||!o||!o->active||o->owner!=cpu||pending(d,cpu)||o->prepared||o->running)return false;
    o->active=false;o->launched=false;o->owner=NESTED_DOMAIN_NONE;
    if(d->cpus[cpu].current==t.slot)d->cpus[cpu].current=NESTED_DOMAIN_NONE;
    return true;
}
static bool current(struct nested_domain *d,unsigned cpu,struct nested_domain_token t,struct nested_domain_vmcs *o)
{return local(d,cpu)&&o&&o->active&&o->owner==cpu&&d->cpus[cpu].current==t.slot;}
bool nested_domain_prepare(struct nested_domain *d,unsigned cpu,struct nested_domain_token t,bool resume)
{
    struct nested_domain_vmcs *o=object(d,t);
    if(!current(d,cpu,t,o)||pending(d,cpu)||o->launched!=resume)return false;
    o->prepared=true;return true;
}
bool nested_domain_start(struct nested_domain *d,unsigned cpu,struct nested_domain_token t)
{
    struct nested_domain_vmcs *o=object(d,t);
    if(!current(d,cpu,t,o)||!o->prepared||o->running)return false;
    o->running=true;d->residency|=UINT64_C(1)<<cpu;return true;
}
bool nested_domain_finish(struct nested_domain *d,unsigned cpu,struct nested_domain_token t,enum nested_domain_outcome outcome)
{
    struct nested_domain_vmcs *o=object(d,t);
    if(!current(d,cpu,t,o)||!o->prepared||
       (outcome!=NESTED_DOMAIN_EXIT&&outcome!=NESTED_DOMAIN_EARLY_FAIL&&outcome!=NESTED_DOMAIN_FAILED_ENTRY)||
       (outcome!=NESTED_DOMAIN_EARLY_FAIL&&!o->running)||
       (outcome==NESTED_DOMAIN_EARLY_FAIL&&o->running))return false;
    if(outcome==NESTED_DOMAIN_EXIT)o->launched=true;
    o->prepared=false;o->running=false;return true;
}
bool nested_domain_invept_begin(struct nested_domain *d,unsigned cpu,unsigned type,uint64_t root,struct nested_domain_invept_ticket *ticket)
{
    if(!local(d,cpu)||!ticket||pending(d,cpu)||(type!=1&&type!=2)||
       (type==1&&((root&4095)||root>=(UINT64_C(1)<<48)))||d->cpus[cpu].cache_generation==UINT64_MAX)return false;
    struct nested_domain_cpu *c=&d->cpus[cpu];
    c->pending_ticket=c->cache_generation+1;c->pending_type=type;c->pending_root=type==1?root:0;
    *ticket=(struct nested_domain_invept_ticket){d->id,d->incarnation,c->pending_ticket,c->pending_root,cpu,type};return true;
}
bool nested_domain_invept_complete(struct nested_domain *d,struct nested_domain_invept_ticket t)
{
    if(!open(d)||t.domain_id!=d->id||t.incarnation!=d->incarnation||!cpu_ok(d,t.cpu)||!t.sequence)return false;
    struct nested_domain_cpu *c=&d->cpus[t.cpu];
    if(c->pending_ticket!=t.sequence||c->pending_root!=t.root||c->pending_type!=t.type)return false;
    c->cache_generation=t.sequence;c->pending_ticket=0;c->pending_type=0;c->pending_root=0;return true;
}
static bool actor(const struct nested_domain *d,struct nested_domain_cpu_token a)
{return open(d)&&a.domain_id==d->id&&a.incarnation==d->incarnation&&cpu_ok(d,a.cpu);}
bool nested_domain_guest_enter(struct nested_domain *d,struct nested_domain_cpu_token a)
{
    if(!actor(d,a)||d->cpus[a.cpu].guest_running||d->cpus[a.cpu].pending_ticket)return false;
    for(unsigned i=0;i<NESTED_DOMAIN_OBJECTS;i++)
        if(d->objects[i].active&&d->objects[i].owner==a.cpu&&d->objects[i].prepared&&!d->objects[i].running)return false;
    d->cpus[a.cpu].guest_running=true;d->residency|=UINT64_C(1)<<a.cpu;return true;
}
bool nested_domain_guest_leave(struct nested_domain *d,struct nested_domain_cpu_token a)
{
    if(!actor(d,a)||!d->cpus[a.cpu].guest_running)return false;
    d->cpus[a.cpu].guest_running=false;return true;
}
bool nested_domain_reclaim_begin(struct nested_domain *d,struct nested_domain_reclaim_ticket *ticket)
{
    if(!open(d)||!ticket||d->revoke_epoch==UINT64_MAX)return false;
    for(unsigned i=0;i<NESTED_DOMAIN_CPUS;i++)if(pending(d,i)||d->cpus[i].pending_ticket||d->cpus[i].guest_running)return false;
    d->gate=NESTED_DOMAIN_REVOKING;d->targets=d->residency;d->acks=0;d->published=false;
    ++d->revoke_epoch;*ticket=(struct nested_domain_reclaim_ticket){d->id,d->incarnation,d->revoke_epoch};return true;
}
static bool revoke_ticket(const struct nested_domain *d,struct nested_domain_reclaim_ticket t)
{return d&&d->gate==NESTED_DOMAIN_REVOKING&&t.domain_id==d->id&&t.incarnation==d->incarnation&&t.epoch==d->revoke_epoch;}
bool nested_domain_reclaim_publish(struct nested_domain *d,struct nested_domain_reclaim_ticket ticket)
{
    if(!revoke_ticket(d,ticket)||d->published)return false;
    d->published=true;return true;
}
bool nested_domain_reclaim_ack(struct nested_domain *d,unsigned cpu,struct nested_domain_reclaim_ticket ticket)
{
    if(!revoke_ticket(d,ticket)||!d->published||!cpu_ok(d,cpu))return false;
    uint64_t bit=UINT64_C(1)<<cpu;
    if(!(d->targets&bit)||(d->acks&bit))return false;
    d->acks|=bit;return true;
}
bool nested_domain_reclaim_complete(struct nested_domain *d,struct nested_domain_reclaim_ticket ticket)
{
    if(!revoke_ticket(d,ticket)||!d->published||d->acks!=d->targets)return false;
    d->gate=NESTED_DOMAIN_OPEN;return true;
}
void nested_domain_fail(struct nested_domain *d)
{if(d)d->gate=NESTED_DOMAIN_POISON;}

bool nested_domain_maintenance_begin(struct nested_domain*d,unsigned purpose,struct nested_domain_maintenance_ticket*out)
{
 if(!open(d)||!out||(purpose!=NESTED_DOMAIN_AD_RESET&&purpose!=NESTED_DOMAIN_AD_OBSERVE&&purpose!=NESTED_DOMAIN_TYPED_TRANSITION&&purpose!=NESTED_DOMAIN_TYPED_OBSERVE&&purpose!=NESTED_DOMAIN_SOURCE_REPLACE)||d->maintenance_epoch==UINT64_MAX)return false;
 for(unsigned i=0;i<NESTED_DOMAIN_CPUS;i++)
  if(pending(d,i)||d->cpus[i].guest_running||d->cpus[i].pending_ticket||
     ((d->participants&(UINT64_C(1)<<i))&&!d->cpus[i].root))return false;
 d->gate=NESTED_DOMAIN_MAINTENANCE;d->maintenance_epoch++;
 d->maintenance_targets=d->residency;d->maintenance_acks=0;
 d->maintenance_purpose=purpose;d->maintenance_published=false;
 *out=(struct nested_domain_maintenance_ticket){d->id,d->incarnation,d->maintenance_epoch,purpose};return true;
}
static bool maintenance_ticket(const struct nested_domain*d,struct nested_domain_maintenance_ticket t)
{return d&&d->gate==NESTED_DOMAIN_MAINTENANCE&&t.domain_id==d->id&&t.incarnation==d->incarnation&&
 t.epoch==d->maintenance_epoch&&t.purpose==d->maintenance_purpose;}
bool nested_domain_maintenance_publish(struct nested_domain*d,struct nested_domain_maintenance_ticket t)
{
 if(!maintenance_ticket(d,t)||d->maintenance_published||d->maintenance_acks!=d->maintenance_targets)return false;
 d->maintenance_published=true;return true;
}
bool nested_domain_maintenance_ack(struct nested_domain*d,unsigned cpu,struct nested_domain_maintenance_ticket t)
{
 if(!maintenance_ticket(d,t)||d->maintenance_published||!cpu_ok(d,cpu))return false;
 uint64_t bit=UINT64_C(1)<<cpu;
 if(!(d->maintenance_targets&bit)||(d->maintenance_acks&bit))return false;
 d->maintenance_acks|=bit;return true;
}
bool nested_domain_maintenance_complete(struct nested_domain*d,struct nested_domain_maintenance_ticket t)
{
 if(!maintenance_ticket(d,t)||!d->maintenance_published||d->maintenance_acks!=d->maintenance_targets)return false;
 d->gate=NESTED_DOMAIN_OPEN;return true;
}
