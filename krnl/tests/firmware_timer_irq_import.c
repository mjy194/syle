#include "../firmware_timer_irq_import.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct ft100 source(void){struct ft100 s={0};assert(ft100_init(&s,8,100,UINT64_MAX));assert(ft100_start(&s,8,123,10000000,1,0x20020)==FT100_OK);return s;}
int main(void){
 struct ft100 s=source(),old=s;struct fti_owner o={0};struct fti_import_gate g={0};
 assert(fti_import_once(&g,&o,&s,7,8,456,true));assert(!memcmp(&s,&old,sizeof s));
 assert(o.origin==100&&o.last==456&&o.deadline_bus==10000230&&!o.pending);
 assert(o.domain==7&&g.epoch==8&&g.consumed);
 struct fti_owner saved=o;assert(!fti_import_once(&g,&o,&s,7,8,457,true)&&g.failed);
 assert(!memcmp(&o,&saved,sizeof o));
 /* Exact first due and multi-period delayed import preserve old phase. */
 for(unsigned k=1;k<=4;k++){s=source();g=(struct fti_import_gate){0};o=(struct fti_owner){0};
  uint64_t now=123+(uint64_t)k*1000000;
  assert(fti_import_once(&g,&o,&s,7,8,now,true));
  assert(o.pending&&o.expirations==k&&o.deadline_bus==230+(uint64_t)(k+1)*10000000);
  uint64_t t;uint8_t v;assert(fti_reserve(&o,&t,&v)&&v==32);
 }
 /* Invalid profile/source/ownership/time and arithmetic remain unpublished. */
 for(unsigned k=0;k<15;k++){s=source();g=(struct fti_import_gate){0};o=(struct fti_owner){0};
  uint64_t ref=456,epoch=8,domain=7;bool quiet=true;
  switch(k){case 0:s.terminal=true;break;case 1:s.ready=false;break;
  case 2:s.armed=false;break;case 3:s.initial--;break;case 4:s.divisor=2;break;
  case 5:s.lvt=0x30020;break;case 6:s.period_bus--;break;case 7:epoch=9;break;
  case 8:domain=0;break;case 9:quiet=false;break;case 10:ref=122;break;
  case 11:s.max_gap=1;break;case 12:ref=UINT64_MAX;break;
  case 13:s.deadline_bus=230;break;case 14:o.pending=true;break;}
  old=s;saved=o;assert(!fti_import_once(&g,&o,&s,domain,epoch,ref,quiet)&&g.failed);
  assert(!memcmp(&s,&old,sizeof s)&&!memcmp(&o,&saved,sizeof o));
 }
 s=source();s.origin=0;s.last=0;s.deadline_bus=UINT64_MAX-10;
 g=(struct fti_import_gate){0};o=(struct fti_owner){0};
 assert(!fti_import_once(&g,&o,&s,7,8,UINT64_MAX/10,true)&&g.failed&&!o.ready);
 assert(!fti_import_once(0,&o,&s,7,8,0,true));
 puts("firmware_timer_irq_import: PASS (host-only ownership transfer contract)");
}
