#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../ept.h"
#include "../memory.h"
#include "../vmx_memory.h"
static _Alignas(4096) unsigned char memory[64][4096];
static unsigned count,live,flushes,fail_after=16;
static bool used[64];
static bool wb=true, map_ok=true, invalidate_ok=true;
struct pmm_stats pmm_get_stats(void) { return (struct pmm_stats){.allocated_pages=live}; }
uint64_t pmm_alloc_page(void) {
    if(count==fail_after) return 0;
    used[count]=true;live++;
    return 0x100000+(count++)*4096;
}
int pmm_free_page(uint64_t p) {
    unsigned i=(p-0x100000)/4096;
    assert(i<64&&used[i]);used[i]=false;live--;return 1;
}
void *paging_phys_to_virt(uint64_t p) {
    assert(p>=0x100000&&p<0x140000);
    if(!map_ok) return NULL;
    return memory[(p-0x100000)/4096];
}
int vmx_memory_region_wb(uint64_t p) { return wb&&p>=0x100000&&p<0x140000; }
bool ept_test_invalidate(uint64_t p) { assert((p&0xfff)==30);flushes++;return invalidate_ok; }
_Noreturn void panic(const char *s) { (void)s;abort(); }
static uint64_t *ptr(uint64_t p) { return paging_phys_to_virt(p); }
int main(void) {
    uint32_t info=0,error=0;
    assert(ept_vectoring_gp_valid(48,0x181,0x58d0,0x80000b0d,0x28,1,1,&info,&error));
    assert(info==0x80000b0d && error==0x28);
    assert(ept_vectoring_gp_valid(48,1,0x58df,0x80001b0d,0x28,1,1,&info,&error));
    assert(info==0x80000b0d); /* undefined vectoring bit12 not copied to entry */
    for(unsigned i=0;i<7;i++) {
        uint64_t a[7]={48,1,0x58d0,0x80000b0d,0x28,1,1};
        a[i]^=(i==2?0x10:1);
        info=123;error=456;
        assert(!ept_vectoring_gp_valid(a[0],a[1],a[2],a[3],a[4],a[5],a[6],&info,&error));
        assert(info==123 && error==456);
    }
    assert(!ept_vectoring_gp_valid(48,1,0x58d0,0x80000b0d|(1u<<13),0x28,1,1,&info,&error));
    uint64_t caps=(1ULL<<6)|(1ULL<<14)|(1ULL<<20)|(1ULL<<25);
    assert(ept_caps_supported(caps));
    for(unsigned b=0;b<64;b++) if(caps&(1ULL<<b)) assert(!ept_caps_supported(caps&~(1ULL<<b)));
    for(unsigned phase=0;phase<3;phase++) {
        uint64_t q=0x180|(phase==0?1:phase==1?10:12),gpa=0x9000+phase*4096;
        assert(ept_fault_matches(phase,48,q,gpa,0x6000,0x6000));
        assert(!ept_fault_matches(phase,49,q,gpa,0x6000,0x6000));
        assert(!ept_fault_matches(phase,48|(1u<<31),q,gpa,0x6000,0x6000));
        assert(!ept_fault_matches(phase,48,q^1,gpa,0x6000,0x6000));
        assert(!ept_fault_matches(phase,48,q^0x100,gpa,0x6000,0x6000));
        assert(!ept_fault_matches(phase,48,q,gpa+8,0x6000,0x6000));
        assert(!ept_fault_matches(phase,48,q,gpa,0x6001,0x6000));
    }
    assert(!ept_fault_matches(3,48,0,0,0,0));
    struct ept_fixture f;
    f=(struct ept_fixture){0};assert(!ept_fixture_remediate(&f,0));
    assert(ept_fixture_init(&f));assert(live==15&&flushes==1);
    uint64_t *leaves=ptr(f.tables[3]);
    for(unsigned i=0;i<512;i++) {
        if(i==0||i>11||i==9) {assert(leaves[i]==0);continue;}
        assert((leaves[i]&~0xfffULL)==f.pages[i-1]);
        assert((leaves[i]&7)==(i==6?5:(i==10||i==11)?1:3));
        assert(((leaves[i]>>3)&7)==6);
    }
    assert(ptr(f.pages[0])[0]==0x2003);
    assert(ptr(f.pages[1])[0]==0x3003);
    assert(ptr(f.pages[2])[0]==0x4003);
    for(unsigned i=1;i<=11;i++) assert(ptr(f.pages[3])[i]==(i*4096|3));
    for(unsigned phase=0;phase<3;phase++) {
        assert(ept_fixture_remediate(&f,phase));
        assert((leaves[9+phase]&7)==(phase==2?5:3));
    }
    assert(!ept_fixture_data_ok(&f));
    ptr(f.pages[7])[0]=0x42;ptr(f.pages[9])[0]=0x77;
    assert(ept_fixture_data_ok(&f));
    assert(flushes==4&&!ept_fixture_remediate(&f,3));
    uint64_t saved_idt=leaves[5];
    assert(ept_fixture_idt_access(&f,false));assert(leaves[5]==0);
    assert(!ept_fixture_idt_access(&f,false));
    assert(ept_fixture_idt_access(&f,true));assert(leaves[5]==saved_idt);
    assert(!ept_fixture_idt_access(&f,true));
    invalidate_ok=false;
    assert(!ept_fixture_idt_access(&f,false));assert(live==15 && leaves[5]==0);
    invalidate_ok=true;assert(ept_fixture_idt_access(&f,true));
    invalidate_ok=false;
    assert(!ept_fixture_remediate(&f,0));assert(live==15);
    invalidate_ok=true;
    assert(ept_fixture_flush(&f));
    ept_fixture_destroy(&f);assert(live==0&&f.eptp==0);
    count=0;wb=false;assert(!ept_fixture_init(&f));assert(live==0);wb=true;
    count=0;map_ok=false;assert(!ept_fixture_init(&f));assert(live==0);map_ok=true;
    count=0;invalidate_ok=false;assert(!ept_fixture_init(&f));assert(live==0);invalidate_ok=true;
    for(unsigned stop=0;stop<15;stop++) {
        count=0;fail_after=stop;
        assert(!ept_fixture_init(&f));assert(live==0);
    }
    count=0;fail_after=64;
    struct ept_fixture second;
    unsigned before_flush=flushes;
    assert(ept_fixture_prepare(&f));assert(ept_fixture_prepare(&second));
    assert(flushes==before_flush&&live==30);
    uint64_t second_data=second.pages[7];
    /* Sharing a leaf must not transfer backing ownership. */
    ptr(f.tables[3])[8]=second_data|0x33;
    ept_fixture_owned_release(&f);
    assert(live==15&&used[(second_data-0x100000)/4096]);
    assert(!f.started&&!f.eptp);
    ept_fixture_owned_release(&f);assert(live==15);
    ept_fixture_owned_release(&second);assert(live==0);
    count=0;fail_after=64;assert(ept_fixture_prepare(&f));
    fail_after=count+3;before_flush=flushes;
    assert(!ept_fixture_prepare(&second));assert(live==15&&flushes==before_flush);
    ept_fixture_owned_release(&f);assert(!live);
    assert(!ept_fixture_prepare(NULL));
}
