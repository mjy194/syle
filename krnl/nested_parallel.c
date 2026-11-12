#include "nested_parallel.h"
bool nested_parallel_init(struct nested_parallel_runtime *r,unsigned cpu)
{
    if(!r || cpu>=64)return false;
    volatile unsigned char *bytes=(void *)r;
    for(size_t i=0;i<sizeof *r;i++)bytes[i]=0;
    r->cpu_index=cpu;r->retain=true;return true;
}
static bool writable(const struct nested_parallel_runtime *r)
{
    return r && !__atomic_load_n(&r->done,__ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&r->faulted,__ATOMIC_ACQUIRE) && r->trace_used<NESTED_PARALLEL_TRACE_CAPACITY;
}
bool nested_parallel_trace_text(struct nested_parallel_runtime *r,const char *text)
{
    if(!writable(r)||!text)return false;
    while(*text){
        if(r->trace_used==NESTED_PARALLEL_TRACE_CAPACITY-1){
            r->trace[r->trace_used]=0;r->trace_overflow=true;return false;
        }
        r->trace[r->trace_used++]=*text++;
    }
    r->trace[r->trace_used]=0;return true;
}
bool nested_parallel_trace_hex(struct nested_parallel_runtime *r,uint64_t value)
{
    static const char digits[]="0123456789abcdef";
    char text[19];text[0]='0';text[1]='x';
    for(unsigned i=0;i<16;i++)text[2+i]=digits[(value>>(60-4*i))&15];
    text[18]=0;return nested_parallel_trace_text(r,text);
}
void nested_parallel_mark_fault(struct nested_parallel_runtime *r,unsigned code)
{
    if(!r || __atomic_load_n(&r->done,__ATOMIC_ACQUIRE) ||
       __atomic_load_n(&r->faulted,__ATOMIC_ACQUIRE))return;
#ifdef VMX_NESTED_ATOMIC_RETRACK_TEST
    /* Publish the shared stop condition before a local nonreturning sink.
     * This covers every guest/runtime panic path, including pre-round faults. */
    if(r->retrack_fault)r->retrack_fault(r);
#endif
    r->fault_code=code;r->retain=true;
    __atomic_store_n(&r->faulted,1,__ATOMIC_RELEASE);
}
bool nested_parallel_faulted(const struct nested_parallel_runtime *r)
{return r && __atomic_load_n(&r->faulted,__ATOMIC_ACQUIRE)!=0;}
bool nested_parallel_finish(struct nested_parallel_runtime *r,unsigned result,bool safe)
{
    if(!r || __atomic_load_n(&r->done,__ATOMIC_ACQUIRE))return false;
    r->result=result;r->retain=!safe || nested_parallel_faulted(r);
    __atomic_store_n(&r->done,1,__ATOMIC_RELEASE);return true;
}
bool nested_parallel_done(const struct nested_parallel_runtime *r)
{return r && __atomic_load_n(&r->done,__ATOMIC_ACQUIRE)!=0;}
#ifndef NESTED_PARALLEL_POLICY_ONLY
_Noreturn void nested_parallel_fault_stop(struct nested_parallel_runtime *r,unsigned code,const char *reason)
{
    __asm__ volatile("cli":::"memory");
    if(reason){(void)nested_parallel_trace_text(r,"FAULT: ");
        (void)nested_parallel_trace_text(r,reason);(void)nested_parallel_trace_text(r,"\n");}
    nested_parallel_mark_fault(r,code);
    for(;;)__asm__ volatile("hlt":::"memory");
}
#endif
