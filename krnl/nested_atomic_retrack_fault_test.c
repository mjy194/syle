#include "nested_parallel.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static unsigned called;
static uint32_t shared_failure;
static void shared_stop(struct nested_parallel_runtime *r){
 assert(!nested_parallel_faulted(r));
 __atomic_store_n(&shared_failure,1,__ATOMIC_RELEASE);called++;
}
int main(void){
 struct nested_parallel_runtime *r=calloc(1,sizeof *r);assert(r);
 assert(nested_parallel_init(r,0));r->retrack_fault=shared_stop;
 nested_parallel_mark_fault(r,42);
 assert(called==1&&__atomic_load_n(&shared_failure,__ATOMIC_ACQUIRE)&&nested_parallel_faulted(r)&&r->retain&&r->fault_code==42);
 nested_parallel_mark_fault(r,43);assert(called==1&&r->fault_code==42);
 free(r);puts("PASS N42B local fault publication invokes shared stop before local retained sink");return 0;
}
