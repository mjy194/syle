#include "../firmware_pmtimer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 struct fpm_state s={0};assert(fpm_init(&s,1,100,UINT64_MAX));uint32_t v=0;
 assert(fpm_config(&s,0x40,4,false,&v)&&v==1);
 v=1;assert(!fpm_config(&s,0x80,1,true,&v));v=0xb001;assert(fpm_config(&s,0x40,4,true,&v));v=1;assert(fpm_config(&s,0x80,1,true,&v));
 assert(fpm_io(&s,1,10000100,0xb008,4,true,&v)&&v==3579545&&!s.timer_status);
 uint64_t wrap=(UINT64_C(1)<<24)*10000000/3579545+1;
 assert(fpm_io(&s,1,100+wrap,0xb008,4,true,&v)&&v<2&&s.timer_status);
 struct fpm_state old=s;v=0xabcdef;
 assert(!fpm_io(&s,1,100+wrap,6,4,true,&v)&&v==0xabcdef&&!memcmp(&s,&old,sizeof s));
 assert(!fpm_io(&s,1,100+wrap,0xb008,2,true,&v));assert(!fpm_io(&s,1,100+wrap,0xb008,4,false,&v));
 assert(!fpm_config(&s,0x80,1,true,&v));assert(!fpm_config(&s,0x04,2,false,&v));
 assert(!fpm_io(&s,2,100+wrap,0xb008,4,true,&v)&&s.failed&&v==0xabcdef);
 struct fpm_state max={0};assert(fpm_init(&max,3,0,UINT64_MAX));assert(fpm_sample(&max,3,UINT64_MAX)&&max.ticks<UINT64_MAX);
 struct fpm_state back={0};assert(fpm_init(&back,4,10,10));assert(!fpm_sample(&back,4,9)&&back.failed);
 const uint64_t samples[]={0,1,9,10,9999999,10000000,UINT64_C(1)<<32,UINT64_C(1)<<48,UINT64_C(1)<<63,UINT64_MAX-1,UINT64_MAX};
 for(unsigned i=0;i<sizeof samples/sizeof samples[0];i++){
  struct fpm_state oracle={0};assert(fpm_init(&oracle,5,0,UINT64_MAX));
  assert(fpm_sample(&oracle,5,samples[i]));
  uint64_t expected=(uint64_t)(((__uint128_t)samples[i]*3579545)/10000000);
  assert(oracle.ticks==expected);
 }
 struct fpm_state high={0};assert(fpm_init(&high,6,UINT64_MAX-1000,UINT64_MAX));
 assert(fpm_sample(&high,6,UINT64_MAX)&&high.ticks==357);
 struct fpm_state mult={0};assert(fpm_init(&mult,7,0,UINT64_MAX));
 assert(fpm_sample(&mult,7,1000000000)&&mult.timer_status);
 uint64_t ticks=mult.ticks;assert(fpm_sample(&mult,7,1000000000)&&mult.timer_status&&mult.ticks==ticks);
 puts("firmware_pmtimer: PASS (host-only 24-bit PM contract)");
}
