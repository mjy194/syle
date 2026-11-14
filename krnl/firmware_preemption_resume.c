#include "firmware_preemption_resume.h"
bool fpr_resume(uint64_t reason,uint64_t cs,uint64_t flags,uint64_t blocking,uint64_t pending_debug,uint64_t dr7,uint64_t entry_event,uint64_t vectoring){
 return (flags&2) && !(flags&~UINT64_C(0x3d7fd7)) && reason==52 && !(cs&3) && !(flags&(UINT64_C(1)<<8)) && !blocking && !pending_debug && dr7==0x400 && !entry_event && !vectoring;
}
