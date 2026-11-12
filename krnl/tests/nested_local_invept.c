#include "../nested_local_invept.h"
#include <assert.h>
#include <stdio.h>
#define OK NESTED_LOCAL_INVEPT_OK
#define INVALID NESTED_LOCAL_INVEPT_INVALID28
#define POLICY NESTED_LOCAL_INVEPT_UNSUPPORTED
int main(void)
{
    for(unsigned width=12;width<=48;width++) {
        uint64_t last=((UINT64_C(1)<<width)-1)&~UINT64_C(4095);
        assert(nested_local_invept_check(1,0x1e,0,width)==OK);
        assert(nested_local_invept_check(1,last|0x1e,0,width)==OK);
        assert(nested_local_invept_check(1,(UINT64_C(1)<<width)|0x1e,0,width)==INVALID);
    }
    const uint64_t arbitrary[]={0x1001e,0x1101e,0x2345601e,0xfffffffff01e};
    for(unsigned i=0;i<sizeof arbitrary/sizeof arbitrary[0];i++)
        assert(nested_local_invept_check(1,arbitrary[i],0,48)==OK);
    const uint64_t types[]={0,2,3,UINT64_MAX,UINT64_C(0x100000001)};
    for(unsigned i=0;i<sizeof types/sizeof types[0];i++)
        assert(nested_local_invept_check(types[i],0x1001e,0,48)==INVALID);
    assert(nested_local_invept_check(1,0x1001e,0,11)==POLICY);
    assert(nested_local_invept_check(1,0x1001e,0,49)==POLICY);
    assert(nested_local_invept_check(1,0x1001e,0,64)==POLICY);
    for(unsigned bit=7;bit<12;bit++)
        assert(nested_local_invept_check(1,0x1001e|(UINT64_C(1)<<bit),0,48)==INVALID);
    for(unsigned bit=48;bit<64;bit++)
        assert(nested_local_invept_check(1,0x1001e|(UINT64_C(1)<<bit),0,48)==INVALID);
    for(unsigned mt=0;mt<8;mt++)for(unsigned walk=0;walk<8;walk++)
        for(unsigned ad=0;ad<2;ad++) {
            uint64_t eptp=0x10000|mt|(walk<<3)|(ad<<6);
            enum nested_local_invept_status want=
                (mt!=0 && mt!=6)||(walk!=3 && walk!=4)?INVALID:
                mt!=6||walk!=3||ad?INVALID:OK;
            assert(nested_local_invept_check(1,eptp,0,48)==want);
        }
    for(unsigned bit=0;bit<64;bit++)
        assert(nested_local_invept_check(1,0x1001e,UINT64_C(1)<<bit,48)==POLICY);
    const uint64_t lows[]={0,0x1e,UINT64_MAX,UINT64_C(0x8000000000000000),0xdeadbeef};
    for(unsigned width=12;width<=48;width++)for(unsigned i=0;i<sizeof lows/sizeof lows[0];i++){
        assert(nested_local_invept_check_allcontexts(2,lows[i],0,width)==OK);
        assert(nested_local_invept_check_allcontexts(1,lows[i],0,width)==
               nested_local_invept_check(1,lows[i],0,width));
    }
    assert(nested_local_invept_check_allcontexts(1,0x1e,0,48)==OK);
    const uint64_t badtypes[]={0,3,UINT64_MAX,UINT64_C(0x100000001),UINT64_C(0x100000002)};
    for(unsigned i=0;i<sizeof badtypes/sizeof badtypes[0];i++)
        assert(nested_local_invept_check_allcontexts(badtypes[i],0,0,48)==INVALID);
    for(unsigned bit=0;bit<64;bit++)
        assert(nested_local_invept_check_allcontexts(2,UINT64_MAX,UINT64_C(1)<<bit,48)==POLICY);
    const unsigned widths[]={0,11,49,64,~0u};
    for(unsigned i=0;i<sizeof widths/sizeof widths[0];i++){
        assert(nested_local_invept_check_allcontexts(2,UINT64_MAX,0,widths[i])==POLICY);
        assert(nested_local_invept_check_allcontexts(1,0x1e,0,widths[i])==POLICY);
    }
    puts("PASS local type1 INVEPT arbitrary root, width, reserved bits and policy distinction");
    return 0;
}
