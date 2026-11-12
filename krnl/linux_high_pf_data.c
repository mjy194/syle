#include "linux_high_pf_data.h"
bool lhpd_metadata(uint64_t counter_pair,uint64_t base,uint64_t flags,uint64_t efer){
 return counter_pair>>32<=62 && (uint32_t)counter_pair==0 && base==0 &&
        flags==UINT64_C(0x80000000000000e3) && efer==0xd01;
}
