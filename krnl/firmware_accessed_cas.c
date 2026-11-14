#include "firmware_accessed_cas.h"
enum fwd_cas_result faa_atomic_cas(uint64_t*p,uint64_t expected,uint64_t desired){
 if(!p||((uintptr_t)p&7)||(expected&0x20)||desired!=(expected|UINT64_C(0x20)))return FWD_CAS_CONFLICT;
 return __atomic_compare_exchange_n(p,&expected,desired,false,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST)?FWD_CAS_OK:FWD_CAS_CONFLICT;
}
