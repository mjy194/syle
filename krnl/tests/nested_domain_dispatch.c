#include "../nested_domain_dispatch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct nvmx_request req(enum nvmx_op op,uint64_t operand)
{return (struct nvmx_request){.op=op,.operand=operand,.flags=2,.region_revision=NVMX_REVISION};}
static void setup(struct nested_domain_dispatch *s,struct nvmx m[2])
{
    assert(nested_domain_dispatch_init(s,10,20));
    assert(nvmx_init(&m[0],0xd000,0xe000,48));assert(nvmx_init(&m[1],0x2d000,0x2e000,48));
}
static struct nvmx_result execute(struct nested_domain_dispatch *s,struct nvmx m[2],unsigned cpu,enum nvmx_op op,uint64_t operand)
{
    struct nvmx_result out;
    assert(nested_domain_dispatch_execute(s,cpu,s->tokens[cpu],&m[cpu],req(op,operand),&out));return out;
}
#define REJECT(call,cpu) do {struct nested_domain old=s.domain;struct nvmx before=m[cpu];struct nvmx_result was=out;assert(!(call));assert(!memcmp(&old,&s.domain,sizeof old));assert(!memcmp(&before,&m[cpu],sizeof before));assert(!memcmp(&was,&out,sizeof out));assert(!s.lock);}while(0)
#define RECLAIM_REJECT(call) do {struct nested_domain before=s.domain; \
    assert(!(call));assert(!memcmp(&before,&s.domain,sizeof before));assert(!s.lock);}while(0)
static void internal_pause_test(void)
{
    struct nested_domain_dispatch s;struct nvmx m[2];struct nvmx_result out={0};
    setup(&s,m);
    for(unsigned cpu=0;cpu<2;cpu++){
        assert(execute(&s,m,cpu,NVMX_ON,m[cpu].on_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_CLEAR,m[cpu].vmcs_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_PTRLD,m[cpu].vmcs_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_LAUNCH,0).status==NVMX_ENTRY_REQUEST);
        assert(nested_domain_dispatch_physical_enter(&s,cpu,s.tokens[cpu],&m[cpu],true));
        assert(nested_domain_dispatch_physical_pause(&s,cpu,s.tokens[cpu],&m[cpu],0,1));
        assert(m[cpu].running&&m[cpu].launched&&!m[cpu].prepared&&s.internal_pending[cpu]);
        assert(!s.domain.cpus[cpu].guest_running&&!s.domain.objects[s.tokens[cpu].slot].running);
        RECLAIM_REJECT(nested_domain_dispatch_physical_pause(&s,cpu,s.tokens[cpu],&m[cpu],0,1));
        REJECT(nested_domain_dispatch_execute(&s,cpu,s.tokens[cpu],&m[cpu],req(NVMX_CLEAR,m[cpu].vmcs_region),&out),cpu);
        RECLAIM_REJECT(nested_domain_dispatch_physical_enter(&s,cpu,s.tokens[cpu],&m[cpu],true));
    }
    assert(!nested_domain_dispatch_final(&s));
    struct nested_domain_reclaim_ticket ticket;
    assert(nested_domain_dispatch_reclaim_begin(&s,&ticket)&&s.domain.targets==3);
    for(unsigned cpu=0;cpu<2;cpu++)
        RECLAIM_REJECT(nested_domain_dispatch_physical_resume(&s,cpu,s.tokens[cpu],&m[cpu]));
    assert(nested_domain_dispatch_reclaim_publish(&s,ticket));
    assert(nested_domain_dispatch_reclaim_ack(&s,0,ticket));
    assert(nested_domain_dispatch_reclaim_ack(&s,1,ticket));
    assert(nested_domain_dispatch_reclaim_complete(&s,ticket));
    for(unsigned cpu=0;cpu<2;cpu++){
        assert(nested_domain_dispatch_physical_resume(&s,cpu,s.tokens[cpu],&m[cpu]));
        RECLAIM_REJECT(nested_domain_dispatch_physical_resume(&s,cpu,s.tokens[cpu],&m[cpu]));
        assert(nested_domain_dispatch_physical_leave(&s,cpu,s.tokens[cpu],&m[cpu],true,0,18));
        assert(!s.internal_pending[cpu]&&!s.internal_resumed[cpu]&&s.internal_used[cpu]);
        assert(m[cpu].running&&m[cpu].launched);
        assert(nvmx_return_from_l2(&m[cpu]));
        assert(execute(&s,m,cpu,NVMX_RESUME,0).status==NVMX_ENTRY_REQUEST);
        assert(nested_domain_dispatch_physical_enter(&s,cpu,s.tokens[cpu],&m[cpu],true));
        assert(nested_domain_dispatch_physical_leave(&s,cpu,s.tokens[cpu],&m[cpu],true,0,18));
        assert(nvmx_return_from_l2(&m[cpu]));
        assert(execute(&s,m,cpu,NVMX_CLEAR,m[cpu].vmcs_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_OFF,0).status==NVMX_SUCCESS);
    }
    assert(nested_domain_dispatch_final(&s));
    /* Physical failure after internal resume must retain the suspended state. */
    for(unsigned failure=0;failure<3;failure++){
        setup(&s,m);
        assert(execute(&s,m,0,NVMX_ON,m[0].on_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,0,NVMX_CLEAR,m[0].vmcs_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,0,NVMX_PTRLD,m[0].vmcs_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,0,NVMX_LAUNCH,0).status==NVMX_ENTRY_REQUEST);
        assert(nested_domain_dispatch_physical_enter(&s,0,s.tokens[0],&m[0],true));
        assert(nested_domain_dispatch_physical_pause(&s,0,s.tokens[0],&m[0],0,1));
        assert(nested_domain_dispatch_physical_resume(&s,0,s.tokens[0],&m[0]));
        assert(!nested_domain_dispatch_physical_leave(&s,0,s.tokens[0],&m[0],true,
            failure==0?1:0,failure==1?(1ull<<31)|33:10));
        assert(s.fault_latch&&s.internal_pending[0]&&s.internal_resumed[0]&&m[0].running);
        assert(s.domain.gate==NESTED_DOMAIN_POISON);
    }
}

static void start_ept(struct nested_domain_dispatch *s,struct nvmx m[2]) {
    setup(s,m);
    assert(execute(s,m,0,NVMX_ON,m[0].on_region).status==NVMX_SUCCESS);
    assert(execute(s,m,0,NVMX_CLEAR,m[0].vmcs_region).status==NVMX_SUCCESS);
    assert(execute(s,m,0,NVMX_PTRLD,m[0].vmcs_region).status==NVMX_SUCCESS);
    assert(execute(s,m,0,NVMX_LAUNCH,0).status==NVMX_ENTRY_REQUEST);
    assert(nested_domain_dispatch_physical_enter(s,0,s->tokens[0],&m[0],true));
}
static void internal_ept_test(void) {
    struct nested_domain_dispatch s;struct nvmx m[2];
    start_ept(&s,m);
    for(unsigned round=0;round<3;round++) {
        assert(nested_domain_dispatch_physical_suspend_ept(&s,0,s.tokens[0],&m[0],0,48));
        assert(s.ept_suspends[0]==round+1 && s.internal_origin[0]==NESTED_INTERNAL_EPT);
        assert(m[0].running && m[0].launched && !m[0].prepared);
        assert(!s.domain.cpus[0].guest_running);
        struct nested_domain_dispatch saved=s;struct nvmx before=m[0];
        assert(!nested_domain_dispatch_physical_suspend_ept(&s,0,s.tokens[0],&m[0],0,48));
        assert(!nested_domain_dispatch_physical_pause(&s,0,s.tokens[0],&m[0],0,1));
        assert(!memcmp(&saved,&s,sizeof s)&&!memcmp(&before,&m[0],sizeof before));
        struct nested_domain_reclaim_ticket ticket;
        assert(nested_domain_dispatch_reclaim_begin(&s,&ticket));
        saved=s;
        assert(!nested_domain_dispatch_physical_resume(&s,0,s.tokens[0],&m[0]));
        assert(!memcmp(&saved,&s,sizeof s));
        assert(nested_domain_dispatch_reclaim_publish(&s,ticket));
        assert(nested_domain_dispatch_reclaim_ack(&s,0,ticket));
        assert(nested_domain_dispatch_reclaim_complete(&s,ticket));
        assert(nested_domain_dispatch_physical_resume(&s,0,s.tokens[0],&m[0]));
    }
    assert(nested_domain_dispatch_physical_leave(&s,0,s.tokens[0],&m[0],true,0,18));
    assert(!s.internal_pending[0]&&!s.internal_resumed[0]&&s.internal_origin[0]==NESTED_INTERNAL_NONE);
    assert(m[0].running&&nvmx_return_from_l2(&m[0]));
    assert(execute(&s,m,0,NVMX_CLEAR,m[0].vmcs_region).status==NVMX_SUCCESS);
    assert(execute(&s,m,0,NVMX_OFF,0).status==NVMX_SUCCESS);
    for(unsigned phase=0;phase<2;phase++) {
        start_ept(&s,m);
        if(phase) {
            assert(nested_domain_dispatch_physical_suspend_ept(&s,0,s.tokens[0],&m[0],0,48));
            assert(nested_domain_dispatch_physical_resume(&s,0,s.tokens[0],&m[0]));
        }
        s.ept_suspends[0]=UINT32_MAX;
        struct nested_domain_dispatch saved=s;struct nvmx before=m[0];
        assert(!nested_domain_dispatch_physical_suspend_ept(&s,0,s.tokens[0],&m[0],0,48));
        assert(!memcmp(&saved,&s,sizeof s)&&!memcmp(&before,&m[0],sizeof before));
    }
    start_ept(&s,m);
    assert(nested_domain_dispatch_physical_pause(&s,0,s.tokens[0],&m[0],0,1));
    assert(nested_domain_dispatch_physical_resume(&s,0,s.tokens[0],&m[0]));
    struct nested_domain_dispatch saved=s;struct nvmx before=m[0];
    assert(!nested_domain_dispatch_physical_suspend_ept(&s,0,s.tokens[0],&m[0],0,48));
    assert(!memcmp(&saved,&s,sizeof s)&&!memcmp(&before,&m[0],sizeof before));
    for(unsigned fail=0;fail<3;fail++) {
        start_ept(&s,m);
        assert(nested_domain_dispatch_physical_suspend_ept(&s,0,s.tokens[0],&m[0],0,48));
        assert(nested_domain_dispatch_physical_resume(&s,0,s.tokens[0],&m[0]));
        assert(!nested_domain_dispatch_physical_suspend_ept(&s,0,s.tokens[0],&m[0],
            fail==0?1:0,fail==1?(1ull<<31)|48:fail==2?49:48));
        assert(s.fault_latch&&s.domain.gate==NESTED_DOMAIN_POISON&&m[0].running);
    }
}
static void local_invept_test(void)
{
    struct nested_domain_dispatch s;struct nvmx m[2];setup(&s,m);
    for(unsigned cpu=0;cpu<2;cpu++){
        assert(execute(&s,m,cpu,NVMX_ON,m[cpu].on_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_CLEAR,m[cpu].vmcs_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_PTRLD,m[cpu].vmcs_region).status==NVMX_SUCCESS);
    }
    struct nested_domain_invept_ticket t={0},sentinel={.sequence=99};
    uint64_t old1=s.domain.cpus[1].cache_generation;
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_begin(&s,0,s.tokens[1],&m[0],1,0x1000,&sentinel));
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_begin(&s,0,s.tokens[0],&m[0],3,0x1000,&sentinel));
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_begin(&s,0,s.tokens[0],&m[0],1,0x1001,&sentinel));
    assert(sentinel.sequence==99);
    assert(nested_domain_dispatch_local_invept_begin(&s,0,s.tokens[0],&m[0],1,0x1000,&t));
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_begin(&s,0,s.tokens[0],&m[0],1,0x2000,&sentinel));
    RECLAIM_REJECT(nested_domain_dispatch_physical_enter(&s,0,s.tokens[0],&m[0],false));
    for(unsigned field=0;field<6;field++){
        struct nested_domain_invept_ticket bad=t;
        switch(field){case 0:bad.domain_id++;break;case 1:bad.incarnation++;break;
        case 2:bad.cpu=1;break;case 3:bad.type=2;break;case 4:bad.root+=4096;break;case 5:bad.sequence--;break;}
        RECLAIM_REJECT(nested_domain_dispatch_local_invept_complete(&s,0,s.tokens[0],&m[0],bad));
    }
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_complete(&s,1,s.tokens[1],&m[1],t));
    assert(nested_domain_dispatch_local_invept_complete(&s,0,s.tokens[0],&m[0],t));
    assert(s.domain.cpus[0].cache_generation==t.sequence&&s.domain.cpus[1].cache_generation==old1);
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_complete(&s,0,s.tokens[0],&m[0],t));
    /* A valid EPTP 0x1e has root zero, even with no cached context. */
    uint64_t old0=s.domain.cpus[0].cache_generation;
    assert(nested_domain_dispatch_local_invept_begin(&s,0,s.tokens[0],&m[0],1,0,&t));
    assert(t.root==0&&t.type==1&&t.sequence==old0+1);
    assert(nested_domain_dispatch_local_invept_complete(&s,0,s.tokens[0],&m[0],t));
    assert(s.domain.cpus[0].cache_generation==old0+1&&s.domain.cpus[1].cache_generation==old1);
    assert(nested_domain_dispatch_physical_enter(&s,0,s.tokens[0],&m[0],false));
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_begin(&s,0,s.tokens[0],&m[0],2,~0ull,&sentinel));
    assert(nested_domain_dispatch_physical_leave(&s,0,s.tokens[0],&m[0],false,0,27));
    assert(execute(&s,m,0,NVMX_LAUNCH,0).status==NVMX_ENTRY_REQUEST);
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_begin(&s,0,s.tokens[0],&m[0],2,0,&sentinel));
    assert(nested_domain_dispatch_physical_enter(&s,0,s.tokens[0],&m[0],true));
    assert(nested_domain_dispatch_physical_pause(&s,0,s.tokens[0],&m[0],0,1));
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_begin(&s,0,s.tokens[0],&m[0],2,0,&sentinel));
    assert(nested_domain_dispatch_local_invept_begin(&s,1,s.tokens[1],&m[1],2,~0ull,&t));
    assert(t.root==0); /* type2 ignores descriptor root */
    uint64_t before=s.domain.cpus[1].cache_generation;
    nested_domain_dispatch_markfail(&s); /* actual flush failure: no completion */
    RECLAIM_REJECT(nested_domain_dispatch_local_invept_complete(&s,1,s.tokens[1],&m[1],t));
    assert(s.domain.cpus[1].cache_generation==before&&s.domain.cpus[1].pending_ticket==t.sequence);
}
int main(void)
{
    struct nested_domain_dispatch s;struct nvmx m[2];struct nvmx_result out={.value=123};
    setup(&s,m);assert(!nested_domain_dispatch_final(&s));
    for(unsigned cpu=0;cpu<2;cpu++){
        assert(nested_domain_dispatch_physical_enter(&s,cpu,s.tokens[cpu],&m[cpu],false));
        assert(nested_domain_dispatch_physical_leave(&s,cpu,s.tokens[cpu],&m[cpu],false,0,27));
        assert(execute(&s,m,cpu,NVMX_ON,m[cpu].on_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_CLEAR,m[cpu].vmcs_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_CLEAR,m[cpu].vmcs_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_PTRLD,m[cpu].vmcs_region).status==NVMX_SUCCESS);
    }
    REJECT(nested_domain_dispatch_execute(&s,0,s.tokens[1],&m[0],req(NVMX_PTRLD,0x2e000),&out),0);
    REJECT(nested_domain_dispatch_execute(&s,0,s.tokens[0],&m[0],req(NVMX_PTRLD,0x2e000),&out),0);
    REJECT(nested_domain_dispatch_execute(&s,0,s.tokens[0],&m[0],req(NVMX_OFF,0),&out),0); /* un-cleared active */
    struct nested_domain_token stale=s.tokens[0];stale.incarnation++;
    REJECT(nested_domain_dispatch_execute(&s,0,stale,&m[0],req(NVMX_PTRST,0),&out),0);
    assert(execute(&s,m,0,NVMX_RESUME,0).status==NVMX_FAIL_VALID);
    for(unsigned round=0;round<2;round++){
        for(unsigned cpu=0;cpu<2;cpu++){
            assert(execute(&s,m,cpu,round?NVMX_RESUME:NVMX_LAUNCH,0).status==NVMX_ENTRY_REQUEST);
            assert(nested_domain_dispatch_physical_enter(&s,cpu,s.tokens[cpu],&m[cpu],true));
        }
        struct nested_domain_reclaim_ticket ticket={99,98,97}, untouched=ticket;
        RECLAIM_REJECT(nested_domain_dispatch_reclaim_begin(&s,&ticket));
        assert(!memcmp(&ticket,&untouched,sizeof ticket));
        assert(s.domain.cpus[0].guest_running&&s.domain.cpus[1].guest_running&&s.domain.residency==3);
        REJECT(nested_domain_dispatch_execute(&s,0,s.tokens[0],&m[0],req(NVMX_PTRST,0),&out),0);
        for(unsigned cpu=0;cpu<2;cpu++){
            assert(nested_domain_dispatch_physical_leave(&s,cpu,s.tokens[cpu],&m[cpu],true,0,18));
            assert(m[cpu].launched&&m[cpu].running&&!m[cpu].prepared);
            assert(nvmx_return_from_l2(&m[cpu])); /* adapter reflection complete */
        }
        if(round==0){
            assert(nested_domain_dispatch_reclaim_begin(&s,&ticket));
            assert(!s.lock&&s.domain.targets==3&&s.domain.gate==NESTED_DOMAIN_REVOKING);
            for(unsigned cpu=0;cpu<2;cpu++){
                assert(m[cpu].root&&m[cpu].current==m[cpu].vmcs_region&&m[cpu].launched);
                RECLAIM_REJECT(nested_domain_dispatch_physical_enter(&s,cpu,s.tokens[cpu],&m[cpu],false));
                REJECT(nested_domain_dispatch_execute(&s,cpu,s.tokens[cpu],&m[cpu],req(NVMX_RESUME,0),&out),cpu);
            }
            RECLAIM_REJECT(nested_domain_dispatch_reclaim_begin(&s,&untouched));
            RECLAIM_REJECT(nested_domain_dispatch_reclaim_ack(&s,0,ticket));
            RECLAIM_REJECT(nested_domain_dispatch_reclaim_complete(&s,ticket));
            for(unsigned field=0;field<3;field++){
                struct nested_domain_reclaim_ticket bad=ticket;
                if(field==0)bad.domain_id++;else if(field==1)bad.incarnation++;else bad.epoch--;
                RECLAIM_REJECT(nested_domain_dispatch_reclaim_publish(&s,bad));
                RECLAIM_REJECT(nested_domain_dispatch_reclaim_ack(&s,0,bad));
                RECLAIM_REJECT(nested_domain_dispatch_reclaim_complete(&s,bad));
            }
            assert(nested_domain_dispatch_reclaim_publish(&s,ticket));
            RECLAIM_REJECT(nested_domain_dispatch_reclaim_publish(&s,ticket));
            RECLAIM_REJECT(nested_domain_dispatch_reclaim_ack(&s,2,ticket));
            assert(nested_domain_dispatch_reclaim_ack(&s,0,ticket));
            RECLAIM_REJECT(nested_domain_dispatch_reclaim_ack(&s,0,ticket));
            RECLAIM_REJECT(nested_domain_dispatch_reclaim_complete(&s,ticket));
            assert(nested_domain_dispatch_reclaim_ack(&s,1,ticket));
            assert(s.domain.gate==NESTED_DOMAIN_REVOKING); /* no automatic reopen */
            assert(nested_domain_dispatch_reclaim_complete(&s,ticket));
            assert(s.domain.gate==NESTED_DOMAIN_OPEN&&s.domain.residency==3&&!s.lock);
            RECLAIM_REJECT(nested_domain_dispatch_reclaim_ack(&s,1,ticket));
        } /* next round performs actual model RESUME/entry/exit for both CPUs */
    }
    assert(!nested_domain_dispatch_final(&s));
    for(unsigned cpu=0;cpu<2;cpu++){
        assert(execute(&s,m,cpu,NVMX_CLEAR,m[cpu].vmcs_region).status==NVMX_SUCCESS);
        assert(execute(&s,m,cpu,NVMX_OFF,0).status==NVMX_SUCCESS);
    }
    assert(nested_domain_dispatch_final(&s)&&s.domain.residency==3);
    /* No-current architecture failure still passes through actual nvmx result. */
    setup(&s,m);assert(execute(&s,m,0,NVMX_READ,0).status==NVMX_FAULT_UD);
    assert(execute(&s,m,0,NVMX_ON,0xd000).status==NVMX_SUCCESS);
    assert(execute(&s,m,0,NVMX_READ,0).status==NVMX_FAIL_INVALID);
    assert(execute(&s,m,0,NVMX_PTRST,0).value==NVMX_NONE);
    /* Physical failure cannot be converted into launch success. */
    for(unsigned kind=0;kind<3;kind++){
        setup(&s,m);assert(execute(&s,m,0,NVMX_ON,0xd000).status==NVMX_SUCCESS);
        assert(execute(&s,m,0,NVMX_CLEAR,0xe000).status==NVMX_SUCCESS);
        assert(execute(&s,m,0,NVMX_PTRLD,0xe000).status==NVMX_SUCCESS);
        assert(execute(&s,m,0,NVMX_LAUNCH,0).status==NVMX_ENTRY_REQUEST);
        assert(nested_domain_dispatch_physical_enter(&s,0,s.tokens[0],&m[0],true));
        assert(!nested_domain_dispatch_physical_leave(&s,0,s.tokens[0],&m[0],true,
            kind==0?1:0,kind==1?(1ull<<31)|33:10));
        assert(s.fault_latch&&s.domain.gate==NESTED_DOMAIN_POISON&&!m[0].launched);
        assert(!s.lock&&!nested_domain_dispatch_final(&s));
    }
    setup(&s,m);s.lock=1;
    assert(!nested_domain_dispatch_execute(&s,0,s.tokens[0],&m[0],req(NVMX_ON,0xd000),&out));
    assert(s.fault_latch&&s.lock==1&&!m[0].root); /* timeout never steals lock */
    s.lock=0;nested_domain_dispatch_markfail(&s);assert(s.domain.gate==NESTED_DOMAIN_POISON&&!s.lock);
    assert(!nested_domain_dispatch_physical_enter(&s,0,s.tokens[0],&m[0],false));
    struct nested_domain_reclaim_ticket denied={10,20,2}, saved=denied;
    RECLAIM_REJECT(nested_domain_dispatch_reclaim_begin(&s,&denied));
    assert(!memcmp(&saved,&denied,sizeof saved));
    RECLAIM_REJECT(nested_domain_dispatch_reclaim_publish(&s,denied));
    RECLAIM_REJECT(nested_domain_dispatch_reclaim_ack(&s,0,denied));
    RECLAIM_REJECT(nested_domain_dispatch_reclaim_complete(&s,denied));
    /* Reclaim lock timeout also closes the fatal gate without stealing lock. */
    setup(&s,m);s.lock=1;
    assert(!nested_domain_dispatch_reclaim_begin(&s,&denied));
    assert(s.lock==1&&s.fault_latch&&!memcmp(&saved,&denied,sizeof saved));
    s.lock=0;
    internal_pause_test();
    internal_ept_test();
    local_invept_test();
    puts("nested_domain_dispatch: PASS");
}
