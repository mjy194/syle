#include <assert.h>
#include <stdio.h>
#include "nested_atomic_uc_provision.h"
static unsigned cases;
static uint32_t required[40];
static struct nauc_plan plan(void)
{
 struct nauc_plan p={.nonce=17,.type_epoch=23,.target_hpa=0x22000,.target_gpa=0x3000,
 .host_pte=0x41000,.host_wb_word=UINT64_C(0x8000000000022003),.host_uc_word=UINT64_C(0x800000000002201b),.host_lease=29,
 .inventory=31,.eptp={0x10001e,0x10101e,0x10201e,0x10301e,0x10401e},.cpu={7,11}};
 return p;
}
static const enum nauc_phase schedule[]={
 NAUC_RETAIN,NAUC_PREFLIGHT,NAUC_PREFLIGHT,NAUC_DETACH,
 NAUC_INVEPT_BEFORE,NAUC_INVEPT_BEFORE,NAUC_INVEPT_BEFORE,NAUC_INVEPT_BEFORE,NAUC_INVEPT_BEFORE,
 NAUC_INVEPT_BEFORE,NAUC_INVEPT_BEFORE,NAUC_INVEPT_BEFORE,NAUC_INVEPT_BEFORE,NAUC_INVEPT_BEFORE,
 NAUC_HOST_BREAK,NAUC_INVLPG_BEFORE,NAUC_INVLPG_BEFORE,NAUC_WBINVD_BEFORE,NAUC_WBINVD_BEFORE,
 NAUC_HOST_MAKE,NAUC_INVLPG_AFTER,NAUC_INVLPG_AFTER,NAUC_WBINVD_AFTER,NAUC_WBINVD_AFTER,
 NAUC_AUDIT_UC,NAUC_AUDIT_UC,NAUC_INSTALL,
 NAUC_INVEPT_AFTER,NAUC_INVEPT_AFTER,NAUC_INVEPT_AFTER,NAUC_INVEPT_AFTER,NAUC_INVEPT_AFTER,
 NAUC_INVEPT_AFTER,NAUC_INVEPT_AFTER,NAUC_INVEPT_AFTER,NAUC_INVEPT_AFTER,NAUC_INVEPT_AFTER,
 NAUC_AUDIT_FINAL,NAUC_AUDIT_FINAL,NAUC_PUBLISH
};
#define ACTIONS (sizeof(schedule)/sizeof(schedule[0]))
struct fixture { unsigned calls,fail_at,mode; bool retained,absent,published; struct nauc_receipt previous; struct nauc_controller *controller; };
static bool execute(void *v,const struct nauc_request *q,struct nauc_receipt *r)
{
 struct fixture *f=v;unsigned n=f->calls++;
 assert(n<ACTIONS);assert(q->ordinal==n+1);assert(q->phase==schedule[n]);
 assert(!f->published);assert(q->required_proofs&NAUC_GATE_CLOSED);
 required[n]=q->required_proofs;
 if(n==0){assert(!f->retained);f->retained=true;}else assert(f->retained);
 if(q->phase==NAUC_HOST_BREAK){assert(!f->absent);f->absent=true;}
 else if(q->phase==NAUC_HOST_MAKE){assert(f->absent);f->absent=false;}
 else if(f->absent)assert(q->phase==NAUC_INVLPG_BEFORE||q->phase==NAUC_WBINVD_BEFORE);
 if(q->phase==NAUC_INVEPT_BEFORE||q->phase==NAUC_INVEPT_AFTER){
  unsigned k=n-(q->phase==NAUC_INVEPT_BEFORE?4:27);
  assert(q->cpu==q->plan.cpu[k/5]);assert(q->eptp==q->plan.eptp[k%5]);
 }else{
  assert(q->eptp==0);
  if(q->phase==NAUC_PREFLIGHT||q->phase==NAUC_INVLPG_BEFORE||q->phase==NAUC_WBINVD_BEFORE||q->phase==NAUC_INVLPG_AFTER||q->phase==NAUC_WBINVD_AFTER||q->phase==NAUC_AUDIT_UC||q->phase==NAUC_AUDIT_FINAL){
   unsigned first=n>0&&schedule[n-1]==q->phase?n-1:n;
   assert(q->cpu==q->plan.cpu[n-first]);
  }else assert(q->cpu==NAUC_GLOBAL);
 }
 *r=(struct nauc_receipt){.action=*q,.proofs=q->required_proofs,.completed=true};
 if(f->fail_at==n+1){
  switch(f->mode){
   case 0:return false; /* Uncertain callback may have performed operation. */
   case 1:r->completed=false;break;
   case 2:*r=(struct nauc_receipt){0};break;
   case 3:r->action.plan.nonce++;break;
   case 4:r->action.plan.type_epoch++;break;
   case 5:r->action.plan.target_hpa+=4096;break;
   case 6:r->action.plan.target_gpa+=4096;break;
   case 7:r->action.plan.host_pte+=8;break;
   case 8:r->action.plan.host_wb_word^=2;break;
   case 9:r->action.plan.host_uc_word^=2;break;
   case 10:r->action.plan.host_lease++;break;
   case 11:r->action.plan.inventory++;break;
   case 12:case 13:case 14:case 15:case 16:r->action.plan.eptp[f->mode-12]+=4096;break;
   case 17:case 18:r->action.plan.cpu[f->mode-17]++;break;
   case 19:r->action.ordinal--;break;
   case 20:r->action.ordinal++;break;
   case 21:r->action.phase++;break;
   case 22:r->action.cpu^=1;break;
   case 23:r->action.eptp^=4096;break;
   case 24:r->action.required_proofs^=1;break;
   case 25:r->proofs=0;break;
   case 26:*r=f->previous;break; /* Duplicate/replayed physical action. */
   case 27:assert(!nauc_run(f->controller,&q->plan,execute,f));break;
   default:assert(f->mode<37);r->proofs&=~(1u<<(f->mode-28));break;
  }
 }
 f->previous=*r;
 if(q->phase==NAUC_PUBLISH)f->published=true;
 return true;
}
int main(void)
{
 struct nauc_plan p=plan();struct nauc_controller c={0};struct fixture f={.controller=&c};
 assert(nauc_run(&c,&p,execute,&f));assert(f.calls==ACTIONS&&c.completed_actions==ACTIONS);
 assert(c.state==NAUC_PUBLISHED&&c.retained&&f.published&&!f.absent);cases++;
 assert(!nauc_run(&c,&p,execute,&f));assert(f.calls==ACTIONS&&c.state==NAUC_POISONED);cases++;
 for(unsigned at=1;at<=ACTIONS;at++)for(unsigned mode=0;mode<37;mode++){
  if(mode>=28&&!(required[at-1]&(1u<<(mode-28))))continue;
  c=(struct nauc_controller){0};f=(struct fixture){.controller=&c,.fail_at=at,.mode=mode};
  assert(!nauc_run(&c,&p,execute,&f));assert(c.state==NAUC_POISONED);
  assert(f.calls==at&&c.completed_actions==at-1&&c.retention_attempted);
  assert(c.retained==(at>1));assert(!nauc_run(&c,&p,execute,&f));assert(f.calls==at);
  if(at<ACTIONS)assert(!f.published);
  cases++;
 }
 for(unsigned mode=0;mode<22;mode++){
  struct nauc_plan bad=p;
  switch(mode){
   case 0:bad.nonce=0;break;case 1:bad.type_epoch=0;break;
   case 2:bad.target_hpa=0;break;case 3:bad.target_hpa++;break;
   case 4:bad.target_gpa++;break;case 5:bad.host_pte=0;break;
   case 6:bad.host_pte++;break;case 7:bad.host_lease=0;break;
   case 8:bad.inventory=0;break;case 9:bad.host_wb_word=0;break;
   case 10:bad.host_uc_word=0;break;case 11:bad.host_uc_word=bad.host_wb_word;break;
   case 12:bad.cpu[1]=bad.cpu[0];break;case 13:bad.cpu[0]=NAUC_GLOBAL;break;
   case 16:bad.target_gpa=0;break;
   case 17:bad.host_wb_word^=4096;break;
   case 18:bad.host_wb_word^=UINT64_C(0x8000000000000000);break;
   case 19:bad.host_wb_word|=128;break;
   case 20:bad.host_wb_word|=8;break;
   case 21:bad.host_uc_word^=2;break;
   case 14:bad.eptp[1]=bad.eptp[0];break;case 15:bad.eptp[0]^=64;break;
  }
  c=(struct nauc_controller){0};f=(struct fixture){.controller=&c};
  assert(!nauc_run(&c,&bad,execute,&f));assert(!f.calls&&c.state==NAUC_POISONED);cases++;
 }
 printf("PASS N46B provision %u cases, %zu exact actions; mocked primitives, no hardware claim\n",cases,ACTIONS);
 return 0;
}
