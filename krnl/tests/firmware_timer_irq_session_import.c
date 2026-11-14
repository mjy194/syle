#include "../firmware_timer_irq_session_import.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct ft100 source(void){struct ft100 s={0};assert(ft100_init(&s,8,100,UINT64_MAX));assert(ft100_start(&s,8,123,10000000,1,0x20020)==FT100_OK);return s;}
static bool wr(void*p,uint32_t f,uint64_t v){(void)f;(void)v;(*(unsigned*)p)++;return true;}
int main(void){
 struct ft100 src=source(),saved=src;struct ftis_session s={0};struct ftirt_route r={0};
 assert(ftis_import(&s,&r,&src,7,0x1234,8,3000123,true));
 assert(s.ready&&r.retired&&r.owner==&s.owner&&r.source==&src);
 assert(s.owner.origin==100&&s.owner.last==3000123&&s.owner.deadline_bus==40000230&&s.owner.expirations==3&&s.owner.pending);
 assert(!memcmp(&saved,&src,sizeof src));
 assert(ftirt_sample(&r,4000123,0,0)&&s.owner.expirations==4);
 struct lei_snapshot g={.rflags=0x202};uint64_t attempt=0;unsigned writes=0;
 assert(ftis_before(&s,7,0x1234,&g,&writes,wr,&attempt)==FTIS_STAGED);
 assert(ftirt_sample(&r,5000123,0,0)&&s.owner.expirations==5);
 assert(ftis_after(&s,7,0x1234,attempt,false,true,31,0,&writes,wr)==FTIS_ACCOUNTED);
 assert(s.owner.pending&&s.owner.in_service&&writes==6);
 assert(!ftis_import(&s,&r,&src,7,0x1234,8,5000123,true)&&s.failed&&r.failed);
 /* Failed admissions cannot publish ready or an active imported owner. */
 for(unsigned k=0;k<10;k++){
  src=source();s=(struct ftis_session){0};r=(struct ftirt_route){0};
  uint64_t domain=7,vmcs=0x1234,epoch=8,ref=456;bool safe=true;
  switch(k){case 0:src.terminal=true;break;case 1:domain=0;break;
  case 2:vmcs=0;break;case 3:epoch=9;break;case 4:ref=122;break;
  case 5:ref=UINT64_MAX;break;case 6:safe=false;break;
  case 7:s.owner.pending=true;break;case 8:s.event.token=1;break;
  case 9:r.import.consumed=true;break;}
  saved=src;assert(!ftis_import(&s,&r,&src,domain,vmcs,epoch,ref,safe));
  assert(s.failed&&r.failed&&!s.ready&&!s.owner.ready&&!r.retired);
  assert(!memcmp(&saved,&src,sizeof src));
  assert(!ftis_init(&s,7,0x1234,0,1));
 }
 s=(struct ftis_session){0};r=(struct ftirt_route){0};src=source();
 assert(ftis_import(&s,&r,&src,7,0x1234,8,456,true));
 assert(!s.owner.pending&&s.owner.deadline_bus==10000230);
 assert(!ftis_import(0,0,0,0,0,0,0,false));
 puts("firmware_timer_irq_session_import: PASS (host-only; one owner, preserved phase)");
}
