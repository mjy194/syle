#include "firmware_dirty_cas.h"
#include <stdint.h>
enum fwd_cas_result fwd_atomic_cas(uint64_t*p,uint64_t expected,uint64_t desired){
 if(!p||((uintptr_t)p&7)||(expected&0x40)||desired!=(expected|UINT64_C(0x40)))return FWD_CAS_CONFLICT;
 return __atomic_compare_exchange_n(p,&expected,desired,false,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST)?FWD_CAS_OK:FWD_CAS_CONFLICT;
}
