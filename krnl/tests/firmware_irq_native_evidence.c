#include "../firmware_irq_native_evidence.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 struct fir_identity id={1,0x1000,4};
 for(unsigned status=0;status<=3;status++)for(unsigned run=0;run<=LVS_EXIT;run++){
  struct fine_outcome o={.evidence={.observed=id,.staged_info=0x80000020,
   .native_status=status,.host_restored=true,.profile_admitted=true},.run_result=run,.status_sentinel_set=true};
  if(status==1){o.exit_fields_read=true;o.evidence.run_reported_exit=true;o.evidence.reason=52;}
  enum fir_result expected=(status==1&&run==LVS_EXIT)?FIR_ACCEPTED:
   ((status==0||status==2)&&run==LVS_ENTRY_FAILED)?FIR_NO_ENTRY:FIR_UNKNOWN;
  assert(fine_classify(&id,&o)==expected);
  struct fine_outcome bad=o;bad.status_sentinel_set=false;assert(fine_classify(&id,&bad)==FIR_UNKNOWN);
  bad=o;bad.evidence.host_restored=false;assert(fine_classify(&id,&bad)==FIR_UNKNOWN);
  bad=o;bad.evidence.observed.attempt++;assert(fine_classify(&id,&bad)==FIR_UNKNOWN);
 }
 struct fine_outcome o={.evidence={.observed=id,.staged_info=0x80000020,.native_status=0,
 .host_restored=true,.profile_admitted=true},.run_result=LVS_ENTRY_FAILED,.status_sentinel_set=true};
 o.exit_fields_read=true;assert(fine_classify(&id,&o)==FIR_UNKNOWN);o.exit_fields_read=false;
 o.evidence.reason=52;assert(fine_classify(&id,&o)==FIR_UNKNOWN);o.evidence.reason=0;
 o.evidence.native_status=UINT32_MAX;assert(fine_classify(&id,&o)==FIR_UNKNOWN);
 o.evidence.native_status=1;o.evidence.run_reported_exit=true;o.evidence.reason=0x80000021;
 o.exit_fields_read=true;o.run_result=LVS_EXIT;assert(fine_classify(&id,&o)==FIR_NO_ENTRY);
 o.run_result=LVS_GUEST_CAPTURE_FAILED;assert(fine_classify(&id,&o)==FIR_UNKNOWN);
 puts("native evidence: status0/2/1 cross lvs outcomes, sentinel, no stale VMREAD, host safety PASS");
}
