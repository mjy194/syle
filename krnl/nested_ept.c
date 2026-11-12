#include "nested_ept.h"
#include "nested_ept_ad.h"
#include <stddef.h>

static bool permissions_valid(uint8_t p) {
    return !(p&~7u) && (!(p&2) || (p&1)) && (!(p&4) || (p&1));
}
static bool host_page_valid(uint64_t hpa,uint64_t limit) {
    return hpa<limit && !(hpa&0xfff);
}
static struct nested_ept_result walk_core(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy,bool ad,struct nested_ept_ad_trace *trace,bool prefix,bool ignore_ad0,bool memory,bool classified) {
    struct nested_ept_result r={.status=classified?NESTED_EPT_UNSUPPORTED:NESTED_EPT_MALFORMED_UNSUPPORTED};
    struct nested_ept_ad_trace staged={0};
    if(!policy||policy->maxphys<12||policy->maxphys>48) return r;
    uint64_t limit=UINT64_C(1)<<policy->maxphys;
    staged.maxphys=policy->maxphys;
    uint64_t address_mask=(limit-1)&~UINT64_C(4095);
    /* Memory type WB (6), walk length 4 (encoded 3), explicit A/D mode. */
    if(!resolve || l2_gpa>=limit ||
       (access!=1 && access!=2 && access!=4) ||
       (memory ? ((eptp12&~address_mask)!=0x18 && (eptp12&~address_mask)!=0x1e)
               : (eptp12&~address_mask)!=(ad?0x5e:0x1e))) return r;
    uint64_t table_gpa=eptp12&address_mask;
    r.effective12=7;
    for(unsigned level=4;level;--level) {
        uint64_t table_hpa=0;
        const uint64_t *table=NULL;
        uint8_t table_perm=0;
        if(!resolve(context,table_gpa,1,&table_hpa,&table,&table_perm) ||
           !host_page_valid(table_hpa,limit) || !table ||
           ((uintptr_t)table&0xfff) || !permissions_valid(table_perm) ||
           !(table_perm&1)) {
            r.status=NESTED_EPT_L0_BACKING_DENIED;
            return r;
        }
        unsigned shift=12+9*(level-1);
        uint64_t entry=table[(l2_gpa>>shift)&0x1ff];
        uint8_t perm=(uint8_t)(entry&7);
        r.effective12 &= perm;
        /* Nonpresent ignores all remaining fields, including PS/reserved. */
        if(!perm) {
            if(ad&&prefix) *trace=staged;
            r.status=NESTED_EPT_L1_DENIED;
            return r;
        }
        if(classified) {
            /* Intel SDM Vol3C 326019-084US §29.3.3.1: present
             * W=1/R=0 and reserved physical-address bits are misconfig.
             * Do not conflate unknown/unsupported feature fields with this. */
            uint64_t physical_reserved=UINT64_C(0x000ffffffffff000)&~address_mask;
            if(((perm&2)&&!(perm&1)) || (entry&physical_reserved)) {
                r.status=NESTED_EPT_MISCONFIG;return r;
            }
            /* Table29-7: PT bit7 is ignored (not a large-page flag). */
            if(level==1)entry&=~UINT64_C(0x80);
            bool known_leaf=level==1 || ((level==2||level==3)&&(entry&0x80));
            if(!known_leaf && (entry&UINT64_C(0xf8))) {
                /* Tables29-2/4/6: nonleaf bits7:3 reserved zero;
                 * at PDPTE/PDE bit7=1 instead selects a leaf above. */
                r.status=NESTED_EPT_MISCONFIG;return r;
            }
            if(known_leaf) {
                unsigned mt=(entry>>3)&7;
                uint64_t alignment=(UINT64_C(1)<<shift)-1;
                if(mt==2 || mt==3 || mt==7 || ((entry&address_mask)&alignment)) {
                    r.status=NESTED_EPT_MISCONFIG;return r;
                }
            }
        }
        bool large=(entry&0x80)!=0;
        if(large && !((level==2 && policy->allow_2m) ||
                      (level==3 && policy->allow_1g))) return r;
        bool leaf=level==1||large;
        uint64_t allowed=address_mask|7;
        if(leaf) allowed|=memory?0x78:0x38; /* explicit mode additionally allows IPAT */
        if(large) allowed|=0x80;
        if(ad) allowed|=leaf?0x300:0x100;
        else if(ignore_ad0) allowed|=0x300;
        uint8_t mt=(entry>>3)&7;
        bool type_ok=memory?(mt==0||mt==1||mt==4||mt==5||mt==6):mt==6;
        if(!permissions_valid(perm) || (entry&~allowed) || (leaf&&!type_ok)) return r;
        if(ad) staged.entries[staged.count++]=(struct nested_ept_ad_entry){
            .entry_gpa=table_gpa+(((l2_gpa>>shift)&0x1ff)*8),
            .raw=entry,.table_hpa=table_hpa,.leaf=leaf};
        uint64_t base=entry&address_mask;
        if(!leaf) {
            table_gpa=base;
            continue;
        }
        /* 4K/PT, 2M/PD or 1G/PDPT: lower address bits must be reserved zero. */
        uint64_t offset_mask=(UINT64_C(1)<<shift)-1;
        if(base&offset_mask) return r;
        uint64_t l1_gpa=base+(l2_gpa&offset_mask);
        if(l1_gpa>=limit) return r;
        r.leaf_resolved=true;
        if(memory) {
            r.memory_explicit=true;r.leaf_type=mt;
            r.ignore_pat=(entry&0x40)!=0;r.walk_type=eptp12&7;
        }
        r.l1_gpa=l1_gpa;
        if(!(r.effective12&access)) {
            if(ad&&prefix) {staged.leaf_level=level;*trace=staged;}
            r.status=NESTED_EPT_L1_DENIED;
            return r;
        }
        uint64_t leaf_hpa=0;
        const uint64_t *unused=NULL;
        uint8_t leaf_perm=0;
        uint64_t page=l1_gpa&~UINT64_C(4095);
        if(!resolve(context,page,access,&leaf_hpa,&unused,&leaf_perm) ||
           !host_page_valid(leaf_hpa,limit) || !permissions_valid(leaf_perm)) {
            r.status=NESTED_EPT_L0_BACKING_DENIED;
            return r;
        }
        r.hpa=leaf_hpa+(l1_gpa&0xfff);
        r.effective02=r.effective12&leaf_perm;
        r.status=(r.effective02&access)?NESTED_EPT_OK:NESTED_EPT_L0_BACKING_DENIED;
        if(ad && r.status==NESTED_EPT_OK) {staged.leaf_level=level;*trace=staged;}
        return r;
    }
    return r;
}
struct nested_ept_result nested_ept_walk_policy(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy) {
    return walk_core(eptp12,l2_gpa,access,resolve,context,policy,false,NULL,false,false,false,false);
}
struct nested_ept_result nested_ept_ad_walk(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy,struct nested_ept_ad_trace *trace) {
    if(!trace) return (struct nested_ept_result){.status=NESTED_EPT_MALFORMED_UNSUPPORTED};
    *trace=(struct nested_ept_ad_trace){0};
    return walk_core(eptp12,l2_gpa,access,resolve,context,policy,true,trace,false,false,false,false);
}
struct nested_ept_result nested_ept_ad_walk_prefix(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy,struct nested_ept_ad_trace *trace) {
    if(!trace) return (struct nested_ept_result){.status=NESTED_EPT_MALFORMED_UNSUPPORTED};
    *trace=(struct nested_ept_ad_trace){0};
    return walk_core(eptp12,l2_gpa,access,resolve,context,policy,true,trace,true,false,false,false);
}
struct nested_ept_result nested_ept_walk_ad0_ignored(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy) {
    return walk_core(eptp12,l2_gpa,access,resolve,context,policy,false,NULL,false,true,false,false);
}
struct nested_ept_result nested_ept_walk(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context) {
    const struct nested_ept_policy strict={48,false,false};
    return nested_ept_walk_policy(eptp12,l2_gpa,access,resolve,context,&strict);
}

struct nested_ept_result nested_ept_walk_memory(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy) {
    return walk_core(eptp12,l2_gpa,access,resolve,context,policy,false,NULL,false,true,true,false);
}

struct nested_ept_result nested_ept_walk_classified(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy) {
    return walk_core(eptp12,l2_gpa,access,resolve,context,policy,false,NULL,false,true,false,true);
}
