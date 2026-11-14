#include "firmware_irq_receipt.h"
enum fir_result fir_classify(const struct fir_identity*i,const struct fir_evidence*e){
 if(!i||!e||!i->domain||!i->vmcs||!i->attempt||i->domain!=e->observed.domain||
 i->vmcs!=e->observed.vmcs||i->attempt!=e->observed.attempt||
 e->staged_info!=UINT32_C(0x80000020)||e->native_status!=1||!e->run_reported_exit||
 !e->host_restored||!e->profile_admitted||(e->vectoring&UINT32_C(0x80000000))||
 (e->exit_interruption&UINT32_C(0x80000000)))return FIR_UNKNOWN;
 if(e->reason==UINT32_C(0x80000021)||e->reason==UINT32_C(0x80000022))return FIR_NO_ENTRY;
 if(e->reason&~UINT32_C(0xffff))return FIR_UNKNOWN;
 switch(e->reason){
 case 10:case 14:case 16:case 28:case 29:case 30:case 31:case 32:case 40:case 12:
  return FIR_ACCEPTED; /* Instruction exit, after injected event. */
 case 48:
  /* SDM28.2.4: EPT during event delivery reports vectoring-valid. Handler
   * first-instruction fetch is not delivery; nested handlers are possible. */
  return FIR_ACCEPTED;
 case 52:
  /* SDM27.7.4: entry-time expiration exits AFTER event injection, potentially
   * BEFORE the first guest instruction. Do not reinject for zero execution. */
  return FIR_ACCEPTED;
 default:return FIR_UNKNOWN; /* Includes external IRQ, exception/NMI, window7. */
 }
}
