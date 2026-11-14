#include "firmware_irq_native_evidence.h"
enum fir_result fine_classify(const struct fir_identity*i,const struct fine_outcome*o){
 if(!i||!o||!o->status_sentinel_set)return FIR_UNKNOWN;
 const struct fir_evidence*e=&o->evidence;
 if(!i->domain||!i->vmcs||!i->attempt||i->domain!=e->observed.domain||i->vmcs!=e->observed.vmcs||
 i->attempt!=e->observed.attempt||e->staged_info!=UINT32_C(0x80000020)||
 !e->host_restored||!e->profile_admitted)return FIR_UNKNOWN;
 if(e->native_status==0||e->native_status==2){
  /* Definite native no-entry and restored host, not a delivery receipt. */
  return o->run_result==LVS_ENTRY_FAILED&&!o->exit_fields_read&&!e->run_reported_exit&&
    !e->reason&&!e->vectoring&&!e->exit_interruption?FIR_NO_ENTRY:FIR_UNKNOWN;
 }
 if(e->native_status!=1||o->run_result!=LVS_EXIT||!o->exit_fields_read)return FIR_UNKNOWN;
 return fir_classify(i,e);
}
