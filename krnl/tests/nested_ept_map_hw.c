#include "../nested_ept_map_hw.h"
#include "../memory.h"
#include "../paging.h"
#include "../vmx_memory.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PAGES 64
static _Alignas(4096) unsigned char memory[TEST_PAGES][4096];
static bool used[TEST_PAGES], flush_ok=true, catch_panic;
static unsigned next_page,live,flushes,fail_alloc_at=TEST_PAGES;
static jmp_buf panic_target;
static struct nested_ept_map_hw c;
static struct ept_fixture f;

uint64_t pmm_alloc_page(void) {
    if(next_page>=TEST_PAGES || next_page==fail_alloc_at) return 0;
    used[next_page]=true;live++;
    return UINT64_C(0x100000)+(next_page++)*4096;
}
int pmm_free_page(uint64_t p) {
    assert(p>=0x100000 && !(p&4095));
    unsigned i=(unsigned)((p-0x100000)/4096);
    assert(i<TEST_PAGES && used[i]);used[i]=false;live--;return 1;
}
void *paging_phys_to_virt(uint64_t p) {
    if(p<0x100000 || p>=0x100000+TEST_PAGES*4096 || (p&4095)) return NULL;
    unsigned i=(unsigned)((p-0x100000)/4096);
    return used[i]?memory[i]:NULL;
}
int vmx_memory_region_wb(uint64_t p) { return paging_phys_to_virt(p)!=NULL; }
bool nested_map_test_flush(uint64_t eptp) {
    assert((eptp&4095)==0x1e);flushes++;return flush_ok;
}
_Noreturn void panic(const char *s) {
    if(catch_panic) longjmp(panic_target,1);
    fprintf(stderr,"unexpected panic: %s\n",s);abort();
}
static uint64_t *ptr(uint64_t p) { return paging_phys_to_virt(p); }
static void fixture_init(void) {
    assert(EPT_GUEST_PAGES==19);
    memset(memory,0,sizeof memory);memset(used,0,sizeof used);
    next_page=live=flushes=0;flush_ok=true;fail_alloc_at=TEST_PAGES;
    f=(struct ept_fixture){0};
    for(unsigned i=0;i<4;i++) f.tables[i]=pmm_alloc_page();
    for(unsigned i=0;i<3;i++) ptr(f.tables[i])[0]=f.tables[i+1]|7;
    for(unsigned i=0;i<EPT_GUEST_PAGES;i++) {
        f.pages[i]=pmm_alloc_page();
        ptr(f.tables[3])[i+1]=f.pages[i]|0x37;
    }
    ptr(f.pages[15])[0]=0x11007;
    ptr(f.pages[16])[0]=0x12007;
    ptr(f.pages[17])[0]=0x13007;
    ptr(f.pages[18])[0]=0x8037;
    ptr(f.pages[18])[15]=0x8037;
    ptr(f.pages[18])[63]=0x8037;
}
static void fixture_free(void) {
    for(unsigned i=0;i<4;i++) assert(pmm_free_page(f.tables[i]));
    for(unsigned i=0;i<EPT_GUEST_PAGES;i++) assert(pmm_free_page(f.pages[i]));
    assert(live==0);
}
int main(void) {
    fixture_init();
    unsigned fixture_live=live;
    assert(nested_ept_map_hw_init(&c,&f));
    assert(live==fixture_live+4 && c.generation==1 && c.published && !c.poisoned);
    assert(c.eptp==(c.tables[0]|0x1e));
    for(unsigned i=0;i<3;i++) assert(ptr(c.tables[i])[0]==(c.tables[i+1]|7));
    assert(ptr(c.tables[3])[0]==(f.pages[7]|0x37));
    assert(ptr(c.tables[3])[15]==ptr(c.tables[3])[0]);
    assert(ptr(c.tables[3])[63]==ptr(c.tables[3])[0]);
    for(unsigned i=64;i<512;i++) assert(ptr(c.tables[3])[i]==0);

    ptr(f.pages[18])[15]=0; /* remove old alias completely */
    ptr(f.pages[16])[0]&=~UINT64_C(2); /* AND upper permissions down to RX */
    assert(nested_ept_map_hw_rebuild(&c,&f));
    assert(c.generation==2 && ptr(c.tables[3])[15]==0);
    assert(ptr(c.tables[3])[0]==(f.pages[7]|0x35));
    assert(c.committed.pages[0].effective12==5);
    struct nested_ept_map saved=c.committed;
    uint64_t saved_leaves[512];memcpy(saved_leaves,ptr(c.tables[3]),sizeof saved_leaves);
    uint64_t old_l0=ptr(f.tables[3])[8];
    ptr(f.tables[3])[8]=0; /* L0 backing missing: no physical publication */
    assert(!nested_ept_map_hw_rebuild(&c,&f));
    assert(c.generation==2 && !c.poisoned);
    assert(memcmp(&saved,&c.committed,sizeof saved)==0);
    assert(memcmp(saved_leaves,ptr(c.tables[3]),sizeof saved_leaves)==0);
    ptr(f.tables[3])[8]=old_l0;

    /* Validation succeeds, physical leaves change, but invalidation fails:
     * no committed generation advance, context poisoned and unusable. */
    ptr(f.pages[18])[0]=0;
    flush_ok=false;
    assert(!nested_ept_map_hw_rebuild(&c,&f));
    assert(c.poisoned && c.generation==2 && ptr(c.tables[3])[0]==0);
    assert(memcmp(&saved,&c.committed,sizeof saved)==0);
    unsigned flush_before=flushes;
    assert(!nested_ept_map_hw_rebuild(&c,&f));
    assert(flushes==flush_before && c.generation==2);
    catch_panic=true;
    if(setjmp(panic_target)==0) {
        nested_ept_map_hw_destroy(&c);
        assert(!"failed destroy INVEPT must panic and retain pages");
    }
    catch_panic=false;
    assert(live==fixture_live+4 && c.published && c.poisoned);
    for(unsigned i=0;i<4;i++) assert(ptr(c.tables[i])!=NULL);
    flush_ok=true;nested_ept_map_hw_destroy(&c);
    assert(live==fixture_live && !c.published && !c.poisoned && !c.eptp);
    fixture_free();

    /* Initialization failure leaves allocations owned until caller cleanup. */
    for(unsigned allocated=0;allocated<4;allocated++) {
        fixture_init();fixture_live=live;
        fail_alloc_at=next_page+allocated;
        assert(!nested_ept_map_hw_init(&c,&f));
        assert(live==fixture_live+allocated && !c.published);
        nested_ept_map_hw_destroy(&c);assert(live==fixture_live);
        fixture_free();
    }
    fixture_init();fixture_live=live;flush_ok=false;
    assert(!nested_ept_map_hw_init(&c,&f));
    assert(c.poisoned && c.published && c.generation==0 && live==fixture_live+4);
    flush_ok=true;nested_ept_map_hw_destroy(&c);fixture_free();
    puts("PASS EPT02 hardware plan publication, poisoning and retained-page cleanup");
}
