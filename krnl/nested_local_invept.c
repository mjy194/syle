#include "nested_local_invept.h"
enum nested_local_invept_status nested_local_invept_check(
    uint64_t type,uint64_t eptp,uint64_t high,unsigned maxphys)
{
    /* Type is a full 64-bit register operand. Type 2 is not advertised. */
    if(maxphys<12 || maxphys>48)
        return NESTED_LOCAL_INVEPT_UNSUPPORTED; /* invalid monitor configuration */
    if(type!=1)return NESTED_LOCAL_INVEPT_INVALID28;
    uint64_t address_mask=((UINT64_C(1)<<maxphys)-1)&~UINT64_C(0xfff);
    if(eptp&~(address_mask|UINT64_C(0x7f)))
        return NESTED_LOCAL_INVEPT_INVALID28;
    unsigned memory_type=(unsigned)(eptp&7);
    unsigned walk_minus_one=(unsigned)((eptp>>3)&7);
    if((memory_type!=0 && memory_type!=6) ||
       (walk_minus_one!=3 && walk_minus_one!=4))
        return NESTED_LOCAL_INVEPT_INVALID28;
    /* This virtual capability profile advertises only WB, four-level, AD0.
     * UC/five-level/AD1 can be valid on other CPUs but not this virtual CPU. */
    if(memory_type!=6 || walk_minus_one!=3 || (eptp&(1u<<6)))
        return NESTED_LOCAL_INVEPT_INVALID28;
    /* Unimplemented descriptor-high policy is not fabricated VMfail(28). */
    if(high)return NESTED_LOCAL_INVEPT_UNSUPPORTED;
    return NESTED_LOCAL_INVEPT_OK;
}

enum nested_local_invept_status nested_local_invept_check_allcontexts(
    uint64_t type,uint64_t eptp,uint64_t high,unsigned maxphys)
{
    if(maxphys<12||maxphys>48)return NESTED_LOCAL_INVEPT_UNSUPPORTED;
    if(type==1)return nested_local_invept_check(type,eptp,high,maxphys);
    if(type!=2)return NESTED_LOCAL_INVEPT_INVALID28;
    if(high)return NESTED_LOCAL_INVEPT_UNSUPPORTED;
    return NESTED_LOCAL_INVEPT_OK;
}
