#include "../nested_domain_dispatch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define REJECT(expr) do{struct nested_domain snapshot=s.domain;assert(!(expr));assert(!memcmp(&snapshot,&s.domain,sizeof snapshot));assert(!s.lock);}while(0)
int main(void){
 struct nested_domain_dispatch s;assert(nested_domain_dispatch_init(&s,1,2));
 struct nested_domain_maintenance_ticket t={0},old;
 REJECT(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_AD_RESET,&t));
 for(unsigned cpu=0;cpu<2;cpu++){
  assert(nested_domain_vmxon(&s.domain,cpu,0xd000+cpu*0x20000));
  struct nested_domain_cpu_token a={1,2,cpu};
  assert(nested_domain_guest_enter(&s.domain,a));
  REJECT(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_AD_RESET,&t));
  assert(nested_domain_guest_leave(&s.domain,a));
 }
 REJECT(nested_domain_dispatch_maintenance_begin(&s,99,&t));
 for(unsigned purpose=1;purpose<=4;purpose++){
  assert(nested_domain_dispatch_maintenance_begin(&s,purpose,&t));
  assert(s.domain.maintenance_targets==3&&s.domain.gate==NESTED_DOMAIN_MAINTENANCE);
  for(unsigned f=0;f<4;f++){
   struct nested_domain_maintenance_ticket bad=t;
   if(f==0)bad.domain_id++;else if(f==1)bad.incarnation++;else if(f==2)bad.epoch++;else bad.purpose=purpose==3?1:3;
   REJECT(nested_domain_dispatch_maintenance_ack(&s,0,bad));
   REJECT(nested_domain_dispatch_maintenance_publish(&s,bad));
   REJECT(nested_domain_dispatch_maintenance_complete(&s,bad));
  }
  struct nested_domain_reclaim_ticket reclaim;
  struct nested_domain_invept_ticket local;
  REJECT(nested_domain_dispatch_reclaim_begin(&s,&reclaim));
  REJECT(nested_domain_invept_begin(&s.domain,0,1,0,&local));
  REJECT(nested_domain_guest_enter(&s.domain,((struct nested_domain_cpu_token){1,2,0})));
  REJECT(nested_domain_dispatch_maintenance_publish(&s,t));
  REJECT(nested_domain_dispatch_maintenance_ack(&s,2,t));
  assert(nested_domain_dispatch_maintenance_ack(&s,0,t));
  REJECT(nested_domain_dispatch_maintenance_ack(&s,0,t));
  REJECT(nested_domain_dispatch_maintenance_publish(&s,t));
  assert(nested_domain_dispatch_maintenance_ack(&s,1,t));
  REJECT(nested_domain_dispatch_maintenance_complete(&s,t));
  assert(nested_domain_dispatch_maintenance_publish(&s,t));
  REJECT(nested_domain_dispatch_maintenance_ack(&s,1,t));
  REJECT(nested_domain_dispatch_maintenance_publish(&s,t));
  assert(nested_domain_dispatch_maintenance_complete(&s,t));
  assert(s.domain.gate==NESTED_DOMAIN_OPEN);
  old=t;
 }
 assert(nested_domain_dispatch_maintenance_begin(&s,1,&t));
 REJECT(nested_domain_dispatch_maintenance_ack(&s,0,old));
 nested_domain_dispatch_markfail(&s);
 REJECT(nested_domain_dispatch_maintenance_ack(&s,0,t));
 REJECT(nested_domain_dispatch_maintenance_publish(&s,t));
 REJECT(nested_domain_dispatch_maintenance_complete(&s,t));
 /* Zero historical residency still requires explicit publication/completion. */
 assert(nested_domain_dispatch_init(&s,3,4));
 assert(nested_domain_vmxon(&s.domain,0,0xd000));assert(nested_domain_vmxon(&s.domain,1,0x2d000));
 s.domain.maintenance_epoch=UINT64_MAX;
 REJECT(nested_domain_dispatch_maintenance_begin(&s,1,&t));s.domain.maintenance_epoch=0;
 assert(nested_domain_dispatch_maintenance_begin(&s,2,&t)&&s.domain.maintenance_targets==0);
 assert(nested_domain_dispatch_maintenance_publish(&s,t));assert(nested_domain_dispatch_maintenance_complete(&s,t));
 puts("nested_domain_maintenance: PASS");
}
