#include "../firmware_irq_receipt.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 struct fir_identity i={1,0x1000,3};
 struct fir_evidence e={.observed=i,.staged_info=0x80000020,.native_status=1,
 .reason=52,.run_reported_exit=true,.host_restored=true,.profile_admitted=true};
 assert(fir_classify(&i,&e)==FIR_ACCEPTED);
 e.reason=48;assert(fir_classify(&i,&e)==FIR_ACCEPTED);
 e.vectoring=0x7fffffff;assert(fir_classify(&i,&e)==FIR_ACCEPTED);
 e.vectoring|=0x80000000;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e.vectoring=0;
 for(unsigned r=0;r<64;r++){
  e.reason=r;bool accept=r==10||r==14||r==16||r==28||r==29||r==30||r==31||r==32||r==40||r==12||r==48||r==52;
  assert(fir_classify(&i,&e)==(accept?FIR_ACCEPTED:FIR_UNKNOWN));
 }
 for(unsigned r=0x21;r<=0x22;r++){e.reason=0x80000000u|r;assert(fir_classify(&i,&e)==FIR_NO_ENTRY);}
 e.reason=0x80000030;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e.reason=48;
 struct fir_evidence old=e;
 e.observed.attempt++;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e=old;
 e.observed.domain++;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e=old;
 e.observed.vmcs+=4096;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e=old;
 for(unsigned b=0;b<32;b++){e.staged_info=old.staged_info^(1u<<b);assert(fir_classify(&i,&e)==FIR_UNKNOWN);}e=old;
 e.native_status=0;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e.native_status=2;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e=old;
 e.run_reported_exit=false;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e=old;
 e.host_restored=false;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e=old;
 e.profile_admitted=false;assert(fir_classify(&i,&e)==FIR_UNKNOWN);e=old;
 e.exit_interruption=0x80001002;assert(fir_classify(&i,&e)==FIR_UNKNOWN);
 assert(fir_classify(0,&e)==FIR_UNKNOWN&&fir_classify(&i,0)==FIR_UNKNOWN);
 puts("firmware receipt: bound evidence, vector-phase not handler execution,48/52 and unknown isolation PASS");
}
