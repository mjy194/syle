#include "nested_ept_ad.h"
#include <stddef.h>
static bool perms(uint8_t p) {
    return !(p&~7u)&&(!(p&2)||(p&1))&&(!(p&4)||(p&1));
}
static bool update_core(const struct nested_ept_ad_trace *t,bool dirty,
    nested_ept_mutable_resolve_fn resolve,void *context,bool prefix) {
    if(!t||!resolve||t->maxphys<12||t->maxphys>48||t->count>4) return false;
    bool full=t->leaf_level!=0;
    if(full) {
        if(t->count<2||t->leaf_level!=5-t->count) return false;
    } else if(!prefix||t->count>3) return false;
    uint64_t limit=UINT64_C(1)<<t->maxphys,address_mask=(limit-1)&~UINT64_C(4095);
    uint64_t *targets[4],set_bits[4];
    for(unsigned i=0;i<t->count;i++) {
        const struct nested_ept_ad_entry *e=&t->entries[i];
        bool leaf=full&&i+1==t->count;
        uint8_t p12=e->raw&7;
        if(e->leaf!=leaf||!p12||!perms(p12)||(dirty&&!(p12&2))||
           e->entry_gpa>=limit||(e->entry_gpa&7)||
           e->table_hpa>=limit||(e->table_hpa&4095)) return false;
        uint64_t allowed=address_mask|7|0x100;
        uint64_t ad_mask=leaf?0x300:0x100;
        if(leaf) allowed|=0x238;
        if(leaf&&t->leaf_level>1) allowed|=0x80;
        if((e->raw&~allowed)||(leaf&&((e->raw>>3)&7)!=6)||
           (leaf&&((e->raw&0x80)!=0)!=(t->leaf_level>1))) return false;
        if(leaf) {
            unsigned shift=12+9*(t->leaf_level-1);
            if((e->raw&address_mask)&((UINT64_C(1)<<shift)-1)) return false;
        } else if(i+1<t->count && (e->raw&address_mask)!=(t->entries[i+1].entry_gpa&~UINT64_C(4095))) return false;
        uint64_t hpa=0,*page=NULL;uint8_t p01=0;
        if(!resolve(context,e->entry_gpa&~UINT64_C(4095),3,&hpa,&page,&p01)||
           !page||((uintptr_t)page&4095)||hpa!=e->table_hpa||
           !perms(p01)||(p01&3)!=3) return false;
        uint64_t *entry=&page[(e->entry_gpa&4095)/8];
        if((*entry&~ad_mask)!=(e->raw&~ad_mask)) return false;
        targets[i]=entry;
        /* Failed-walk prefix mode is strictly A-only: current D survives OR,
         * but even a previously observed D is not newly restored here. */
        set_bits[i]=prefix?0x100:0x100|(e->raw&ad_mask)|((leaf&&dirty)?0x200:0);
    }
    /* Every target passed ownership, permission and identity checks. OR,
     * rather than assign, also preserves any alias target's earlier OR. */
    for(unsigned i=0;i<t->count;i++) *targets[i]|=set_bits[i];
    return true;
}

bool nested_ept_ad_update(const struct nested_ept_ad_trace *trace,bool dirty,
    nested_ept_mutable_resolve_fn resolve,void *context) {
    return update_core(trace,dirty,resolve,context,false);
}
bool nested_ept_ad_update_prefix(const struct nested_ept_ad_trace *trace,
    nested_ept_mutable_resolve_fn resolve,void *context) {
    return update_core(trace,false,resolve,context,true);
}
