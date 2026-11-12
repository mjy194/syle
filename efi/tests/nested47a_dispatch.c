/* Production transitions and the original dispatch regression suite. */
#include "../../krnl/tests/nested47a_dispatch_base.inc"
#define DENY(expr) do {struct nested_domain_dispatch saved_=s; assert(!(expr)); assert(!memcmp(&saved_,&s,sizeof s));} while(0)
static void owners(struct nested_domain_dispatch *s,struct nvmx m[2]) {
 setup(s,m);
 for(unsigned c=0;c<2;c++) {
  assert(execute(s,m,c,NVMX_ON,m[c].on_region).status==NVMX_SUCCESS);
  assert(execute(s,m,c,NVMX_CLEAR,m[c].vmcs_region).status==NVMX_SUCCESS);
  assert(execute(s,m,c,NVMX_PTRLD,m[c].vmcs_region).status==NVMX_SUCCESS);
 }
}
static void enter(struct nested_domain_dispatch *s,struct nvmx m[2],unsigned c) {
 assert(execute(s,m,c,m[c].launched?NVMX_RESUME:NVMX_LAUNCH,0).status==NVMX_ENTRY_REQUEST);
 assert(nested_domain_dispatch_physical_enter(s,c,s->tokens[c],&m[c],true));
}
static void leave(struct nested_domain_dispatch *s,struct nvmx m[2],unsigned c) {
 assert(nested_domain_dispatch_physical_leave(s,c,s->tokens[c],&m[c],true,0,18));
 assert(nvmx_return_from_l2(&m[c]));
}
static void checkpoint(struct nested_domain_dispatch *s,struct nvmx m[2]) {
 owners(s,m);for(unsigned c=0;c<2;c++){enter(s,m,c);leave(s,m,c);}
}
static void finish(struct nested_domain_dispatch *s,struct nested_domain_maintenance_ticket t) {
 assert(nested_domain_dispatch_maintenance_ack(s,0,t));
 assert(nested_domain_dispatch_maintenance_ack(s,1,t));
 assert(nested_domain_dispatch_maintenance_publish(s,t));
 assert(nested_domain_dispatch_maintenance_complete(s,t));
}
static void transitions(void) {
 struct nested_domain_dispatch s;struct nvmx m[2];owners(&s,m);
 struct nested_domain_maintenance_ticket t={.epoch=999},sentinel=t;
 DENY(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
 for(unsigned c=0;c<2;c++) {
  enter(&s,m,c);
  DENY(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
  assert(nested_domain_dispatch_physical_suspend_ept(&s,c,s.tokens[c],&m[c],0,48));
 }
 assert(s.domain.residency==3);
 DENY(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
 assert(!memcmp(&t,&sentinel,sizeof t));
 /* Ordinary maintenance is deliberately unchanged for internal suspensions. */
 assert(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_AD_RESET,&t));
 for(unsigned c=0;c<2;c++)assert(!nested_domain_dispatch_source_stopped(&s,c,s.tokens[c],t));
 finish(&s,t);
 for(unsigned c=0;c<2;c++) {
  assert(nested_domain_dispatch_physical_resume(&s,c,s.tokens[c],&m[c]));
  DENY(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
  leave(&s,m,c);
  if(c==0)DENY(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
 }
 assert(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
 for(unsigned c=0;c<2;c++)assert(nested_domain_dispatch_source_stopped(&s,c,s.tokens[c],t));
 finish(&s,t);
 for(unsigned c=0;c<2;c++){assert(!nested_domain_dispatch_source_stopped(&s,c,s.tokens[c],t));enter(&s,m,c);leave(&s,m,c);}
 puts("PASS N47A reason48 suspension rejected; full VMCALL leaves permit source round; actual model resume after commit");
}
static void tickets(void) {
 struct nested_domain_dispatch s;struct nvmx m[2];checkpoint(&s,m);
 struct nested_domain_maintenance_ticket t;
 assert(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
 for(unsigned f=0;f<8;f++) {
  struct nested_domain_maintenance_ticket bad=t;
  switch(f){case 0:bad.domain_id++;break;case 1:bad.incarnation++;break;case 2:bad.epoch++;break;case 3:bad.epoch=0;break;default:bad.purpose=f-3;break;}
  DENY(nested_domain_dispatch_source_stopped(&s,0,s.tokens[0],bad));
  DENY(nested_domain_dispatch_maintenance_ack(&s,0,bad));
  DENY(nested_domain_dispatch_maintenance_publish(&s,bad));
  DENY(nested_domain_dispatch_maintenance_complete(&s,bad));
 }
 for(unsigned f=0;f<5;f++) {
  struct nested_domain_token bad=s.tokens[0];
  switch(f){case 0:bad.domain_id++;break;case 1:bad.incarnation++;break;case 2:bad.generation++;break;case 3:bad.slot=NESTED_DOMAIN_OBJECTS;break;default:bad=s.tokens[1];break;}
  DENY(nested_domain_dispatch_source_stopped(&s,0,bad,t));
 }
 DENY(nested_domain_dispatch_source_stopped(&s,2,s.tokens[0],t));
 DENY(nested_domain_dispatch_maintenance_ack(&s,2,t));
 DENY(nested_domain_dispatch_maintenance_publish(&s,t));
 DENY(nested_domain_dispatch_maintenance_complete(&s,t));
 assert(nested_domain_dispatch_maintenance_ack(&s,0,t));
 DENY(nested_domain_dispatch_maintenance_ack(&s,0,t));
 DENY(nested_domain_dispatch_maintenance_publish(&s,t));
 assert(nested_domain_dispatch_maintenance_ack(&s,1,t));
 DENY(nested_domain_dispatch_maintenance_complete(&s,t));
 assert(nested_domain_dispatch_source_stopped(&s,0,s.tokens[0],t));
 assert(nested_domain_dispatch_maintenance_publish(&s,t));
 assert(nested_domain_dispatch_source_stopped(&s,1,s.tokens[1],t));
 DENY(nested_domain_dispatch_maintenance_ack(&s,1,t));
 assert(nested_domain_dispatch_maintenance_complete(&s,t));
 DENY(nested_domain_dispatch_source_stopped(&s,0,s.tokens[0],t));
 DENY(nested_domain_dispatch_maintenance_ack(&s,1,t));
 struct nested_domain_maintenance_ticket old=t;
 assert(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
 DENY(nested_domain_dispatch_source_stopped(&s,0,s.tokens[0],old));
 DENY(nested_domain_dispatch_maintenance_ack(&s,0,old));
 finish(&s,t);
 puts("PASS N47A exact full ticket/token/CPU; stale and duplicate acknowledgements; ready before and after commit");
}
static void perturb(struct nested_domain_dispatch *s,unsigned c,unsigned f) {
 struct nested_domain_token *t=&s->tokens[c];struct nested_domain_vmcs *o=&s->domain.objects[t->slot];
 switch(f){
 case 0:t->domain_id++;break;case 1:t->incarnation++;break;case 2:t->generation++;break;case 3:t->slot=NESTED_DOMAIN_OBJECTS;break;
 case 4:o->used=false;break;case 5:o->active=false;break;case 6:o->owner=1-c;break;case 7:o->launched=false;break;
 case 8:o->prepared=true;break;case 9:o->running=true;break;case 10:o->gpa+=4096;break;
 case 11:s->domain.cpus[c].root=false;break;case 12:s->domain.cpus[c].current=NESTED_DOMAIN_NONE;break;
 case 13:s->domain.cpus[c].guest_running=true;break;case 14:s->domain.cpus[c].pending_ticket=1;break;
 case 15:s->internal_pending[c]=true;break;case 16:s->internal_resumed[c]=true;break;case 17:s->internal_origin[c]=NESTED_INTERNAL_EPT;break;
 case 18:s->domain.residency=1;break;case 19:s->domain.participants=1;break;case 20:s->domain.cpus[c].vmxon+=4096;break;
 case 21:s->domain.objects[15].prepared=true;break;case 22:s->domain.objects[15].running=true;break;
 case 23:s->domain.cpus[7].guest_running=true;break;case 24:s->domain.cpus[7].pending_ticket=1;break;
 }
}
static void invariants(void) {
 for(unsigned c=0;c<2;c++)for(unsigned f=0;f<25;f++) {
  struct nested_domain_dispatch s;struct nvmx m[2];checkpoint(&s,m);
  struct nested_domain_maintenance_ticket t={.epoch=999},sentinel=t;
  struct nested_domain_dispatch pristine=s;perturb(&s,c,f);
  DENY(nested_domain_dispatch_source_plan(&s,&t));
  assert(!memcmp(&t,&sentinel,sizeof t));
  DENY(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
  assert(!memcmp(&t,&sentinel,sizeof t));s=pristine;
  assert(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&t));
  perturb(&s,c,f);
  /* Even querying the OTHER owner must reject a broken global checkpoint. */
  DENY(nested_domain_dispatch_source_stopped(&s,1-c,s.tokens[1-c],t));
 }
 puts("PASS N47A 50 owner/global corruptions reject before ticket and under closed query lock");
}
static bool same_ticket(struct nested_domain_maintenance_ticket a,struct nested_domain_maintenance_ticket b) {
 return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.epoch==b.epoch&&a.purpose==b.purpose;
}
static void plans(void) {
 struct nested_domain_dispatch s;struct nvmx m[2];checkpoint(&s,m);
 struct nested_domain_maintenance_ticket expected={0},issued,sentinel={.epoch=999},out=sentinel;
 struct nested_domain_dispatch before=s;
 assert(nested_domain_dispatch_source_plan(&s,&expected));
 assert(!memcmp(&s,&before,sizeof s));
 assert(expected.domain_id==s.domain.id&&expected.incarnation==s.domain.incarnation&&
        expected.epoch==s.domain.maintenance_epoch+1&&expected.purpose==NESTED_DOMAIN_SOURCE_REPLACE);
 DENY(nested_domain_dispatch_source_plan(&s,NULL));
 assert(!nested_domain_dispatch_source_plan(NULL,&out)&&same_ticket(out,sentinel));
 assert(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&issued));
 assert(same_ticket(expected,issued));
 DENY(nested_domain_dispatch_source_plan(&s,&out));assert(same_ticket(out,sentinel));
 finish(&s,issued);
 before=s;struct nested_domain_maintenance_ticket next;
 assert(nested_domain_dispatch_source_plan(&s,&next));
 assert(!memcmp(&s,&before,sizeof s)&&next.epoch==expected.epoch+1&&!same_ticket(next,expected));
 assert(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&issued));
 assert(same_ticket(next,issued)&&!same_ticket(expected,issued));
 DENY(nested_domain_dispatch_source_stopped(&s,0,s.tokens[0],expected));
 DENY(nested_domain_dispatch_maintenance_ack(&s,0,expected));
 finish(&s,issued);
 for(unsigned f=0;f<2;f++){
  before=s;
  if(f){s.domain.incarnation=0;s.tokens[0].incarnation=0;s.tokens[1].incarnation=0;}
  else{s.domain.id=0;s.tokens[0].domain_id=0;s.tokens[1].domain_id=0;}
  DENY(nested_domain_dispatch_source_plan(&s,&out));assert(same_ticket(out,sentinel));s=before;
 }
 s.domain.maintenance_epoch=UINT64_MAX;
 DENY(nested_domain_dispatch_source_plan(&s,&out));assert(same_ticket(out,sentinel));
 DENY(nested_domain_dispatch_maintenance_begin(&s,NESTED_DOMAIN_SOURCE_REPLACE,&out));assert(same_ticket(out,sentinel));
 checkpoint(&s,m);enter(&s,m,0);
 assert(nested_domain_dispatch_physical_suspend_ept(&s,0,s.tokens[0],&m[0],0,48));
 DENY(nested_domain_dispatch_source_plan(&s,&out));assert(same_ticket(out,sentinel));
 puts("PASS N47A readonly source plan, failure atomic output/state, exact issued ticket, stale plan, overflow and internal suspend rejection");
}
int main(void){original_dispatch_main();transitions();tickets();invariants();plans();return 0;}
