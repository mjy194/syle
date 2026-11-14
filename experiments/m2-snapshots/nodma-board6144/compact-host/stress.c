#include "firmware_observer_compact.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t buffer[FOC_MAX_CAPACITY];
struct hash {uint64_t hash,bytes;unsigned calls,fail;};
static void digest(struct hash*h,const char*p,size_t n){for(size_t i=0;i<n;i++)h->hash=(h->hash^(uint8_t)p[i])*UINT64_C(1099511628211);h->bytes+=n;}
static bool sink(void*p,const char*b,size_t n){struct hash*h=p;digest(h,b,n);return ++h->calls!=h->fail;}
static void oracle_hex(struct hash*h,uint64_t v){char b[20];int n=snprintf(b,sizeof b,"0x%016llx\n",(unsigned long long)v);assert(n==19);digest(h,b,19);}
int main(void){struct foc_trace t={0};struct hash actual={0},expected={0};assert(foc_init(&t,buffer,sizeof buffer,&actual,sink)&&foc_defer(&t));
 uint64_t rng=123;for(unsigned i=0;i<10000;i++){rng=rng*6364136223846793005ull+1;
  if(rng&1){assert(foc_hex(&t,rng));oracle_hex(&expected,rng);}else{char b[16];size_t n=(rng>>8)%16;for(size_t j=0;j<n;j++)b[j]=(char)((rng>>(j%8*8))&255);assert(foc_text(&t,b,n));digest(&expected,b,n);}}
 assert(foc_flush(&t)&&actual.hash==expected.hash&&actual.bytes==expected.bytes&&t.decoded_events==10000);
 /* Capacity near2MiB, all-hex encoded9 vs logical19, no omitted records. */
 t=(struct foc_trace){0};actual=(struct hash){0};expected=(struct hash){0};assert(foc_init(&t,buffer,sizeof buffer,&actual,sink)&&foc_defer(&t));
 unsigned count=FOC_MAX_CAPACITY/9;for(unsigned i=0;i<count;i++){assert(foc_hex(&t,i));oracle_hex(&expected,i);}
 assert(t.used==(size_t)count*9&&foc_flush(&t)&&actual.hash==expected.hash&&actual.bytes==expected.bytes&&actual.calls==count);
 /* Every truncated prefix fails before any sink; no plausible partial replay. */
 uint8_t pristine[64];size_t size;
 t=(struct foc_trace){0};actual=(struct hash){0};assert(foc_init(&t,buffer,sizeof buffer,&actual,sink)&&foc_defer(&t));
 assert(foc_text(&t,"abc",3)&&foc_text(&t,"abc",3)&&foc_hex(&t,123));size=t.used;memcpy(pristine,buffer,size);
 struct foc_trace base=t;
 for(size_t cut=0;cut<size;cut++){t=base;t.used=cut;actual=(struct hash){0};assert(!foc_flush(&t)&&t.failed&&!actual.calls);}
 /* Control-field flips that invalidate tags/IDs/lengths are rejected pre-sink. */
 unsigned positions[]={0,1,2,3,4,8,9,10,11};
 for(unsigned k=0;k<sizeof positions/sizeof *positions;k++)for(unsigned bit=0;bit<8;bit++){
  t=base;actual=(struct hash){0};memcpy(buffer,pristine,size);buffer[positions[k]]^=1u<<bit;
  assert(!foc_flush(&t)&&t.failed&&!actual.calls);
 }
 memcpy(buffer,pristine,size);
 /* Partial sink failure at each event, at-most-once attempted prefix. */
 for(unsigned fail=1;fail<=3;fail++){t=base;actual=(struct hash){.fail=fail};assert(!foc_flush(&t)&&t.sink_failed&&actual.calls==fail);assert(!foc_flush(&t)&&actual.calls==fail);}
 puts("compact stress: PASS randomized oracle/2MiB/all truncations/controls/each sink failure");}
