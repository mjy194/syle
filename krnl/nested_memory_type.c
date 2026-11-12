#include "nested_memory_type.h"
#include <stddef.h>
/* Evidence checked against Intel SDM, not a guessed "most restrictive" rule:
 * Vol.3A 11.5.2.2 Table 11-7 and continuation (PDF pp.428-429):
 * https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf
 * Vol.3C 326019-084US 29.3.7.2 (PDF p.145) refers to newer 3A Table 12-7,
 * substitutes EPT type for MTRR type, and specifies CD/IPAT overrides:
 * https://cdrdv2-public.intel.com/825750/326019-sdm-vol-3c.pdf
 * The older table's footnote 3 explicitly defines combinations that earlier
 * editions called undefined. In particular UC x WC = WC and WP x UC- = WC.
 */
bool nested_memory_eptp_valid(uint8_t t) {return t==0||t==6;}
bool nested_memory_leaf_valid(uint8_t t) {return t==0||t==1||t==4||t==5||t==6;}
bool nested_memory_pat_type_valid(uint8_t t) {return nested_memory_leaf_valid(t)||t==7;}
bool nested_memory_pat_valid(uint64_t pat) {
    for(unsigned i=0;i<8;i++)
        if(!nested_memory_pat_type_valid((uint8_t)(pat>>(i*8)))) return false;
    return true;
}
static unsigned index_of(uint8_t t) {
    /* Caller has validated encoding; indices order UC,WC,WT,WP,WB,UC-. */
    return t<2?t:(unsigned)t-2;
}
bool nested_memory_effective(uint8_t leaf,uint8_t pat,bool ipat,bool cd,uint8_t *out) {
    if(!out||!nested_memory_leaf_valid(leaf)||!nested_memory_pat_type_valid(pat)) return false;
    static const uint8_t matrix[5][6]={
        /* PAT: UC WC WT WP WB UC- */
        {0,1,0,0,0,0}, /* EPT UC */
        {0,1,0,0,1,1}, /* EPT WC */
        {0,1,4,5,4,0}, /* EPT WT */
        {0,1,4,5,5,1}, /* EPT WP */
        {0,1,4,5,6,0}  /* EPT WB */
    };
    *out=cd?NESTED_MEM_UC:ipat?leaf:matrix[index_of(leaf)][index_of(pat)];
    return true;
}
bool nested_memory_wb_leaf_safe(uint8_t leaf,bool ipat,bool cd,uint64_t pat) {
    if(leaf!=NESTED_MEM_WB||cd||!nested_memory_pat_valid(pat)) return false;
    return ipat||pat==UINT64_C(0x0606060606060606);
}

bool nested_memory_backing_compatible(uint8_t leaf,bool ipat,bool cd,bool nw,
    uint64_t pat,uint8_t backing) {
    if(cd||nw||!nested_memory_leaf_valid(leaf)||!nested_memory_leaf_valid(backing)||
       !nested_memory_pat_valid(pat))return false;
    for(unsigned slot=0;slot<8;slot++) {
        uint8_t effective;
        if(!nested_memory_effective(leaf,(uint8_t)(pat>>(slot*8)),ipat,false,&effective)||
           effective!=backing)return false;
    }
    return true;
}
