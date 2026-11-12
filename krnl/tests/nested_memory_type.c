#include "../nested_memory_type.h"
#include <assert.h>
#include <stdio.h>
static uint64_t repeated(uint8_t value) {
    uint64_t pat=0;for(unsigned i=0;i<8;i++) pat|=(uint64_t)value<<(i*8);return pat;
}
int main(void) {
    assert(nested_uc_pat_cached_resume(1,0));
    assert(!nested_uc_pat_cached_resume(0,0));
    assert(!nested_uc_pat_cached_resume(2,0));
    assert(!nested_uc_pat_cached_resume(1,1));
    assert(!nested_uc_pat_cached_resume(~0u,~0u));

    const uint8_t leaves[]={0,1,4,5,6};
    const uint8_t pats[]={0,1,4,5,6,7};
    /* Independently transcribed SDM rows UC,WC,WT,WP,WB, columns as above. */
    const uint8_t expected[5][6]={
        {0,1,0,0,0,0},{0,1,0,0,1,1},{0,1,4,5,4,0},
        {0,1,4,5,5,1},{0,1,4,5,6,0}
    };
    for(unsigned value=0;value<256;value++) {
        bool leaf=value==0||value==1||value==4||value==5||value==6;
        bool pat=leaf||value==7;
        assert(nested_memory_eptp_valid((uint8_t)value)==(value==0||value==6));
        assert(nested_memory_leaf_valid((uint8_t)value)==leaf);
        assert(nested_memory_pat_type_valid((uint8_t)value)==pat);
        assert(nested_memory_pat_valid(repeated((uint8_t)value))==pat);
        for(unsigned byte=0;byte<8;byte++) {
            uint64_t whole=(UINT64_C(0x0606060606060606)&~(UINT64_C(255)<<(byte*8)))|
                           (uint64_t)value<<(byte*8);
            assert(nested_memory_pat_valid(whole)==pat);
        }
        if(!leaf) for(unsigned ipat=0;ipat<2;ipat++) for(unsigned cd=0;cd<2;cd++) {
            uint8_t out=0xaa;
            assert(!nested_memory_effective((uint8_t)value,6,ipat,cd,&out)&&out==0xaa);
        }
        if(!pat) for(unsigned ipat=0;ipat<2;ipat++) for(unsigned cd=0;cd<2;cd++) {
            uint8_t out=0xaa;
            assert(!nested_memory_effective(6,(uint8_t)value,ipat,cd,&out)&&out==0xaa);
        }
    }
    for(unsigned i=0;i<5;i++) for(unsigned j=0;j<6;j++) {
        uint8_t out=0xaa;
        assert(nested_memory_effective(leaves[i],pats[j],false,false,&out));
        assert(out==expected[i][j]&&out!=7);
        assert(nested_memory_effective(leaves[i],pats[j],true,false,&out)&&out==leaves[i]);
        assert(nested_memory_effective(leaves[i],pats[j],false,true,&out)&&out==0);
        assert(nested_memory_effective(leaves[i],pats[j],true,true,&out)&&out==0);
    }
    assert(!nested_memory_effective(6,6,false,false,NULL));
    /* Easy-to-misimplement asymmetric/non-minimum combinations. */
    uint8_t out;
    assert(nested_memory_effective(0,1,false,false,&out)&&out==1);
    assert(nested_memory_effective(5,7,false,false,&out)&&out==1);
    assert(nested_memory_effective(4,5,false,false,&out)&&out==5);
    assert(nested_memory_effective(5,4,false,false,&out)&&out==4);
    assert(nested_memory_effective(6,7,false,false,&out)&&out==0);

    uint64_t all_wb=repeated(6),normal=UINT64_C(0x0007040600070406);
    assert(nested_memory_pat_valid(normal));
    assert(nested_memory_wb_leaf_safe(6,true,false,normal));
    assert(!nested_memory_wb_leaf_safe(6,false,false,normal));
    assert(nested_memory_wb_leaf_safe(6,false,false,all_wb));
    assert(nested_memory_wb_leaf_safe(6,true,false,all_wb));
    assert(!nested_memory_wb_leaf_safe(6,true,true,all_wb));
    assert(!nested_memory_wb_leaf_safe(6,false,true,all_wb));
    for(unsigned i=0;i<5;i++) if(leaves[i]!=6) {
        assert(!nested_memory_wb_leaf_safe(leaves[i],true,false,all_wb));
        assert(!nested_memory_wb_leaf_safe(leaves[i],false,false,all_wb));
    }
    for(unsigned byte=0;byte<8;byte++) for(unsigned i=0;i<6;i++) {
        uint64_t mixed=(all_wb&~(UINT64_C(255)<<(byte*8)))|((uint64_t)pats[i]<<(byte*8));
        assert(nested_memory_wb_leaf_safe(6,true,false,mixed));
        assert(nested_memory_wb_leaf_safe(6,false,false,mixed)==(pats[i]==6));
    }
    assert(!nested_memory_wb_leaf_safe(6,true,false,all_wb|(UINT64_C(0x80)<<56)));
    assert(!nested_memory_wb_leaf_safe(7,true,false,all_wb));
    /* Generic backing compatibility checks every possible guest PAT slot. */
    assert(nested_memory_backing_compatible(0,true,false,false,normal,0));
    assert(nested_memory_backing_compatible(0,false,false,false,normal,0));
    for(unsigned slot=0;slot<8;slot++) {
        uint64_t wc=(normal&~(UINT64_C(255)<<(slot*8)))|(UINT64_C(1)<<(slot*8));
        assert(!nested_memory_backing_compatible(0,false,false,false,wc,0));
        assert(nested_memory_backing_compatible(0,true,false,false,wc,0));
        uint64_t mixed=(all_wb&~(UINT64_C(255)<<(slot*8)))|(UINT64_C(4)<<(slot*8));
        assert(!nested_memory_backing_compatible(6,false,false,false,mixed,6));
        assert(nested_memory_backing_compatible(6,true,false,false,mixed,6));
        for(unsigned code=0;code<256;code++) if(!nested_memory_pat_type_valid(code)) {
            uint64_t badpat=(normal&~(UINT64_C(255)<<(slot*8)))|((uint64_t)code<<(slot*8));
            assert(!nested_memory_backing_compatible(0,true,false,false,badpat,0));
            assert(!nested_memory_backing_compatible(6,true,false,false,badpat,6));
        }
    }
    /* Every table combination and every IPAT combination, all five backing
     * types: compatibility is exact equality, never a minimum-type guess. */
    for(unsigned li=0;li<5;li++)for(unsigned pi=0;pi<6;pi++)for(unsigned bi=0;bi<5;bi++) {
        uint8_t effective=255;
        assert(nested_memory_effective(leaves[li],pats[pi],false,false,&effective));
        assert(nested_memory_backing_compatible(leaves[li],false,false,false,repeated(pats[pi]),leaves[bi])==(effective==leaves[bi]));
        assert(nested_memory_backing_compatible(leaves[li],true,false,false,repeated(pats[pi]),leaves[bi])==(leaves[li]==leaves[bi]));
    }
    assert(nested_memory_backing_compatible(6,false,false,false,all_wb,6));
    assert(nested_memory_backing_compatible(6,false,false,false,repeated(7),0)); /* WB x UC- */
    assert(nested_memory_backing_compatible(4,false,false,false,repeated(7),0)); /* WT x UC- */
    assert(nested_memory_backing_compatible(4,false,false,false,repeated(0),0)); /* WT x UC */
    assert(nested_memory_backing_compatible(5,false,false,false,repeated(7),1)); /* WP x UC- is WC */
    for(unsigned cd=0;cd<2;cd++)for(unsigned nw=0;nw<2;nw++)if(cd||nw) {
        assert(!nested_memory_backing_compatible(0,true,cd,nw,normal,0));
        assert(!nested_memory_backing_compatible(6,false,cd,nw,all_wb,6));
    }
    for(unsigned code=0;code<256;code++)if(!nested_memory_leaf_valid(code)) {
        assert(!nested_memory_backing_compatible(code,true,false,false,normal,0));
        assert(!nested_memory_backing_compatible(0,true,false,false,normal,code));
    }
    puts("PASS Intel EPT/PAT memory-type matrix, encoding legality and conservative WB admission");
}
