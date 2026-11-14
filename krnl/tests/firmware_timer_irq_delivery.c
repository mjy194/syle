#include "../firmware_timer_irq_delivery.h"
#include <assert.h>
#include <stdio.h>
struct writes {unsigned n,fail;struct ftis_session*reenter;struct fine_outcome*o;};
static bool wr(void*p,uint32_t f,uint64_t v){struct writes*w=p;(void)f;(void)v;++w->n;
 if(w->reenter)assert(ftid_after(w->reenter,w->o,w,wr)==FTIS_FATAL);
 return w->n!=w->fail;
}
static struct ftis_session fresh(void){struct ftis_session s={0};assert(ftis_init(&s,7,0x1234,0,UINT64_MAX));assert(ftis_sample(&s,7,0x1234,1000000));return s;}
static struct fine_outcome stage(struct ftis_session*s){struct writes w={0};struct lei_snapshot g={.rflags=0x202};uint64_t a;
 assert(ftis_before(s,7,0x1234,&g,&w,wr,&a)==FTIS_STAGED);
 return (struct fine_outcome){.evidence={.observed={7,0x1234,a},.staged_info=0x80000020,.native_status=1,.reason=48,.run_reported_exit=true,.host_restored=true,.profile_admitted=true},.run_result=LVS_EXIT,.status_sentinel_set=true,.exit_fields_read=true};
}
int main(void){
 struct ftis_session s;struct fine_outcome o;struct writes w;
 for(unsigned k=0;k<2;k++){s=fresh();o=stage(&s);w=(struct writes){0};o.evidence.reason=k?52:48;
  o.evidence.vectoring=0x7fffffff;assert(ftis_sample(&s,7,0x1234,2000000));
  assert(ftid_after(&s,&o,&w,wr)==FTIS_ACCOUNTED&&w.n==3&&s.owner.in_service&&s.owner.pending);
  assert(ftis_eoi(&s,7,0x1234));assert(ftid_after(&s,&o,&w,wr)==FTIS_FATAL&&w.n==3);
 }
 /* Failure at each metadata write never accounts or cancels. */
 for(unsigned path=0;path<2;path++)for(unsigned n=1;n<=3;n++){
  s=fresh();o=stage(&s);w=(struct writes){.fail=n};if(path)o.evidence.reason=0x80000021;
  assert(ftid_after(&s,&o,&w,wr)==FTIS_FATAL&&w.n==n&&s.owner.reserved&&s.owner.pending&&!s.owner.in_service);
 }
 /* Entry-failed exits and definite assembly no-entry status0/2. */
 for(unsigned k=0;k<4;k++){s=fresh();o=stage(&s);w=(struct writes){0};
  if(k<2)o.evidence.reason=0x80000021+k;
  else {o.evidence.native_status=k==2?0:2;o.evidence.reason=0;o.evidence.run_reported_exit=false;o.run_result=LVS_ENTRY_FAILED;o.exit_fields_read=false;}
  assert(ftid_after(&s,&o,&w,wr)==FTIS_STOP_NO_ENTRY&&w.n==3&&s.failed&&s.owner.pending&&!s.owner.reserved&&!s.owner.in_service);
 }
 /* Complete evidence and stage binding, no bare receipt/result accepted. */
 for(unsigned k=0;k<14;k++){s=fresh();o=stage(&s);w=(struct writes){0};
  switch(k){case 0:o.evidence.vectoring=0x80000020;break;case 1:o.evidence.observed.domain++;break;
  case 2:o.evidence.observed.vmcs++;break;case 3:o.evidence.observed.attempt++;break;
  case 4:o.evidence.staged_info=0x80000021;break;case 5:o.status_sentinel_set=false;break;
  case 6:o.evidence.host_restored=false;break;case 7:o.evidence.profile_admitted=false;break;
  case 8:o.evidence.native_status=0;break;case 9:o.evidence.reason=7;break;
  case 10:o.evidence.exit_interruption=0x80000020;break;case 11:s.event.token++;break;
  case 12:s.event.vector=33;break;case 13:o.exit_fields_read=false;break;}
  assert(ftid_after(&s,&o,&w,wr)==FTIS_FATAL&&!w.n&&s.owner.pending&&s.owner.reserved&&!s.owner.in_service);
 }
 s=fresh();o=stage(&s);w=(struct writes){.reenter=&s,.o=&o};
 assert(ftid_after(&s,&o,&w,wr)==FTIS_FATAL&&w.n==1&&s.owner.reserved);
 s=fresh();o=stage(&s);assert(!ftis_eoi(&s,7,0x1234));w=(struct writes){0};assert(ftid_after(&s,&o,&w,wr)==FTIS_FATAL&&!w.n);
 assert(ftid_after(0,0,0,0)==FTIS_FATAL);
 puts("firmware_timer_irq_delivery: PASS (host-only; accepted does not mean handler ran)");
}
