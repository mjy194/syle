/* Executes the production smp_nested.inc completion slice, not copied predicates.
 * The runner extracts it unchanged and uses each target's actual Makefile flags.
 * Paging, console and PMM are host mocks; pool detachment is the real policy. */
#include "../nested_parallel.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#if !defined(VMX_NESTED_OWNER_EVICT_TEST) || !defined(VMX_NESTED_OWNER_SPARSE_TEST)
#error This regression requires an actual owner eviction profile
#endif
static struct nested_parallel_runtime nested_runs[2];
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
#include "typed_root_overlap_fixture.h"
static struct {struct tr_overlap overlap[2];} typed_runtime;
#endif
static uint64_t shared_pages[16],nested_shared;
static _Alignas(4096) unsigned char pool_pages[2][NESTED_OWNER_POOL_PAGES][4096];
static unsigned freed;
static const char *panic_reason;
static jmp_buf panic_target;
static void *paging_phys_to_virt(uint64_t hpa){return (void *)(uintptr_t)hpa;}
static void log_text(const char *text){(void)text;}
static void log_hex(uint64_t value){(void)value;}
static bool pmm_free_page(uint64_t hpa){assert(hpa);freed++;return true;}
static _Noreturn void panic(const char *reason){panic_reason=reason;longjmp(panic_target,1);}
static void complete(void){
#include "nested-owner-completion.inc"
}
static void configure(void){
    memset(nested_runs,0,sizeof nested_runs);
    memset(shared_pages,0,sizeof shared_pages);
    nested_shared=(uint64_t)(uintptr_t)shared_pages;
    freed=0;panic_reason=NULL;
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    for(unsigned phase=0;phase<2;phase++)typed_runtime.overlap[phase]=overlap_fixture(phase);
#endif
    for(unsigned i=0;i<2;i++){
        struct nested_owner_sparse *s=&nested_runs[i].sparse;
        for(unsigned n=0;n<NESTED_OWNER_POOL_PAGES;n++)
            s->pages[n]=(struct nested_sparse_page){
                0x100000+(i*NESTED_OWNER_POOL_PAGES+n)*4096,pool_pages[i][n]};
        assert(nested_owner_pool_init(&s->pool,i,1,s->pages,s->slots,NESTED_OWNER_POOL_PAGES,48));
        s->destroyed=true;s->misses=3;s->peak_live=16;
        shared_pages[i*8]=shared_pages[i*8+2]=0x3200+i;
        shared_pages[i*8+3]=UINT64_C(0x3344334433443344);
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
        s->creations=6;s->evictions=4;
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
        shared_pages[i*8+1]=0x39b20000+i;
#else
        shared_pages[i*8+1]=0x38cd0000+i;
#endif
        for(unsigned c=0;c<3;c++)shared_pages[i*8+4+c]=UINT64_C(0x38cd38cd38cd3800)+i*3+c;
        shared_pages[i*8+7]=UINT64_C(0x38cd38cd38cd3800)+i*3;
#else
        s->creations=4;s->evictions=2;
        shared_pages[i*8+1]=0x3200+(i^1);
#endif
    }
}
static void expect(bool accepted){
    volatile bool succeeded=false;
    if(!setjmp(panic_target)){complete();succeeded=true;}
    if(succeeded!=accepted){
        fprintf(stderr,"completion expected %s, got %s (%s)\n",accepted?"accept":"reject",
                succeeded?"accept":"reject",panic_reason?panic_reason:"no panic");
        assert(succeeded==accepted);
    }
    if(accepted){
        assert(freed==2*NESTED_OWNER_POOL_PAGES);
        assert(!nested_runs[0].sparse.pool.active&&!nested_runs[1].sparse.pool.active);
    }else assert(freed<2*NESTED_OWNER_POOL_PAGES);
}
int main(void){
    configure();expect(true);
    for(unsigned owner=0;owner<2;owner++){
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
        const uint64_t bad[][2]={{5,4},{6,3},{4,2}};
        configure();nested_runs[owner].sparse.creations=7;nested_runs[owner].sparse.evictions=5;expect(true);
#else
        const uint64_t bad[][2]={{3,2},{5,2},{4,1},{4,3},{6,4}};
#endif
        for(unsigned n=0;n<sizeof bad/sizeof bad[0];n++){
            configure();nested_runs[owner].sparse.creations=bad[n][0];
            nested_runs[owner].sparse.evictions=bad[n][1];expect(false);
        }
        configure();nested_runs[owner].sparse.peak_live=NESTED_OWNER_POOL_PAGES;expect(true);
        configure();nested_runs[owner].sparse.peak_live=NESTED_OWNER_POOL_PAGES+1;expect(false);
        configure();nested_runs[owner].sparse.destroyed=false;expect(false);
        configure();nested_runs[owner].sparse.pool.fault=true;expect(false);
        configure();nested_runs[owner].trace_overflow=true;expect(false);
        configure();shared_pages[owner*8+2]=0;expect(false);
        configure();shared_pages[owner*8+1]=0;expect(false);
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
        for(unsigned c=0;c<4;c++){configure();shared_pages[owner*8+4+c]=0;expect(false);}
#endif
    }
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    for(unsigned phase=0;phase<2;phase++){
        configure();typed_runtime.overlap[phase].finished=0;expect(false);
        configure();typed_runtime.overlap[phase].peer.after.evictions=typed_runtime.overlap[phase].peer.before.evictions;expect(false);
        configure();typed_runtime.overlap[phase].final.receipt.commit_version=typed_runtime.overlap[phase].peer.retired.receipt.commit_version;expect(false);
    }
    puts("PASS N39B2 actual completion: both causal overlap witnesses required; >=6/4 owner guards preserved");
#elif defined(VMX_NESTED_OWNER_ROOT_SCOPED_TEST)
    puts("PASS N39B1 actual completion: >=6/4 accepted, short sequence/over-budget rejected, both owners");
#elif defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
    puts("PASS N38C-B actual completion: >=6/4 accepted, short sequence/over-budget rejected, both owners");
#else
    puts("PASS N34C2 actual completion: exact 4/2 retained, other counts/over-budget rejected, both owners");
#endif
    return 0;
}
