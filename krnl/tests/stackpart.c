#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../ept.h"
#include "../memory.h"
#include "../vmx_memory.h"
static _Alignas(4096) unsigned char memory[32][4096];
static unsigned count,live,flushes,fail_after=32;
static bool used[32];
static bool wb=true, map_ok=true, invalidate_ok=true;
struct pmm_stats pmm_get_stats(void) { return (struct pmm_stats){.allocated_pages=live}; }
uint64_t pmm_alloc_page(void) {
    if(count==fail_after) return 0;
    used[count]=true;live++;
    return 0x100000+(count++)*4096;
}
int pmm_free_page(uint64_t p) {
    unsigned i=(p-0x100000)/4096;
    assert(i<32&&used[i]);used[i]=false;live--;return 1;
}
void *paging_phys_to_virt(uint64_t p) {
    assert(p>=0x100000&&p<0x120000);
    if(!map_ok) return NULL;
    return memory[(p-0x100000)/4096];
}
int vmx_memory_region_wb(uint64_t p) { return wb&&p>=0x100000&&p<0x120000; }
bool ept_test_invalidate(uint64_t p) { assert((p&0xfff)==30);flushes++;return invalidate_ok; }
_Noreturn void panic(const char *s) { (void)s;abort(); }
static uint64_t *ptr(uint64_t p) { return paging_phys_to_virt(p); }
int main(void) {
    struct ept_fixture f;
    assert(ept_fixture_init(&f));assert(live==17);
    assert(ept_stackpart_observe(&f,0x46)==0);
    assert(ept_stackpart_protect(&f,true));
    assert(!ept_stackpart_protect(&f,true));
    uint64_t *upper=ptr(f.pages[12]),*lower=ptr(f.pages[11]);
    uint64_t old=upper[0];
    upper[0]=8;assert(ept_stackpart_observe(&f,0x46)==1);
    upper[1]=0x10046;upper[2]=0xd020;upper[3]=16;
    assert(ept_stackpart_observe(&f,0x46)==4);
    upper[4]=0;assert(ept_stackpart_observe(&f,0x46)==-1);
    upper[4]=UINT64_C(0x5a00000000d00004);
    uint64_t saved=lower[511];lower[511]=1;
    assert(ept_stackpart_observe(&f,0x46)==-1);lower[511]=saved;
    upper[1]=0x46;assert(ept_stackpart_observe(&f,0x46)==-1);
    upper[1]=0x10046;
    assert(ept_stackpart_protect(&f,false));
    assert(!ept_stackpart_protect(&f,false));
    lower[510]=0x28;lower[511]=0x6123;
    assert(ept_stackpart_complete(&f,0x46,0x6123));
    assert(!ept_stackpart_complete(&f,0x46,0x6124));
    upper[0]=old;assert(!ept_stackpart_complete(&f,0x46,0x6123));
    invalidate_ok=false;assert(!ept_stackpart_protect(&f,true));assert(live==17);
    invalidate_ok=true;assert(ept_stackpart_protect(&f,false));
    ept_fixture_destroy(&f);assert(live==0);
}
