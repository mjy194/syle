#include "../nested_domain_memory.h"
#include "../memory.h"
#include "../paging.h"
#include "../vmx_memory.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define POOL 96
static _Alignas(4096) unsigned char ram[POOL][4096];
static bool live[POOL];
static unsigned allocations,frees,release_calls,fail_at;
static unsigned bad_wb,bad_map;
static jmp_buf panic_jump;
static bool expect_panic;
static unsigned index_of(uint64_t p) {
    assert(p>=0x100000 && !(p&4095));
    unsigned i=(unsigned)((p-0x100000)/4096);
    assert(i<POOL);return i;
}
uint64_t pmm_alloc_page_below(uint64_t limit) {
    assert(limit==UINT64_C(0x100000000));
    if(allocations==fail_at)return 0;
    assert(allocations<POOL);
    unsigned i=allocations++;
    assert(!live[i]);live[i]=true;
    memset(ram[i],0,4096);
    return 0x100000+(uint64_t)i*4096;
}
int pmm_free_page(uint64_t p) {
    unsigned i=index_of(p);assert(live[i]);live[i]=false;frees++;return 1;
}
void *paging_phys_to_virt(uint64_t p) {
    unsigned i=index_of(p);assert(live[i]);
    return i==bad_map?NULL:ram[i];
}
int vmx_memory_region_wb(uint64_t p) {return index_of(p)!=bad_wb;}
_Noreturn void panic(const char *message) {
    assert(message && expect_panic);
    longjmp(panic_jump,1);
}
static uint64_t *ptr(uint64_t p) {return paging_phys_to_virt(p);}
bool ept_fixture_prepare(struct ept_fixture *f) {
    memset(f,0,sizeof *f);
    for(unsigned i=0;i<EPT_GUEST_PAGES;i++)
        if(!(f->pages[i]=pmm_alloc_page_below(UINT64_C(0x100000000))))abort();
    for(unsigned i=0;i<4;i++)
        if(!(f->tables[i]=pmm_alloc_page_below(UINT64_C(0x100000000))))abort();
    /* Distinct code/descriptors and private stacks expose copying mistakes. */
    for(unsigned i=4;i<14;i++)
        for(unsigned j=0;i!=9 && j<512;j++)ptr(f->pages[i])[j]=((uint64_t)i<<32)|j;
    for(unsigned l=0;l<3;l++)ptr(f->tables[l])[0]=f->tables[l+1]|7;
    for(unsigned n=1;n<=EPT_GUEST_PAGES;n++)ptr(f->tables[3])[n]=f->pages[n-1]|0x30|(n==6?5:3);
    f->eptp=f->tables[0]|0x1e;
    return true;
}
void ept_fixture_owned_release(struct ept_fixture *f) {
    release_calls++;
    for(unsigned i=0;i<EPT_GUEST_PAGES;i++)assert(pmm_free_page(f->pages[i]));
    for(unsigned i=0;i<4;i++)assert(pmm_free_page(f->tables[i]));
    memset(f,0,sizeof *f);
}
static void reset(void) {
    memset(live,0,sizeof live);
    allocations=frees=release_calls=0;
    fail_at=bad_wb=bad_map=POOL;
}
static unsigned perm(unsigned n) {return n==6?5:(n>=16&&n<=19?1:3);}
static uint64_t gva_to_gpa(struct nested_domain_memory *m,unsigned cpu,uint64_t gva) {
    uint64_t gpa=cpu?0x21000:0x1000;
    for(int shift=39;shift>=12;shift-=9) {
        uint64_t entry=ptr(m->backing.pages[gpa/4096-1])[(gva>>shift)&511];
        assert((entry&0x63)==0x63);
        gpa=entry&~UINT64_C(4095);
    }
    return gpa+(gva&4095);
}
static bool domain_resolve(void *opaque,uint64_t gpa,uint8_t need,
                           uint64_t *hpa,const uint64_t **view,uint8_t *permissions) {
    (void)need;
    struct nested_domain_memory *m=opaque;
    if(!gpa||(gpa&4095)||gpa>=0x40000)return false;
    unsigned n=gpa/4096;
    uint64_t entry=ptr(m->root01[0][3])[n];
    *hpa=entry&~UINT64_C(4095);*permissions=entry&7;*view=ptr(*hpa);
    return true;
}
static void atomic_ad_tests(void) {
    reset();struct nested_domain_memory m;
    assert(nested_domain_memory_prepare(&m));
    for(unsigned kind=0;kind<3;kind++) {
        uint64_t *t=kind==2?ptr(m.backing.pages[18]):ptr(m.root01[kind][3]);
        uint64_t old=t[16];t[16]|=2;
        assert(!nested_domain_memory_prepare_atomic_ad(&m)&&!m.atomic_ad_prepared);
        t[16]=old;
        uint64_t alias=t[20];t[20]=(old&~UINT64_C(7))|3;
        assert(!nested_domain_memory_prepare_atomic_ad(&m));t[20]=alias;
    }
    uint64_t old=m.backing.pages[20];m.backing.pages[20]=m.backing.pages[15];
    assert(!nested_domain_memory_prepare_atomic_ad(&m));m.backing.pages[20]=old;
    uint64_t *root=ptr(m.backing.pages[15]);root[0]|=0x100;
    assert(!nested_domain_memory_prepare_atomic_ad(&m));root[0]&=~UINT64_C(0x100);
    assert(nested_domain_memory_prepare_atomic_ad(&m)&&m.atomic_ad_prepared);
    assert(!nested_domain_memory_prepare_atomic_ad(&m));
    assert(!nested_domain_memory_prepare_contexts(&m));
    assert(!nested_domain_memory_prepare_replacement(&m));
    assert(!nested_domain_memory_remap_data(&m,0xf000,true));
    for(unsigned cpu=0;cpu<2;cpu++) {
        uint64_t *leaf=ptr(m.root01[cpu][3]);
        for(unsigned n=16;n<=19;n++)assert((leaf[n]&7)==1);
        for(unsigned n=1;n<=4;n++)assert((leaf[n]&7)==3&&(leaf[n+32]&7)==3);
    }
    nested_domain_memory_release(&m);assert(!m.atomic_ad_prepared&&frees==allocations);
    reset();assert(nested_domain_memory_prepare(&m));
    assert(nested_domain_memory_prepare_contexts(&m));
    assert(!nested_domain_memory_prepare_atomic_ad(&m));nested_domain_memory_release(&m);
}
static void typed_uc_tests(void) {
 reset();struct nested_domain_memory m;assert(nested_domain_memory_prepare(&m));
 uint64_t *oldleaf=ptr(m.backing.tables[3]);uint64_t old=oldleaf[20];
 oldleaf[20]=m.backing.pages[14]|0x33;
 assert(!nested_domain_memory_prepare_typed_uc(&m)&&!m.typed_uc_prepared);
 assert((ptr(m.root01[0][3])[15]&0xfff)==0x33);oldleaf[20]=old;
 assert(nested_domain_memory_prepare_typed_uc(&m));
    assert(nested_domain_memory_typed_ready(&m));
    ptr(m.root01[1][3])[10]=m.backing.pages[9]|0x33;
    assert(!nested_domain_memory_typed_ready(&m));
    ptr(m.root01[1][3])[10]=m.backing.pages[9]|0x73;
    assert(nested_domain_memory_typed_ready(&m));
 assert(!nested_domain_memory_prepare_typed_uc(&m));
 assert(!nested_domain_memory_prepare_atomic_ad(&m));
 assert(!nested_domain_memory_prepare_contexts(&m));
 assert(!nested_domain_memory_prepare_replacement(&m));
 for(unsigned n=1;n<64;n++){
  for(unsigned cpu=0;cpu<2;cpu++){
   assert(ptr(m.root01[cpu][3])[n]==(m.backing.pages[n-1]|0x70|perm(n)));
   assert(ptr(m.root02[cpu][3])[n]==(m.backing.pages[n-1]|0x70|perm(n)));
  }
  assert(oldleaf[n]==(m.backing.pages[n-1]|0x70|(n==6?5:3)));
  assert(ptr(m.backing.pages[18])[n]==((uint64_t)n*4096|0x70|perm(n)));
 }
 nested_domain_memory_release(&m);assert(!m.typed_uc_prepared&&allocations==frees);
}
static void typed_context_tests(void) {
 reset();struct nested_domain_memory m;assert(nested_domain_memory_prepare(&m));
 uint64_t raw=ptr(m.root01[0][3])[20];ptr(m.root01[0][3])[20]^=2;
 assert(!nested_domain_memory_prepare_typed_contexts(&m)&&!m.typed_contexts_prepared&&!m.typed_uc_prepared);
 ptr(m.root01[0][3])[20]=raw;
 assert(nested_domain_memory_prepare_typed_contexts(&m));
 assert(m.typed_contexts_prepared&&m.contexts_prepared&&m.typed_uc_prepared);
 assert(nested_domain_memory_typed_ready(&m));
 assert(!nested_domain_memory_prepare_typed_contexts(&m));
 assert(!nested_domain_memory_prepare_eviction(&m));
 for(unsigned n=16;n<=23;n++) {
  for(unsigned cpu=0;cpu<2;cpu++) {
   assert(ptr(m.root01[cpu][3])[n]==(m.backing.pages[n-1]|0x71));
   assert(ptr(m.root02[cpu][3])[n]==(m.backing.pages[n-1]|0x71));
  }
  assert(ptr(m.backing.tables[3])[n]==(m.backing.pages[n-1]|0x71));
  assert(ptr(m.backing.pages[18])[n]==((uint64_t)n*4096|0x71));
  assert(ptr(m.backing.pages[22])[n]==((uint64_t)n*4096|0x71));
 }
 assert(ptr(m.backing.pages[18])[15]==0xf073&&ptr(m.backing.pages[22])[15]==0xf073);
 assert(ptr(m.backing.pages[18])[48]==0x30073&&ptr(m.backing.pages[22])[48]==0x31073);
 assert(ptr(m.backing.pages[47])[0]==UINT64_C(0x38ba38ba38ba38ba));
 assert(ptr(m.backing.pages[48])[0]==UINT64_C(0x38bb38bb38bb38bb));
 ptr(m.backing.pages[22])[15]=0xb073;assert(!nested_domain_memory_typed_ready(&m));
 ptr(m.backing.pages[22])[15]=0xf073;
 ptr(m.backing.tables[3])[20]|=2;assert(!nested_domain_memory_typed_ready(&m));
 ptr(m.backing.tables[3])[20]&=~UINT64_C(2);assert(nested_domain_memory_typed_ready(&m));
 nested_domain_memory_release(&m);assert(!m.typed_contexts_prepared&&allocations==frees);
}
static void typed_dynamic_tests(void) {
    reset();struct nested_domain_memory m;assert(nested_domain_memory_prepare(&m));
    static unsigned char before[sizeof ram];
    assert(!nested_domain_memory_prepare_typed_dynamic(NULL));
    /* Every failure leaves both bytes and publication flags untouched. */
    for(unsigned kind=0;kind<5;kind++) {
        uint64_t *slot=kind==0?&ptr(m.root02[1][3])[511]:
            kind==1?&ptr(m.backing.pages[18])[24]:
            kind==2?&m.backing.pages[24]:
            kind==3?&m.root02[1][2]:&ptr(m.backing.tables[2])[511];
        uint64_t old=*slot;
        *slot=kind==2?m.backing.pages[23]:kind==3?m.root01[0][2]:old^2;
        memcpy(before,ram,sizeof ram);struct nested_domain_memory saved=m;
        assert(!nested_domain_memory_prepare_typed_dynamic(&m));
        assert(!memcmp(&m,&saved,sizeof m)&&!memcmp(before,ram,sizeof ram));
        *slot=old;
    }
    for(unsigned n=23;n<=26;n++) {
        bad_map=index_of(m.backing.pages[n]);
        memcpy(before,ram,sizeof ram);struct nested_domain_memory saved=m;
        assert(!nested_domain_memory_prepare_typed_dynamic(&m));
        assert(!memcmp(&m,&saved,sizeof m)&&!memcmp(before,ram,sizeof ram));
        bad_map=POOL;
    }
    assert(nested_domain_memory_prepare_typed_dynamic(&m));
    assert(m.typed_dynamic_prepared&&m.typed_contexts_prepared&&m.typed_uc_prepared&&
           m.contexts_prepared&&m.eviction_prepared&&!m.atomic_ad_prepared);
    assert(allocations==83&&!frees&&nested_domain_memory_typed_ready(&m));
    memcpy(before,ram,sizeof ram);struct nested_domain_memory saved=m;
    assert(!nested_domain_memory_prepare_typed_dynamic(&m));
    assert(!nested_domain_memory_prepare_typed_contexts(&m));
    assert(!nested_domain_memory_prepare_eviction(&m));
    assert(!nested_domain_memory_prepare_atomic_ad(&m));
    assert(!nested_domain_memory_prepare_replacement(&m));
    assert(!nested_domain_memory_remap_data(&m,0xf000,true));
    assert(!memcmp(&m,&saved,sizeof m)&&!memcmp(before,ram,sizeof ram));
    const uint64_t witnesses[]={UINT64_C(0x38ca38ca38ca38ca),
        UINT64_C(0x38cb38cb38cb38cb),UINT64_C(0x38cc38cc38cc38cc)};
    struct nested_ept_policy policy={.maxphys=48};
    for(unsigned root=0;root<3;root++) {
        uint64_t key=0x1001e + root*0x4000;
        struct nested_ept_result r=nested_ept_walk_memory(key,0xf000,2,domain_resolve,&m,&policy);
        assert(r.status==NESTED_EPT_OK&&r.leaf_resolved&&r.l1_gpa==0xf000&&
               r.hpa==m.backing.pages[14]&&r.effective02==3&&r.ignore_pat&&r.leaf_type==6);
        r=nested_ept_walk_memory(key,0x30000,1,domain_resolve,&m,&policy);
        assert(r.status==NESTED_EPT_OK&&r.l1_gpa==(48+root)*4096&&
               r.hpa==m.backing.pages[47+root]&&r.ignore_pat&&r.leaf_type==6);
        assert(ptr(r.hpa)[0]==witnesses[root]);
        for(unsigned n=16;n<=27;n++) {
            r=nested_ept_walk_memory(key,n*4096,1,domain_resolve,&m,&policy);
            assert(r.status==NESTED_EPT_OK&&r.effective02==1&&r.ignore_pat&&r.leaf_type==6);
            r=nested_ept_walk_memory(key,n*4096,2,domain_resolve,&m,&policy);
            assert(r.status==NESTED_EPT_L1_DENIED);
            /* Both guest CR3s expose the same RO EPT12 page aliases. */
            for(unsigned cpu=0;cpu<2;cpu++)assert(gva_to_gpa(&m,cpu,n*4096)==n*4096);
        }
        for(unsigned l=0;l<4;l++) {
            uint64_t *table=ptr(m.backing.pages[15+4*root+l]);
            table[511]=m.backing.pages[14]|0x73;
            assert(!nested_domain_memory_typed_ready(&m));table[511]=0;
        }
    }
    for(unsigned r=0;r<5;r++) {
        uint64_t *tables=r<2?m.root01[r]:r<4?m.root02[r-2]:m.backing.tables;
        for(unsigned l=0;l<4;l++) {
            uint64_t *table=ptr(tables[l]);
            table[511]=m.backing.pages[23]|0x73;
            assert(!nested_domain_memory_typed_ready(&m));table[511]=0;
        }
        for(unsigned n=16;n<=27;n++) {
            uint64_t *table=ptr(tables[3]);assert(table[n]==(m.backing.pages[n-1]|0x71));
            table[n]|=2;assert(!nested_domain_memory_typed_ready(&m));table[n]&=~UINT64_C(2);
        }
    }
    uint64_t old=m.backing.pages[49];m.backing.pages[49]=m.backing.pages[48];
    assert(!nested_domain_memory_typed_ready(&m));m.backing.pages[49]=old;
    for(unsigned root=0;root<3;root++) {
        ptr(m.backing.pages[47+root])[0]^=1;
        assert(!nested_domain_memory_typed_ready(&m));ptr(m.backing.pages[47+root])[0]^=1;
        ptr(m.backing.pages[18+4*root])[15]=0xf043;
        assert(!nested_domain_memory_typed_ready(&m));ptr(m.backing.pages[18+4*root])[15]=0xf073;
    }
    assert(nested_domain_memory_typed_ready(&m));
    assert(!memcmp(before,ram,sizeof ram));
    nested_domain_memory_release(&m);
    assert(!m.typed_dynamic_prepared&&!m.eviction_prepared&&allocations==frees);
    reset();assert(nested_domain_memory_prepare(&m));
    assert(nested_domain_memory_prepare_typed_contexts(&m));
    memcpy(before,ram,sizeof ram);saved=m;
    assert(!nested_domain_memory_prepare_typed_dynamic(&m));
    assert(!memcmp(&m,&saved,sizeof m)&&!memcmp(before,ram,sizeof ram));
    assert(nested_domain_memory_typed_ready(&m));nested_domain_memory_release(&m);
}
int main(void) {
    static struct nested_domain_memory m;
    reset();
    assert(!nested_domain_memory_prepare(NULL));
    assert(nested_domain_memory_prepare(&m));
    assert(m.ready && allocations==83 && !frees);
    for(unsigned cpu=0;cpu<2;cpu++) {
        struct ept_fixture view;
        assert(nested_domain_memory_view(&m,cpu,&view));
        assert(!memcmp(view.pages,m.backing.pages,sizeof view.pages));
        assert(view.eptp==(m.root01[cpu][0]|0x1e));
        for(unsigned level=0;level<3;level++)
            for(unsigned slot=0;slot<512;slot++) {
                assert(ptr(m.root01[cpu][level])[slot]==
                       (slot?0:(m.root01[cpu][level+1]|7)));
                assert(ptr(m.root02[cpu][level])[slot]==
                       (slot?0:(m.root02[cpu][level+1]|7)));
            }
        for(unsigned n=0;n<512;n++) {
            uint64_t expected=n&&n<64?m.backing.pages[n-1]|0x30|perm(n):0;
            assert(ptr(m.root01[cpu][3])[n]==expected);
            assert(ptr(m.root02[cpu][3])[n]==expected);
            if(n&&n<64)assert(ptr(m.backing.pages[18])[n]==
                              (((uint64_t)n*4096)|0x30|perm(n)));
        }
        const unsigned private_pages[]={1,2,3,4,5,7,8,9,11,12,13,14};
        for(unsigned i=0;i<sizeof private_pages/sizeof private_pages[0];i++)
            assert(gva_to_gpa(&m,cpu,private_pages[i]*4096+24)==
                   (private_pages[i]+cpu*32)*4096+24);
        const unsigned shared[]={6,10,15,16,17,18,19};
        for(unsigned i=0;i<sizeof shared/sizeof shared[0];i++)
            assert(gva_to_gpa(&m,cpu,shared[i]*4096)==shared[i]*4096);
        assert(ptr(m.backing.pages[cpu*32+11])[0x828/8]==cpu);
    }
    /* The same counter GVA is private through distinct guest CR3s, while
     * both EPT01s retain the same GPA->HPA map. Deliberately leave CPU0
     * incomplete: CPU1 completion must depend only on its own config GPA. */
    uint64_t counter0=gva_to_gpa(&m,0,0xc820);
    uint64_t counter1=gva_to_gpa(&m,1,0xc820);
    assert(counter0==0xc820 && counter1==0x2c820);
    uint64_t *value0=&ptr(m.backing.pages[(counter0>>12)-1])[(counter0&4095)/8];
    uint64_t *value1=&ptr(m.backing.pages[(counter1>>12)-1])[(counter1&4095)/8];
    assert(value0!=value1);
    *value0=1;*value1=2;
    assert(*value1==2 && *value0!=2);
    *value0=2;*value1=1;
    assert(*value0==2 && *value1!=2);
    *value0=0;*value1=0;
    assert(!memcmp(ptr(m.backing.pages[4]),ptr(m.backing.pages[36]),4096));
    assert(!memcmp(ptr(m.backing.pages[7]),ptr(m.backing.pages[39]),4096));
    for(unsigned i=0;i<512;i++)assert(ptr(m.backing.pages[9])[i]==0);
    assert(ptr(m.backing.pages[14])[0]==UINT64_C(0x3344334433443344));
    struct ept_fixture unchanged,snapshot;
    memset(&unchanged,0xa5,sizeof unchanged);snapshot=unchanged;
    assert(!nested_domain_memory_view(&m,2,&unchanged));
    assert(!memcmp(&unchanged,&snapshot,sizeof snapshot));
    nested_domain_memory_release(&m);
    assert(!m.ready && frees==83 && release_calls==1);
    for(unsigned i=0;i<POOL;i++)assert(!live[i]);
    expect_panic=true;
    if(!setjmp(panic_jump)) {nested_domain_memory_release(&m);abort();}
    expect_panic=false;
    assert(frees==83 && release_calls==1);

    /* A partially prepared domain is deliberately retained, not reusable or
     * releasable by the successful-domain API. No CPU has entered in this test. */
    for(volatile unsigned stop=67;stop<83;stop++) {
        reset();fail_at=stop;
        assert(!nested_domain_memory_prepare(&m));
        assert(!m.ready && allocations==stop && !frees && !release_calls);
        assert(!nested_domain_memory_view(&m,0,&unchanged));
        expect_panic=true;
        if(!setjmp(panic_jump)){nested_domain_memory_release(&m);abort();}
        expect_panic=false;
        assert(!frees);
    }
    for(unsigned kind=0;kind<2;kind++) {
        reset();
        if(kind)bad_map=67;else bad_wb=67;
        assert(!nested_domain_memory_prepare(&m));
        assert(!m.ready && allocations==68 && !frees);
    }
    reset();
    assert(nested_domain_memory_prepare(&m));
    assert(!nested_domain_memory_remap_data(&m,0xf000,true));
    assert(nested_domain_memory_prepare_replacement(&m));
    assert(allocations==84 && !frees);
    assert(!nested_domain_memory_prepare_replacement(&m));
    uint64_t old=m.backing.pages[14], replacement=m.replacement_hpa;
    for(unsigned j=0;j<512;j++)assert(ptr(replacement)[j]==0);
    ptr(replacement)[0]=0x1234;
    assert(!nested_domain_memory_remap_data(&m,0xf000,false));
    assert(!nested_domain_memory_remap_data(&m,0xe000,true));
    /* Every alias-table position must be audited, including unused tail. */
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned kind=0;kind<2;kind++) {
        uint64_t *roots=kind?m.root02[cpu]:m.root01[cpu];
        for(unsigned level=0;level<4;level++) {
            uint64_t *table=ptr(roots[level]),saved=table[511];
            table[511]=old|0x33;
            assert(!nested_domain_memory_remap_data(&m,0xf000,true));
            assert(m.backing.pages[14]==old && m.replacement_hpa==replacement);
            for(unsigned c=0;c<2;c++) {
                assert(ptr(m.root01[c][3])[15]==(old|0x33));
                assert(ptr(m.root02[c][3])[15]==(old|0x33));
            }
            table[511]=saved;
        }
        uint64_t *table=ptr(roots[3]),saved=table[15];
        table[15]^=8; /* WB type tamper */
        assert(!nested_domain_memory_remap_data(&m,0xf000,true));
        table[15]=saved;
    }
    for(unsigned n=0;n<EPT_GUEST_PAGES;n++) {
        m.replacement_hpa=m.backing.pages[n];
        assert(!nested_domain_memory_remap_data(&m,0xf000,true));
    }
    for(unsigned l=0;l<4;l++) {
        m.replacement_hpa=m.backing.tables[l];
        assert(!nested_domain_memory_remap_data(&m,0xf000,true));
        for(unsigned cpu=0;cpu<2;cpu++) {
            m.replacement_hpa=m.root01[cpu][l];
            assert(!nested_domain_memory_remap_data(&m,0xf000,true));
            m.replacement_hpa=m.root02[cpu][l];
            assert(!nested_domain_memory_remap_data(&m,0xf000,true));
        }
    }
    m.replacement_hpa=replacement;
    bad_wb=index_of(replacement);
    assert(!nested_domain_memory_remap_data(&m,0xf000,true));
    bad_wb=POOL;
    assert(nested_domain_memory_remap_data(&m,0xf000,true));
    assert(m.backing.pages[14]==replacement && !m.replacement_hpa && m.retained_hpa==old);
    assert(live[index_of(old)] && live[index_of(replacement)] && !frees);
    for(unsigned cpu=0;cpu<2;cpu++) {
        assert(ptr(m.root01[cpu][3])[15]==(replacement|0x33));
        assert(ptr(m.root02[cpu][3])[15]==(replacement|0x33));
    }
    assert(!nested_domain_memory_remap_data(&m,0xf000,true));
    assert(!nested_domain_memory_prepare_replacement(&m));
    nested_domain_memory_release(&m);
    assert(frees==84 && release_calls==1);
    for(unsigned n=0;n<POOL;n++)assert(!live[n]);

    reset();assert(nested_domain_memory_prepare(&m));
    assert(nested_domain_memory_prepare_replacement(&m));
    nested_domain_memory_release(&m); /* Prepared but unused replacement. */
    assert(frees==84 && release_calls==1);

    for(unsigned failure=0;failure<3;failure++) {
        reset();assert(nested_domain_memory_prepare(&m));
        if(failure==0)fail_at=83;
        if(failure==1)bad_wb=83;
        if(failure==2)bad_map=83;
        assert(!nested_domain_memory_prepare_replacement(&m));
        assert(!m.replacement_hpa && !m.retained_hpa && m.ready);
        assert(frees==(failure?1u:0u));
        bad_map=bad_wb=POOL;
        nested_domain_memory_release(&m);
        assert(frees==allocations);
    }
    reset();assert(nested_domain_memory_prepare(&m));
    uint64_t config=ptr(m.backing.pages[11])[1];
    uint64_t private_config=ptr(m.backing.pages[43])[1];
    /* Shape tampering must reject before modifying root B or data. */
    uint64_t saved=ptr(m.root02[1][3])[20];
    ptr(m.root02[1][3])[20]^=8;
    assert(!nested_domain_memory_prepare_contexts(&m));
    assert(!m.contexts_prepared);
    ptr(m.root02[1][3])[20]=saved;
    assert(nested_domain_memory_prepare_contexts(&m));
    assert(m.contexts_prepared && allocations==83 && !frees);
    assert(!nested_domain_memory_prepare_contexts(&m));
    assert(!nested_domain_memory_prepare_replacement(&m));
    assert(!nested_domain_memory_remap_data(&m,0xf000,true));
    assert(ptr(m.backing.pages[11])[1]==config);
    assert(ptr(m.backing.pages[43])[1]==private_config);
    assert(ptr(m.backing.pages[10])[0]==UINT64_C(0x7788778877887788));
    assert(ptr(m.backing.pages[14])[0]==UINT64_C(0x3344334433443344));
    for(unsigned root=0;root<2;root++) {
        uint64_t eptp=root?0x1401e:0x1001e;
        struct nested_ept_result r=nested_ept_walk(eptp,0xf000,1,domain_resolve,&m);
        assert(r.status==NESTED_EPT_OK && r.leaf_resolved);
        assert(r.l1_gpa==(root?0xb000:0xf000));
        assert(r.hpa==m.backing.pages[root?10:14] && r.effective02==3);
        for(unsigned n=16;n<=23;n++) {
            r=nested_ept_walk(eptp,n*4096,1,domain_resolve,&m);
            assert(r.status==NESTED_EPT_OK && r.effective02==1);
            r=nested_ept_walk(eptp,n*4096,2,domain_resolve,&m);
            assert(r.status==NESTED_EPT_L1_DENIED);
        }
        r=nested_ept_walk(eptp,0x6000,4,domain_resolve,&m);
        assert(r.status==NESTED_EPT_OK && r.effective02==5);
    }
    for(unsigned n=0;n<512;n++) {
        uint64_t expected=n&&n<64?m.backing.pages[n-1]|0x30|
            (n>=16&&n<=23?1:perm(n)):0;
        for(unsigned cpu=0;cpu<2;cpu++) {
            assert(ptr(m.root01[cpu][3])[n]==expected);
            assert(ptr(m.root02[cpu][3])[n]==expected);
        }
    }
    nested_domain_memory_release(&m);
    assert(frees==83 && !m.contexts_prepared);
    reset();assert(nested_domain_memory_prepare(&m));
    assert(!nested_domain_memory_prepare_eviction(&m));
    assert(nested_domain_memory_prepare_contexts(&m));
    uint64_t saved_b=ptr(m.backing.pages[22])[15];
    ptr(m.backing.pages[22])[15]^=8;
    assert(!nested_domain_memory_prepare_eviction(&m));
    assert(!m.eviction_prepared);
    ptr(m.backing.pages[22])[15]=saved_b;
    assert(nested_domain_memory_prepare_eviction(&m));
    assert(m.eviction_prepared && allocations==83 && !frees);
    assert(!nested_domain_memory_prepare_eviction(&m));
    assert(!nested_domain_memory_prepare_contexts(&m));
    const uint64_t roots3[]={0x1001e,0x1401e,0x1801e};
    const unsigned target3[]={15,11,28};
    const uint64_t data3[]={UINT64_C(0x3344334433443344),
        UINT64_C(0x7788778877887788),UINT64_C(0x99aa99aa99aa99aa)};
    for(unsigned root=0;root<3;root++) {
        struct nested_ept_result r=nested_ept_walk(roots3[root],0xf000,1,domain_resolve,&m);
        assert(r.status==NESTED_EPT_OK && r.effective02==3);
        assert(r.l1_gpa==target3[root]*4096);
        assert(r.hpa==m.backing.pages[target3[root]-1]);
        assert(ptr(r.hpa)[0]==data3[root]);
        for(unsigned n=16;n<=27;n++) {
            r=nested_ept_walk(roots3[root],n*4096,1,domain_resolve,&m);
            assert(r.status==NESTED_EPT_OK && r.effective02==1);
            r=nested_ept_walk(roots3[root],n*4096,2,domain_resolve,&m);
            assert(r.status==NESTED_EPT_L1_DENIED);
        }
    }
    for(unsigned n=0;n<512;n++) {
        uint64_t expected=n&&n<64?m.backing.pages[n-1]|0x30|
            (n>=16&&n<=27?1:perm(n)):0;
        for(unsigned cpu=0;cpu<2;cpu++) {
            assert(ptr(m.root01[cpu][3])[n]==expected);
            assert(ptr(m.root02[cpu][3])[n]==expected);
        }
    }
    assert(gva_to_gpa(&m,1,0xc000)==0x2c000);
    assert(ptr(m.backing.pages[43])[0x828/8]==1);
    nested_domain_memory_release(&m);
    assert(frees==83 && !m.eviction_prepared);
    atomic_ad_tests();
    typed_uc_tests();
    typed_context_tests();
    typed_dynamic_tests();
    puts("PASS domain shared GPA/HPA, private GVA roots/stacks, EPT permissions, owner release and failure retention");
    return 0;
}
