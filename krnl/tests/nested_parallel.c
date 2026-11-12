#include "../nested_parallel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct guarded {uint64_t before;struct nested_parallel_runtime runtime;uint64_t after;};
int main(void)
{
    struct guarded g={.before=0x11223344,.after=0xaabbccdd};
    struct nested_parallel_runtime *r=&g.runtime;
    assert(nested_parallel_init(r,1)&&r->retain&&r->cpu_index==1);
    assert(!nested_parallel_done(r)&&!nested_parallel_faulted(r)&&r->trace_used==0);
    assert(nested_parallel_trace_text(r,"hello "));
    assert(nested_parallel_trace_hex(r,UINT64_C(0x1234abcdef567890)));
    assert(!strcmp(r->trace,"hello 0x1234abcdef567890"));
    assert(nested_parallel_trace_text(r,"\n"));
    assert(nested_parallel_finish(r,7,true));assert(nested_parallel_done(r)&&r->result==7&&!r->retain);
    size_t old=r->trace_used;
    assert(!nested_parallel_trace_text(r,"late")&&r->trace_used==old);
    assert(!nested_parallel_finish(r,8,false)&&r->result==7&&!r->retain);
    nested_parallel_mark_fault(r,99);assert(!nested_parallel_faulted(r));
    assert(nested_parallel_init(r,63));
    for(unsigned i=0;i<NESTED_PARALLEL_TRACE_CAPACITY-1;i++)assert(nested_parallel_trace_text(r,"A"));
    assert(r->trace_used==NESTED_PARALLEL_TRACE_CAPACITY-1&&!r->trace_overflow);
    assert(nested_parallel_trace_text(r,""));
    assert(!nested_parallel_trace_text(r,"B")&&r->trace_overflow);
    assert(r->trace[NESTED_PARALLEL_TRACE_CAPACITY-1]==0);
    assert(!nested_parallel_trace_hex(r,0));
    assert(g.before==0x11223344&&g.after==0xaabbccdd);
    assert(nested_parallel_init(r,0));
    for(unsigned i=0;i<NESTED_PARALLEL_TRACE_CAPACITY-4;i++)assert(nested_parallel_trace_text(r,"a"));
    assert(!nested_parallel_trace_text(r,"123456"));
    assert(r->trace_used==NESTED_PARALLEL_TRACE_CAPACITY-1&&r->trace_overflow);
    assert(!strcmp(r->trace+NESTED_PARALLEL_TRACE_CAPACITY-4,"123"));
    assert(g.before==0x11223344&&g.after==0xaabbccdd);
    assert(nested_parallel_init(r,0));assert(nested_parallel_trace_text(r,"fault"));
    nested_parallel_mark_fault(r,42);
    assert(nested_parallel_faulted(r)&&r->retain&&r->fault_code==42&&!nested_parallel_done(r));
    nested_parallel_mark_fault(r,43);assert(r->fault_code==42);
    assert(!nested_parallel_trace_text(r,"late")&&!strcmp(r->trace,"fault"));
    assert(nested_parallel_finish(r,9,true)&&nested_parallel_done(r)&&r->retain);
    assert(nested_parallel_init(r,0));assert(nested_parallel_finish(r,0,false)&&r->retain);
    assert(!nested_parallel_init(r,64));
    assert(!nested_parallel_init(0,0));assert(!nested_parallel_trace_text(0,"x"));
    assert(!nested_parallel_trace_hex(0,0));assert(!nested_parallel_finish(0,0,true));
    assert(!nested_parallel_done(0)&&!nested_parallel_faulted(0));nested_parallel_mark_fault(0,1);
    puts("nested_parallel trace/state: PASS");
}
