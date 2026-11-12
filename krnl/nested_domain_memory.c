#include "nested_domain_memory.h"
#include "memory.h"
#include "paging.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
_Static_assert(EPT_GUEST_PAGES==63,"shared domain backing shape");
static uint64_t *ptr(uint64_t p){return paging_phys_to_virt(p);}
static bool alloc(uint64_t *p){*p=pmm_alloc_page_below(UINT64_C(0x100000000));if(!*p||!vmx_memory_region_wb(*p)||!ptr(*p))return false;for(unsigned i=0;i<512;i++)ptr(*p)[i]=0;return true;}
static unsigned permission(unsigned page){return page==6?5:(page>=16&&page<=19?1:3);}
static bool resolve(void *opaque,uint64_t gpa,uint8_t need,uint64_t *hpa,const uint64_t **view,uint8_t *perm){
    (void)need;struct nested_domain_memory *m=opaque;
    if(!gpa||(gpa&4095)||gpa>=UINT64_C(64)*4096)return false;
    unsigned page=gpa>>12;*hpa=m->backing.pages[page-1];*view=ptr(*hpa);*perm=m->contexts_prepared&&page>=20&&page<=(m->eviction_prepared?27u:23u)?1:permission(page);return *hpa&&*view;
}
bool nested_domain_memory_prepare(struct nested_domain_memory *m){
    if(!m)return false;
    *m=(struct nested_domain_memory){0};
    if(!ept_fixture_prepare(&m->backing))return false;
    /* Own fixture tables are not used for execution; retained until final free. */
    uint64_t *pages=m->backing.pages;
    for(unsigned n=1;n<=14;n++)if(n!=6&&n!=10){
        uint64_t *src=ptr(pages[n-1]),*dst=ptr(pages[n+31]);
        for(unsigned i=0;i<512;i++)dst[i]=src[i];
    }
    for(unsigned cpu=0;cpu<2;cpu++){
        unsigned offset=cpu*32;
        for(unsigned level=0;level<3;level++){
            uint64_t *t=ptr(pages[offset+level]);
            for(unsigned j=0;j<512;j++)t[j]=0;
            t[0]=((uint64_t)(offset+level+2)*4096)|0x63;
        }
        uint64_t *pt=ptr(pages[offset+3]);
        for(unsigned j=0;j<512;j++)pt[j]=0;
        for(unsigned j=1;j<64;j++){
            unsigned mapped=j;
            if(cpu&&j<=14&&j!=6&&j!=10)mapped+=32;
            pt[j]=((uint64_t)mapped*4096)|0x63;
        }
        ptr(pages[offset+11])[0x828/8]=cpu;
    }
    /* Shared EPT12: immutable tables, root GPA10000, identity GPA translation. */
    for(unsigned n=16;n<=19;n++)for(unsigned i=0;i<512;i++)ptr(pages[n-1])[i]=0;
    for(unsigned n=16;n<19;n++)ptr(pages[n-1])[0]=((uint64_t)(n+1)*4096)|7;
    for(unsigned n=1;n<64;n++)ptr(pages[18])[n]=((uint64_t)n*4096)|0x30|permission(n);
    ptr(pages[14])[0]=UINT64_C(0x3344334433443344); /* same data GPA/HPA on both */
    for(unsigned cpu=0;cpu<2;cpu++){
        for(unsigned l=0;l<4;l++)if(!alloc(&m->root01[cpu][l])||!alloc(&m->root02[cpu][l]))return false;
        for(unsigned l=0;l<3;l++){ptr(m->root01[cpu][l])[0]=m->root01[cpu][l+1]|7;ptr(m->root02[cpu][l])[0]=m->root02[cpu][l+1]|7;}
        for(unsigned n=1;n<64;n++){
            ptr(m->root01[cpu][3])[n]=pages[n-1]|0x30|permission(n);
            struct nested_ept_result r=nested_ept_walk(0x1001e,(uint64_t)n*4096,1,resolve,m);
            if(r.status!=NESTED_EPT_OK||!r.leaf_resolved||r.hpa!=pages[n-1]||r.l1_gpa!=(uint64_t)n*4096)return false;
            ptr(m->root02[cpu][3])[n]=r.hpa|0x30|r.effective02;
        }
    }
    for(unsigned n=0;n<512;n++)if(ptr(m->root01[0][3])[n]!=ptr(m->root01[1][3])[n])return false;
    m->ready=true;return true;
}
bool nested_domain_memory_prepare_contexts(struct nested_domain_memory *m) {
    if(!m || !m->ready || m->atomic_ad_prepared || m->typed_uc_prepared || m->contexts_prepared || m->replacement_hpa || m->retained_hpa)
        return false;
    uint64_t *pages[EPT_GUEST_PAGES],*leaves[4];
    /* Validate the complete old shape before any write. */
    for(unsigned n=0;n<EPT_GUEST_PAGES;n++) {
        if(!(pages[n]=ptr(m->backing.pages[n])))return false;
    }
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned kind=0;kind<2;kind++) {
        uint64_t *roots=kind?m->root02[cpu]:m->root01[cpu];
        for(unsigned l=0;l<4;l++) {
            uint64_t *table=ptr(roots[l]);
            if(!table)return false;
            for(unsigned n=0;n<512;n++) {
                uint64_t expected=l<3?(n?0:roots[l+1]|7):
                    (n&&n<64?m->backing.pages[n-1]|0x30|permission(n):0);
                if(table[n]!=expected)return false;
            }
            if(l==3)leaves[cpu*2+kind]=table;
        }
    }
    for(unsigned l=0;l<4;l++)for(unsigned n=0;n<512;n++) {
        uint64_t expected=l<3?(n?0:((uint64_t)(17+l)*4096)|7):
            (n&&n<64?((uint64_t)n*4096)|0x30|permission(n):0);
        if(pages[15+l][n]!=expected)return false;
    }
    for(unsigned l=0;l<4;l++)for(unsigned n=0;n<512;n++)pages[19+l][n]=0;
    for(unsigned l=0;l<3;l++)pages[19+l][0]=((uint64_t)(21+l)*4096)|7;
    for(unsigned n=1;n<64;n++) {
        unsigned permissions=n>=16&&n<=23?1:permission(n);
        pages[18][n]=((uint64_t)n*4096)|0x30|permissions;
        pages[22][n]=((uint64_t)(n==15?11:n)*4096)|0x30|permissions;
    }
    for(unsigned i=0;i<4;i++)for(unsigned n=20;n<=23;n++)
        leaves[i][n]=m->backing.pages[n-1]|0x31;
    pages[10][0]=UINT64_C(0x7788778877887788);
    m->contexts_prepared=true;
    return true;
}
bool nested_domain_memory_prepare_eviction(struct nested_domain_memory *m) {
    if(!m || !m->ready || !m->contexts_prepared || m->eviction_prepared || m->typed_uc_prepared ||
       m->replacement_hpa || m->retained_hpa)return false;
    uint64_t *pages[EPT_GUEST_PAGES],*leaves[4];
    for(unsigned n=0;n<EPT_GUEST_PAGES;n++)
        if(!(pages[n]=ptr(m->backing.pages[n])))return false;
    /* Validate the exact C1 fixture, including both EPT12 roots, before writes. */
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned kind=0;kind<2;kind++) {
        uint64_t *roots=kind?m->root02[cpu]:m->root01[cpu];
        for(unsigned l=0;l<4;l++) {
            uint64_t *table=ptr(roots[l]);
            if(!table)return false;
            for(unsigned n=0;n<512;n++) {
                unsigned permissions=n>=16&&n<=23?1:permission(n);
                uint64_t expected=l<3?(n?0:roots[l+1]|7):
                    (n&&n<64?m->backing.pages[n-1]|0x30|permissions:0);
                if(table[n]!=expected)return false;
            }
            if(l==3)leaves[cpu*2+kind]=table;
        }
    }
    for(unsigned root=0;root<2;root++)for(unsigned l=0;l<4;l++)
        for(unsigned n=0;n<512;n++) {
            unsigned permissions=n>=16&&n<=23?1:permission(n);
            unsigned mapped=root&&n==15?11:n;
            uint64_t expected=l<3?(n?0:((uint64_t)(17+root*4+l)*4096)|7):
                (n&&n<64?((uint64_t)mapped*4096)|0x30|permissions:0);
            if(pages[15+root*4+l][n]!=expected)return false;
        }
    for(unsigned l=0;l<4;l++)for(unsigned n=0;n<512;n++)pages[23+l][n]=0;
    for(unsigned l=0;l<3;l++)pages[23+l][0]=((uint64_t)(25+l)*4096)|7;
    for(unsigned n=1;n<64;n++) {
        unsigned permissions=n>=16&&n<=27?1:permission(n);
        pages[26][n]=((uint64_t)(n==15?28:n)*4096)|0x30|permissions;
    }
    for(unsigned n=24;n<=27;n++) {
        pages[18][n]=pages[22][n]=((uint64_t)n*4096)|0x31;
        for(unsigned i=0;i<4;i++)leaves[i][n]=m->backing.pages[n-1]|0x31;
    }
    pages[27][0]=UINT64_C(0x99aa99aa99aa99aa);
    m->eviction_prepared=true;
    return true;
}
bool nested_domain_memory_view(struct nested_domain_memory *m,unsigned cpu,struct ept_fixture *f){
    if(!m||!m->ready||cpu>1||!f)return false;
    *f=m->backing;for(unsigned i=0;i<4;i++)f->tables[i]=m->root01[cpu][i];f->eptp=f->tables[0]|0x1e;return true;
}
/* Full shape validation is intentional: there must be no hidden alias or
 * alternate upper-level path to the retired data page. No hardware AD bits. */
static bool unique_replacement(struct nested_domain_memory *m,uint64_t replacement) {
    if(!replacement || (replacement&4095) || replacement>=UINT64_C(0x100000000) ||
       !vmx_memory_region_wb(replacement) || !ptr(replacement))return false;
    for(unsigned i=0;i<EPT_GUEST_PAGES;i++)
        if(m->backing.pages[i]==replacement)return false;
    for(unsigned l=0;l<4;l++) {
        if(m->backing.tables[l]==replacement)return false;
        for(unsigned cpu=0;cpu<2;cpu++)
            if(m->root01[cpu][l]==replacement || m->root02[cpu][l]==replacement)return false;
    }
    return replacement!=m->retained_hpa;
}
bool nested_domain_memory_prepare_replacement(struct nested_domain_memory *m) {
    if(!m || !m->ready || m->atomic_ad_prepared || m->typed_uc_prepared || m->contexts_prepared || m->replacement_hpa || m->retained_hpa)return false;
    uint64_t replacement=0;
    if(!alloc(&replacement)) {
        if(replacement && !pmm_free_page(replacement))panic("DOMAIN REPLACEMENT FREE FAILED");
        return false;
    }
    if(!unique_replacement(m,replacement))panic("DOMAIN ALLOCATOR ALIAS RETAIN");
    m->replacement_hpa=replacement;
    return true;
}
bool nested_domain_memory_remap_data(struct nested_domain_memory *m,uint64_t gpa,bool gate_closed) {
    if(!m || !m->ready || m->atomic_ad_prepared || m->typed_uc_prepared || m->contexts_prepared || !gate_closed || gpa!=0xf000 || m->retained_hpa ||
       !unique_replacement(m,m->replacement_hpa))return false;
    const uint64_t old=m->backing.pages[14];
    if(!old || (old&4095) || !vmx_memory_region_wb(old))return false;
    for(unsigned n=0;n<EPT_GUEST_PAGES;n++)
        if(n!=14 && m->backing.pages[n]==old)return false;
    uint64_t *leaves[4];
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned kind=0;kind<2;kind++) {
        uint64_t *roots=kind?m->root02[cpu]:m->root01[cpu];
        for(unsigned l=0;l<4;l++) {
            if(roots[l]==old)return false;
            uint64_t *table=ptr(roots[l]);
            if(!table)return false;
            for(unsigned n=0;n<512;n++) {
                uint64_t expected=l<3?(n?0:roots[l+1]|7):
                    (n && n<64?m->backing.pages[n-1]|0x30|permission(n):0);
                if(table[n]!=expected)return false;
            }
            if(l==3)leaves[cpu*2+kind]=table;
        }
    }
    for(unsigned l=0;l<4;l++)if(m->backing.tables[l]==old)return false;
    /* All rejection points precede the first store. Caller owns publication. */
    for(unsigned i=0;i<4;i++)leaves[i][15]=m->replacement_hpa|0x33;
    m->backing.pages[14]=m->replacement_hpa;
    m->retained_hpa=old;
    m->replacement_hpa=0;
    return true;
}
void nested_domain_memory_release(struct nested_domain_memory *m){
    if(m&&m->atomic_catalog_retained)panic("ATOMIC CATALOG BACKING RETAINED");
    if(!m||!m->ready)panic("DOMAIN RELEASE UNPREPARED RETAIN");
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned i=0;i<4;i++)
        if(!pmm_free_page(m->root01[cpu][i])||!pmm_free_page(m->root02[cpu][i]))panic("DOMAIN ROOT FREE FAILED");
    ept_fixture_owned_release(&m->backing);
    if(m->replacement_hpa && !pmm_free_page(m->replacement_hpa))panic("DOMAIN REPLACEMENT FREE FAILED");
    if(m->retained_hpa && !pmm_free_page(m->retained_hpa))panic("DOMAIN RETIRED FREE FAILED");
    m->replacement_hpa=m->retained_hpa=0;
    m->ready=false;m->contexts_prepared=false;m->eviction_prepared=false;m->atomic_ad_prepared=false;m->typed_uc_prepared=false;m->typed_contexts_prepared=false;m->typed_dynamic_prepared=false;
}

/* Validate every allocated backing/table page, not just the target. In
 * particular a spare guest-RW page must not alias an immutable EPT12 table. */
static bool private_backing(const struct nested_domain_memory *m) {
    uint64_t pages[EPT_GUEST_PAGES+20];
    unsigned count=0;
    for(unsigned n=0;n<EPT_GUEST_PAGES;n++)pages[count++]=m->backing.pages[n];
    for(unsigned l=0;l<4;l++)pages[count++]=m->backing.tables[l];
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned l=0;l<4;l++) {
        pages[count++]=m->root01[cpu][l];pages[count++]=m->root02[cpu][l];
    }
    for(unsigned i=0;i<count;i++) {
        uint64_t h=pages[i];
        if(!h||(h&4095)||h>=UINT64_C(0x100000000)||!vmx_memory_region_wb(h)||!ptr(h))return false;
        for(unsigned j=0;j<i;j++)if(h==pages[j])return false;
    }
    return m->backing.eptp==(m->backing.tables[0]|0x1e);
}

static bool base_shape(struct nested_domain_memory *m) {
    if(!m||!m->ready||m->atomic_ad_prepared||m->typed_uc_prepared||m->contexts_prepared||m->eviction_prepared||
       m->typed_contexts_prepared||m->typed_dynamic_prepared||m->replacement_hpa||m->retained_hpa||!private_backing(m))return false;
    /* All private backing must be distinct, otherwise an apparently ordinary
     * RW guest page could alias the immutable EPT12 table HPA. */
    for(unsigned i=0;i<EPT_GUEST_PAGES;i++) {
        uint64_t h=m->backing.pages[i];
        if(!h||(h&4095)||h>=UINT64_C(0x100000000)||!vmx_memory_region_wb(h)||!ptr(h))return false;
        for(unsigned j=0;j<i;j++)if(h==m->backing.pages[j])return false;
        for(unsigned l=0;l<4;l++) {
            if(h==m->backing.tables[l])return false;
            for(unsigned cpu=0;cpu<2;cpu++)if(h==m->root01[cpu][l]||h==m->root02[cpu][l])return false;
        }
    }
    /* Exact full paths forbid alternate writable/huge aliases in either
     * physical execution map. Guest paging pages on BOTH owners remain RW. */
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned kind=0;kind<2;kind++) {
        uint64_t *roots=kind?m->root02[cpu]:m->root01[cpu];
        for(unsigned l=0;l<4;l++) {
            if(!roots[l]||(roots[l]&4095)||roots[l]>=UINT64_C(0x100000000))return false;
            const uint64_t *table=ptr(roots[l]);if(!table)return false;
            for(unsigned n=0;n<512;n++) {
                uint64_t expected=l<3?(n?0:roots[l+1]|7):
                    (n&&n<64?m->backing.pages[n-1]|0x30|permission(n):0);
                if(table[n]!=expected)return false;
            }
        }
    }
    /* Ordinary loads are permitted here ONLY before either owner publishes
     * this fixture. Initial entries already have A/D clear; no reset writes. */
    for(unsigned l=0;l<4;l++) {
        const uint64_t *table=ptr(m->backing.pages[15+l]);
        for(unsigned n=0;n<512;n++) {
            uint64_t expected=l<3?(n?0:((uint64_t)(17+l)*4096)|7):
                (n&&n<64?((uint64_t)n*4096)|0x30|permission(n):0);
            if(table[n]!=expected)return false;
        }
    }
    return true;
}

bool nested_domain_memory_prepare_atomic_ad(struct nested_domain_memory *m) {
    if(!base_shape(m))return false;
    m->atomic_ad_prepared=true;return true;
}
bool nested_domain_memory_prepare_typed_uc(struct nested_domain_memory *m) {
    if(!base_shape(m))return false;
    /* The retained legacy fixture is not executed, but remains in inventory.
     * Its original nested-test layout has RW data and RX code page6. */
    uint64_t *legacy[4];
    for(unsigned l=0;l<4;l++) {
        uint64_t h=m->backing.tables[l];
        if(!h||(h&4095)||h>=UINT64_C(0x100000000)||!vmx_memory_region_wb(h)||!(legacy[l]=ptr(h)))return false;
        for(unsigned j=0;j<l;j++)if(h==m->backing.tables[j])return false;
        for(unsigned cpu=0;cpu<2;cpu++)for(unsigned j=0;j<4;j++)
            if(h==m->root01[cpu][j]||h==m->root02[cpu][j])return false;
        for(unsigned n=0;n<512;n++) {
            uint64_t expected=l<3?(n?0:m->backing.tables[l+1]|7):
                (n&&n<64?m->backing.pages[n-1]|0x30|(n==6?5:3):0);
            if(legacy[l][n]!=expected)return false;
        }
    }
    if(m->backing.eptp!=(m->backing.tables[0]|0x1e))return false;
    /* Exact fixed layout dedicates page15 to data: descriptors5/37, code6,
     * stacks7..9/39..41, mailbox10, paging1..4/33..36, EPT12 16..19.
     * Full unique backing and every leaf validated before first store. */
    for(unsigned cpu=0;cpu<2;cpu++) {
        for(unsigned n=1;n<64;n++)ptr(m->root01[cpu][3])[n]|=0x40;
        for(unsigned n=1;n<64;n++)ptr(m->root02[cpu][3])[n]|=0x40;
    }
    for(unsigned n=1;n<64;n++)legacy[3][n]|=0x40;
    for(unsigned n=1;n<64;n++)ptr(m->backing.pages[18])[n]|=0x40;
    m->typed_uc_prepared=true;return true;
}

bool nested_domain_memory_prepare_typed_contexts(struct nested_domain_memory *m) {
    /* prepare_typed_uc validates all backing and fixed roots before writing.
     * No operation below may fail: all referenced views were proven private
     * and mapped during that validation, before either owner starts. */
    if(!nested_domain_memory_prepare_typed_uc(m))return false;
    for(unsigned l=0;l<4;l++)for(unsigned n=0;n<512;n++)ptr(m->backing.pages[19+l])[n]=0;
    for(unsigned l=0;l<3;l++)ptr(m->backing.pages[19+l])[0]=((uint64_t)(21+l)*4096)|7;
    for(unsigned n=1;n<64;n++) {
        unsigned permissions=n>=16&&n<=23?1:permission(n);
        ptr(m->backing.pages[18])[n]=((uint64_t)n*4096)|0x70|permissions;
        ptr(m->backing.pages[22])[n]=((uint64_t)(n==48?49:n)*4096)|0x70|permissions;
    }
    for(unsigned n=16;n<=23;n++) {
        for(unsigned cpu=0;cpu<2;cpu++) {
            ptr(m->root01[cpu][3])[n]=m->backing.pages[n-1]|0x71;
            ptr(m->root02[cpu][3])[n]=m->backing.pages[n-1]|0x71;
        }
        ptr(m->backing.tables[3])[n]=m->backing.pages[n-1]|0x71;
    }
    ptr(m->backing.pages[47])[0]=UINT64_C(0x38ba38ba38ba38ba);
    ptr(m->backing.pages[48])[0]=UINT64_C(0x38bb38bb38bb38bb);
    m->contexts_prepared=true;m->typed_contexts_prepared=true;
    return true;
}

bool nested_domain_memory_prepare_typed_dynamic(struct nested_domain_memory *m) {
    /* This validates the entire original inventory before its first store.
     * Everything below is mapped private memory and cannot fail; there is no
     * partially published two-root fixture on a rejected dynamic prepare. */
    if(!nested_domain_memory_prepare_typed_contexts(m))return false;
    for(unsigned l=0;l<4;l++)for(unsigned n=0;n<512;n++)ptr(m->backing.pages[23+l])[n]=0;
    for(unsigned l=0;l<3;l++)ptr(m->backing.pages[23+l])[0]=((uint64_t)(25+l)*4096)|7;
    for(unsigned root=0;root<3;root++)for(unsigned n=1;n<64;n++) {
        unsigned permissions=n>=16&&n<=27?1:permission(n);
        unsigned mapped=n==48?48+root:n;
        ptr(m->backing.pages[18+4*root])[n]=((uint64_t)mapped*4096)|0x70|permissions;
    }
    for(unsigned n=24;n<=27;n++) {
        for(unsigned cpu=0;cpu<2;cpu++) {
            ptr(m->root01[cpu][3])[n]=m->backing.pages[n-1]|0x71;
            ptr(m->root02[cpu][3])[n]=m->backing.pages[n-1]|0x71;
        }
        ptr(m->backing.tables[3])[n]=m->backing.pages[n-1]|0x71;
    }
    ptr(m->backing.pages[47])[0]=UINT64_C(0x38ca38ca38ca38ca);
    ptr(m->backing.pages[48])[0]=UINT64_C(0x38cb38cb38cb38cb);
    ptr(m->backing.pages[49])[0]=UINT64_C(0x38cc38cc38cc38cc);
    m->eviction_prepared=true;m->typed_dynamic_prepared=true;
    return true;
}

/* Includes the final orchestration writes, not merely the earlier prepare. */
bool nested_domain_memory_typed_ready(const struct nested_domain_memory *m) {
    if(!m||!m->ready||!m->typed_uc_prepared||m->atomic_ad_prepared||
       (m->contexts_prepared!=m->typed_contexts_prepared)||
       (m->eviction_prepared!=m->typed_dynamic_prepared)||
       (m->typed_dynamic_prepared&&!m->typed_contexts_prepared)||
       m->replacement_hpa||m->retained_hpa||!private_backing(m))return false;
    unsigned last_table=m->typed_dynamic_prepared?27:23;
    for(unsigned r=0;r<5;r++){
        const uint64_t *tables=r<2?m->root01[r]:r<4?m->root02[r-2]:m->backing.tables;
        for(unsigned l=0;l<4;l++){
            const uint64_t *v=ptr(tables[l]);if(!v)return false;
            for(unsigned n=0;n<512;n++){
                uint64_t wanted=l<3?(n?0:tables[l+1]|7):
                    (n&&n<64?m->backing.pages[n-1]|0x70|(m->typed_contexts_prepared&&n>=16&&n<=last_table?1:(r==4?(n==6?5:3):permission(n))):0);
                if(v[n]!=wanted)return false;
            }
        }
    }
    for(unsigned root=0;root<(m->typed_dynamic_prepared?3u:m->typed_contexts_prepared?2u:1u);root++)for(unsigned l=0;l<4;l++){
        const uint64_t *v=ptr(m->backing.pages[15+4*root+l]);if(!v)return false;
        for(unsigned n=0;n<512;n++){
            unsigned mapped=n==48?48+root:n;
            unsigned permissions=m->typed_contexts_prepared&&n>=16&&n<=last_table?1:permission(n);
            uint64_t wanted=l<3?(n?0:((uint64_t)(17+4*root+l)*4096)|7):
                (n&&n<64?((uint64_t)mapped*4096)|0x70|permissions:0);
            if(v[n]!=wanted)return false;
        }
    }
    if(m->typed_dynamic_prepared) {
        const uint64_t witness[]={UINT64_C(0x38ca38ca38ca38ca),
            UINT64_C(0x38cb38cb38cb38cb),UINT64_C(0x38cc38cc38cc38cc)};
        for(unsigned root=0;root<3;root++)if(ptr(m->backing.pages[47+root])[0]!=witness[root])return false;
    }
    return true;
}
