#include "nested_domain_dispatch.h"
static bool lock_raw(struct nested_domain_dispatch *s)
{
    for(unsigned i=0;i<NESTED_DISPATCH_LOCK_TRIES;i++){
        uint32_t expected=0;
        if(__atomic_compare_exchange_n(&s->lock,&expected,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return true;
        __asm__ volatile("pause":::"memory");
    }
    return false;
}
static void unlock(struct nested_domain_dispatch *s){__atomic_store_n(&s->lock,0,__ATOMIC_RELEASE);}
static bool lock(struct nested_domain_dispatch *s)
{
    if(!s||__atomic_load_n(&s->fault_latch,__ATOMIC_ACQUIRE))return false;
    if(!lock_raw(s)){__atomic_store_n(&s->fault_latch,1,__ATOMIC_RELEASE);return false;}
    if(__atomic_load_n(&s->fault_latch,__ATOMIC_ACQUIRE)){unlock(s);return false;}
    return true;
}
static bool assigned(const struct nested_domain_dispatch *s,unsigned cpu,struct nested_domain_token t,const struct nvmx *m)
{
    if(cpu>=2||!m)return false;
    const struct nested_domain_token *a=&s->tokens[cpu];
    return t.domain_id==a->domain_id&&t.incarnation==a->incarnation&&t.slot==a->slot&&t.generation==a->generation&&
        t.domain_id==s->domain.id&&t.incarnation==s->domain.incarnation&&
        t.slot<NESTED_DOMAIN_OBJECTS&&s->domain.objects[t.slot].used&&s->domain.objects[t.slot].generation==t.generation&&
        m->on_region==(cpu?0x2d000u:0xd000u)&&m->vmcs_region==(cpu?0x2e000u:0xe000u);
}
static bool coherent(const struct nested_domain *d,unsigned cpu,struct nested_domain_token t,const struct nvmx *m)
{
    const struct nested_domain_vmcs *o=&d->objects[t.slot];
    return d->gate==NESTED_DOMAIN_OPEN&&!m->poisoned&&m->root==d->cpus[cpu].root&&
        m->active==o->active&&(!o->active||o->owner==cpu)&&m->launched==o->launched&&
        (m->current==NVMX_NONE?d->cpus[cpu].current==NESTED_DOMAIN_NONE:
            m->current==o->gpa&&d->cpus[cpu].current==t.slot);
}
static void fatal_locked(struct nested_domain_dispatch *s)
{nested_domain_fail(&s->domain);__atomic_store_n(&s->fault_latch,1,__ATOMIC_RELEASE);}
bool nested_domain_dispatch_init(struct nested_domain_dispatch *s,uint64_t id,uint64_t incarnation)
{
    if(!s||!id||!incarnation)return false;
    struct nested_domain_dispatch next={0};
    if(!nested_domain_init(&next.domain,id,incarnation,3)||
       !nested_domain_alloc(&next.domain,0xe000,&next.tokens[0])||
       !nested_domain_alloc(&next.domain,0x2e000,&next.tokens[1]))return false;
    *s=next;return true;
}
bool nested_domain_dispatch_execute(struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_token t,struct nvmx *local,struct nvmx_request request,struct nvmx_result *out)
{
    if(!local||!out||!lock(s))return false;
    bool ok=false;
    if(!assigned(s,cpu,t,local)||!coherent(&s->domain,cpu,t,local)||
       s->internal_pending[cpu]||s->domain.cpus[cpu].guest_running||local->prepared||local->running)goto done;
    /* Cross-owner region access is outside this fixed profile, irrespective
     * of what the private single-object VMX model would otherwise return. */
    if((request.op==NVMX_PTRLD||request.op==NVMX_CLEAR)&&
       request.operand==(cpu?0xe000u:0x2e000u))goto done;
    struct nvmx candidate=*local;
    struct nested_domain domain=s->domain;
    struct nvmx_result result=nvmx_execute(&candidate,request);
    bool accepted=true;
    if(result.status==NVMX_SUCCESS){
        switch(request.op){
        case NVMX_ON:accepted=nested_domain_vmxon(&domain,cpu,request.operand);break;
        case NVMX_OFF:accepted=nested_domain_vmxoff(&domain,cpu);break;
        case NVMX_PTRLD:accepted=nested_domain_load(&domain,cpu,t);break;
        case NVMX_CLEAR:{
            struct nested_domain_vmcs *o=&domain.objects[t.slot];
            if(o->active)accepted=nested_domain_clear(&domain,cpu,t);
            else accepted=!o->prepared&&!o->running&&!o->launched&&domain.cpus[cpu].current!=t.slot;
            break;
        }
        default:break;
        }
    }else if(result.status==NVMX_ENTRY_REQUEST){
        accepted=(request.op==NVMX_LAUNCH||request.op==NVMX_RESUME)&&
            nested_domain_prepare(&domain,cpu,t,request.op==NVMX_RESUME);
    }
    if(!accepted)goto done;
    s->domain=domain;*local=candidate;*out=result;ok=true;
done:unlock(s);return ok;
}
bool nested_domain_dispatch_physical_enter(struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_token t,struct nvmx *local,bool l2)
{
    if(!local||!lock(s))return false;
    bool ok=false;
    if(!assigned(s,cpu,t,local)||!coherent(&s->domain,cpu,t,local)||s->internal_pending[cpu])goto done;
    struct nested_domain domain=s->domain;
    if(l2){
        if(!local->prepared||local->running||!nested_domain_start(&domain,cpu,t))goto done;
    }else if(local->prepared||local->running||domain.objects[t.slot].prepared||domain.objects[t.slot].running)goto done;
    struct nested_domain_cpu_token actor={domain.id,domain.incarnation,cpu};
    if(!nested_domain_guest_enter(&domain,actor))goto done;
    s->domain=domain;ok=true;
done:unlock(s);return ok;
}
bool nested_domain_dispatch_physical_leave(struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_token t,struct nvmx *local,bool l2,unsigned status,uint64_t reason)
{
    if(!local||!lock(s))return false;
    bool ok=false;
    if(!assigned(s,cpu,t,local)||!coherent(&s->domain,cpu,t,local))goto fail;
    if(status||(reason&(UINT64_C(1)<<31)))goto fail;
    struct nested_domain domain=s->domain;struct nvmx candidate=*local;
    struct nested_domain_cpu_token actor={domain.id,domain.incarnation,cpu};
    if(!nested_domain_guest_leave(&domain,actor))goto fail;
    if(l2){
        if(reason!=18)goto fail;
        if(s->internal_pending[cpu]){
            if((s->internal_origin[cpu]!=NESTED_INTERNAL_KICK&&s->internal_origin[cpu]!=NESTED_INTERNAL_EPT)||
               !s->internal_resumed[cpu]||!candidate.running||candidate.prepared||
               !candidate.launched||domain.objects[t.slot].prepared||domain.objects[t.slot].running)goto fail;
        }else if(!nested_domain_finish(&domain,cpu,t,NESTED_DOMAIN_EXIT)||!nvmx_entry_commit(&candidate))goto fail;
    }else if(s->internal_pending[cpu]||candidate.prepared||candidate.running||domain.objects[t.slot].prepared||domain.objects[t.slot].running)goto fail;
    s->domain=domain;*local=candidate;
    s->internal_pending[cpu]=false;s->internal_resumed[cpu]=false;
    s->internal_origin[cpu]=NESTED_INTERNAL_NONE;ok=true;goto done;
fail:fatal_locked(s);
done:unlock(s);return ok;
}
void nested_domain_dispatch_markfail(struct nested_domain_dispatch *s)
{
    if(!s)return;
    __atomic_store_n(&s->fault_latch,1,__ATOMIC_RELEASE);
    if(lock_raw(s)){nested_domain_fail(&s->domain);unlock(s);}
}
bool nested_domain_dispatch_final(struct nested_domain_dispatch *s)
{
    if(!lock(s))return false;
    bool ok=s->domain.gate==NESTED_DOMAIN_OPEN&&s->domain.residency==3;
    for(unsigned i=0;i<2;i++)if(s->internal_pending[i]||s->internal_resumed[i]||s->internal_origin[i]!=NESTED_INTERNAL_NONE)ok=false;
    for(unsigned i=0;i<NESTED_DOMAIN_CPUS;i++)if(s->domain.cpus[i].root||s->domain.cpus[i].guest_running||
        s->domain.cpus[i].pending_ticket||s->domain.cpus[i].current!=NESTED_DOMAIN_NONE)ok=false;
    for(unsigned i=0;i<NESTED_DOMAIN_OBJECTS;i++)if(s->domain.objects[i].active||s->domain.objects[i].launched||
        s->domain.objects[i].prepared||s->domain.objects[i].running)ok=false;
    unlock(s);return ok;
}

bool nested_domain_dispatch_reclaim_begin(struct nested_domain_dispatch *s,
    struct nested_domain_reclaim_ticket *out)
{
    if(!out||!lock(s))return false;
    bool ok=nested_domain_reclaim_begin(&s->domain,out);
    unlock(s);return ok;
}
bool nested_domain_dispatch_reclaim_publish(struct nested_domain_dispatch *s,
    struct nested_domain_reclaim_ticket ticket)
{
    if(!lock(s))return false;
    bool ok=nested_domain_reclaim_publish(&s->domain,ticket);
    unlock(s);return ok;
}
bool nested_domain_dispatch_reclaim_ack(struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_reclaim_ticket ticket)
{
    if(!lock(s))return false;
    bool ok=nested_domain_reclaim_ack(&s->domain,cpu,ticket);
    unlock(s);return ok;
}
bool nested_domain_dispatch_reclaim_complete(struct nested_domain_dispatch *s,
    struct nested_domain_reclaim_ticket ticket)
{
    if(!lock(s))return false;
    bool ok=nested_domain_reclaim_complete(&s->domain,ticket);
    unlock(s);return ok;
}

bool nested_domain_dispatch_physical_pause(struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_token t,struct nvmx *local,unsigned status,uint64_t reason)
{
    if(!local||!lock(s))return false;
    bool ok=false;
    if(!assigned(s,cpu,t,local))goto fail;
    if(status||reason!=1)goto fail;
    if(s->internal_used[cpu]||s->internal_pending[cpu]||s->ept_suspends[cpu]||
       s->internal_origin[cpu]!=NESTED_INTERNAL_NONE)goto done;
    if(!coherent(&s->domain,cpu,t,local)||!local->prepared||local->running||
       !s->domain.objects[t.slot].prepared||!s->domain.objects[t.slot].running)goto fail;
    struct nested_domain domain=s->domain;struct nvmx candidate=*local;
    struct nested_domain_cpu_token actor={domain.id,domain.incarnation,cpu};
    if(!nested_domain_guest_leave(&domain,actor)||
       !nested_domain_finish(&domain,cpu,t,NESTED_DOMAIN_EXIT)||!nvmx_entry_commit(&candidate))goto fail;
    s->domain=domain;*local=candidate;
    s->internal_pending[cpu]=true;s->internal_resumed[cpu]=false;s->internal_used[cpu]=true;
    s->internal_origin[cpu]=NESTED_INTERNAL_KICK;
    ok=true;goto done;
fail:fatal_locked(s);
done:unlock(s);return ok;
}
bool nested_domain_dispatch_physical_resume(struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_token t,struct nvmx *local)
{
    if(!local||!lock(s))return false;
    bool ok=false;
    if(!assigned(s,cpu,t,local)||!coherent(&s->domain,cpu,t,local)||
       (s->internal_origin[cpu]!=NESTED_INTERNAL_KICK&&s->internal_origin[cpu]!=NESTED_INTERNAL_EPT)||
       !s->internal_pending[cpu]||s->internal_resumed[cpu]||!local->running||
       local->prepared||!local->launched||s->domain.objects[t.slot].prepared||
       s->domain.objects[t.slot].running)goto done;
    struct nested_domain domain=s->domain;
    struct nested_domain_cpu_token actor={domain.id,domain.incarnation,cpu};
    if(!nested_domain_guest_enter(&domain,actor))goto done;
    s->domain=domain;s->internal_resumed[cpu]=true;ok=true;
done:unlock(s);return ok;
}


bool nested_domain_dispatch_physical_suspend_ept(struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_token t,struct nvmx *local,unsigned status,uint64_t reason)
{
    if(!local||!lock(s))return false;
    bool ok=false;
    if(!assigned(s,cpu,t,local)||status||reason!=48)goto fail;
    if(s->internal_used[cpu]||s->internal_origin[cpu]==NESTED_INTERNAL_KICK||
       s->ept_suspends[cpu]==UINT32_MAX)goto done;
    if(!coherent(&s->domain,cpu,t,local))goto done;
    struct nested_domain domain=s->domain;struct nvmx candidate=*local;
    struct nested_domain_cpu_token actor={domain.id,domain.incarnation,cpu};
    if(s->internal_pending[cpu]){
        if(s->internal_origin[cpu]!=NESTED_INTERNAL_EPT||!s->internal_resumed[cpu]||
           !candidate.running||candidate.prepared||!candidate.launched||
           domain.objects[t.slot].prepared||domain.objects[t.slot].running)goto done;
        if(!nested_domain_guest_leave(&domain,actor))goto fail;
    }else{
        if(s->internal_origin[cpu]!=NESTED_INTERNAL_NONE||s->internal_resumed[cpu]||
           !candidate.prepared||candidate.running||!domain.objects[t.slot].prepared||
           !domain.objects[t.slot].running)goto fail;
        if(!nested_domain_guest_leave(&domain,actor)||
           !nested_domain_finish(&domain,cpu,t,NESTED_DOMAIN_EXIT)||
           !nvmx_entry_commit(&candidate))goto fail;
    }
    s->domain=domain;*local=candidate;
    s->internal_origin[cpu]=NESTED_INTERNAL_EPT;
    s->internal_pending[cpu]=true;s->internal_resumed[cpu]=false;
    s->ept_suspends[cpu]++;
    ok=true;goto done;
fail:fatal_locked(s);
done:unlock(s);return ok;
}

/* Serialized policy only: the caller executes the owner-local hardware flush
 * between begin and complete, never under this metadata lock. */
static bool invept_actor(const struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_token token,const struct nvmx *local)
{
    return assigned(s,cpu,token,local)&&coherent(&s->domain,cpu,token,local)&&
        !s->domain.cpus[cpu].guest_running&&!local->prepared&&!local->running&&
        !s->internal_pending[cpu]&&!s->domain.objects[token.slot].prepared&&
        !s->domain.objects[token.slot].running;
}
bool nested_domain_dispatch_local_invept_begin(struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_token token,struct nvmx *local,unsigned type,uint64_t root,
    struct nested_domain_invept_ticket *out)
{
    if(!out||!local||!lock(s))return false;
    bool ok=invept_actor(s,cpu,token,local)&&
        nested_domain_invept_begin(&s->domain,cpu,type,root,out);
    unlock(s);return ok;
}
bool nested_domain_dispatch_local_invept_complete(struct nested_domain_dispatch *s,unsigned cpu,
    struct nested_domain_token token,struct nvmx *local,struct nested_domain_invept_ticket ticket)
{
    if(!local||!lock(s))return false;
    bool ok=ticket.cpu==cpu&&invept_actor(s,cpu,token,local)&&
        nested_domain_invept_complete(&s->domain,ticket);
    unlock(s);return ok;
}

/* Called only under the dispatch lock. SOURCE_REPLACE is narrower than
 * ordinary maintenance: both fixed owners must have fully left L2, not merely
 * suspended their physical consumer for internal EPT repair. */
static bool source_checkpoint(const struct nested_domain_dispatch *s)
{
 const struct nested_domain *d=&s->domain;
 if(d->participants!=3||d->residency!=3)return false;
 for(unsigned cpu=0;cpu<2;cpu++){
  const struct nested_domain_token *t=&s->tokens[cpu];
  if(t->domain_id!=d->id||t->incarnation!=d->incarnation||t->slot>=NESTED_DOMAIN_OBJECTS||
     !t->generation)return false;
  const struct nested_domain_vmcs *o=&d->objects[t->slot];
  const struct nested_domain_cpu *c=&d->cpus[cpu];
  if(!o->used||o->generation!=t->generation||o->gpa!=(cpu?0x2e000u:0xe000u)||
     !o->active||o->owner!=cpu||!o->launched||o->prepared||o->running||
     !c->root||c->vmxon!=(cpu?0x2d000u:0xd000u)||c->current!=t->slot||
     c->guest_running||c->pending_ticket||s->internal_pending[cpu]||
     s->internal_resumed[cpu]||s->internal_origin[cpu]!=NESTED_INTERNAL_NONE)return false;
 }
 for(unsigned cpu=0;cpu<NESTED_DOMAIN_CPUS;cpu++)
  if(d->cpus[cpu].guest_running||d->cpus[cpu].pending_ticket)return false;
 for(unsigned slot=0;slot<NESTED_DOMAIN_OBJECTS;slot++)
  if(d->objects[slot].prepared||d->objects[slot].running)return false;
 return true;
}

bool nested_domain_dispatch_source_plan(struct nested_domain_dispatch *s,
 struct nested_domain_maintenance_ticket *out)
{
 if(!out||!lock(s))return false;
 const struct nested_domain *d=&s->domain;
 bool ok=d->id&&d->incarnation&&d->gate==NESTED_DOMAIN_OPEN&&
  d->maintenance_epoch!=UINT64_MAX&&source_checkpoint(s);
 if(ok)*out=(struct nested_domain_maintenance_ticket){d->id,d->incarnation,
  d->maintenance_epoch+1,NESTED_DOMAIN_SOURCE_REPLACE};
 unlock(s);return ok;
}

bool nested_domain_dispatch_maintenance_begin(struct nested_domain_dispatch *s,unsigned purpose,struct nested_domain_maintenance_ticket *out)
{
 if(!lock(s))return false;
 bool ok=(purpose!=NESTED_DOMAIN_SOURCE_REPLACE||source_checkpoint(s))&&
  nested_domain_maintenance_begin(&s->domain,purpose,out);
 unlock(s);return ok;
}

bool nested_domain_dispatch_maintenance_publish(struct nested_domain_dispatch *s,struct nested_domain_maintenance_ticket t)
{
 if(!lock(s))return false;
 bool ok=nested_domain_maintenance_publish(&s->domain,t);
 unlock(s);return ok;
}

bool nested_domain_dispatch_maintenance_ack(struct nested_domain_dispatch *s,unsigned cpu,struct nested_domain_maintenance_ticket t)
{
 if(!lock(s))return false;
 bool ok=nested_domain_maintenance_ack(&s->domain,cpu,t);
 unlock(s);return ok;
}

bool nested_domain_dispatch_maintenance_complete(struct nested_domain_dispatch *s,struct nested_domain_maintenance_ticket t)
{
 if(!lock(s))return false;
 bool ok=nested_domain_maintenance_complete(&s->domain,t);
 unlock(s);return ok;
}

bool nested_domain_dispatch_consumer_stopped(struct nested_domain_dispatch *s,unsigned cpu,struct nested_domain_token t)
{
 if(cpu>=2||!lock(s))return false;
 const struct nested_domain_token *a=&s->tokens[cpu];
 bool ok=t.domain_id==a->domain_id&&t.incarnation==a->incarnation&&t.slot==a->slot&&t.generation==a->generation&&
  t.domain_id==s->domain.id&&t.incarnation==s->domain.incarnation&&
  t.slot<NESTED_DOMAIN_OBJECTS&&s->domain.objects[t.slot].used&&s->domain.objects[t.slot].generation==t.generation&&
  s->domain.gate==NESTED_DOMAIN_OPEN&&!s->domain.cpus[cpu].guest_running;
 unlock(s);return ok;
}


bool nested_domain_dispatch_maintenance_stopped(struct nested_domain_dispatch *s,unsigned cpu,
 struct nested_domain_token t,struct nested_domain_maintenance_ticket ticket)
{
 if(cpu>=2||!lock(s))return false;
 const struct nested_domain_token *a=&s->tokens[cpu];
 const struct nested_domain *d=&s->domain;
 bool ok=t.domain_id==a->domain_id&&t.incarnation==a->incarnation&&t.slot==a->slot&&t.generation==a->generation&&
  t.domain_id==d->id&&t.incarnation==d->incarnation&&t.slot<NESTED_DOMAIN_OBJECTS&&
  d->objects[t.slot].used&&d->objects[t.slot].generation==t.generation&&
  d->objects[t.slot].active&&d->objects[t.slot].owner==cpu&&!d->objects[t.slot].prepared&&!d->objects[t.slot].running&&
  ticket.domain_id==d->id&&ticket.incarnation==d->incarnation&&ticket.epoch==d->maintenance_epoch&&ticket.epoch&&
  ticket.purpose==NESTED_DOMAIN_AD_RESET&&ticket.purpose==d->maintenance_purpose&&
  d->gate==NESTED_DOMAIN_MAINTENANCE&&d->participants==3&&d->maintenance_targets==3&&
  d->cpus[cpu].root&&d->cpus[cpu].current==t.slot&&!d->cpus[cpu].guest_running&&!d->cpus[cpu].pending_ticket&&
  !s->internal_pending[cpu]&&!s->internal_resumed[cpu]&&s->internal_origin[cpu]==NESTED_INTERNAL_NONE;
 unlock(s);return ok;
}

bool nested_domain_dispatch_source_stopped(struct nested_domain_dispatch *s,unsigned cpu,
 struct nested_domain_token t,struct nested_domain_maintenance_ticket ticket)
{
 if(cpu>=2||!lock(s))return false;
 const struct nested_domain_token *a=&s->tokens[cpu];
 const struct nested_domain *d=&s->domain;
 bool ok=t.domain_id==a->domain_id&&t.incarnation==a->incarnation&&t.slot==a->slot&&t.generation==a->generation&&
  ticket.domain_id==d->id&&ticket.incarnation==d->incarnation&&ticket.epoch==d->maintenance_epoch&&ticket.epoch&&
  ticket.purpose==NESTED_DOMAIN_SOURCE_REPLACE&&ticket.purpose==d->maintenance_purpose&&
  d->gate==NESTED_DOMAIN_MAINTENANCE&&d->maintenance_targets==3&&source_checkpoint(s);
 unlock(s);return ok;
}
