#define _POSIX_C_SOURCE 200809L
#include "nested_atomic_uc_provision.h"
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Thread_local unsigned actual_cpu;
struct model {struct nauc_controller c;struct nauc_plan plan;unsigned calls,fail_at,mode;bool retained,detached,absent,uc,installed,published;unsigned before[2],after[2],pre,tlb1,cache1,tlb2,cache2,audit,final;pthread_mutex_t lock;pthread_cond_t changed;bool pending,done,stop,ok;struct nauc_request request;struct nauc_receipt receipt;};
static struct nauc_plan plan(void){return (struct nauc_plan){.nonce=51,.type_epoch=83,.target_hpa=0x99000,.target_gpa=0xf000,.host_pte=0x14018,.host_wb_word=UINT64_C(0x8000000000099003),.host_uc_word=UINT64_C(0x800000000009901b),.host_lease=13,.inventory=17,.eptp={0x20001e,0x21001e,0x22001e,0x23001e,0x24001e},.cpu={0,1}};}
static bool operation(struct model *m,const struct nauc_request *q,struct nauc_receipt *r){unsigned cpu=actual_cpu;assert(cpu<2);assert(q->cpu==NAUC_GLOBAL?cpu==0:q->cpu==cpu);assert(!m->published&&q->ordinal==++m->calls&&q->plan.nonce==51&&q->plan.type_epoch==83&&(q->required_proofs&NAUC_GATE_CLOSED));unsigned bit=1u<<cpu;
 switch(q->phase){case NAUC_RETAIN:assert(!m->retained);m->retained=true;break;
 case NAUC_PREFLIGHT:assert(m->retained&&!m->detached&&!(m->pre&bit));m->pre|=bit;break;
 case NAUC_DETACH:assert(m->pre==3);m->detached=true;break;
 case NAUC_INVEPT_BEFORE:case NAUC_INVEPT_AFTER:{assert(m->detached);unsigned index=99;for(unsigned i=0;i<5;i++)if(q->eptp==m->plan.eptp[i])index=i;assert(index<5);unsigned *mask=q->phase==NAUC_INVEPT_BEFORE?m->before:m->after;assert(!(mask[cpu]&(1u<<index)));if(q->phase==NAUC_INVEPT_AFTER)assert(m->installed&&m->uc);mask[cpu]|=1u<<index;break;}
 case NAUC_HOST_BREAK:assert(m->before[0]==31&&m->before[1]==31);m->absent=true;break;
 case NAUC_INVLPG_BEFORE:assert(m->absent&&!(m->tlb1&bit));m->tlb1|=bit;break;
 case NAUC_WBINVD_BEFORE:assert(m->absent&&m->tlb1==3&&!(m->cache1&bit));m->cache1|=bit;break;
 case NAUC_HOST_MAKE:assert(m->absent&&m->cache1==3);m->absent=false;m->uc=true;break;
 case NAUC_INVLPG_AFTER:assert(m->uc&&!m->absent&&!(m->tlb2&bit));m->tlb2|=bit;break;
 case NAUC_WBINVD_AFTER:assert(m->uc&&m->tlb2==3&&!(m->cache2&bit));m->cache2|=bit;break;
 case NAUC_AUDIT_UC:assert(m->uc&&m->cache2==3&&!(m->audit&bit));m->audit|=bit;break;
 case NAUC_INSTALL:assert(m->uc&&m->audit==3);m->installed=true;break;
 case NAUC_AUDIT_FINAL:assert(m->installed&&m->after[0]==31&&m->after[1]==31&&!(m->final&bit));m->final|=bit;break;
 case NAUC_PUBLISH:assert(m->final==3&&!m->absent&&m->uc&&m->retained);m->published=true;break;
 default:assert(0);}
 *r=(struct nauc_receipt){.action=*q,.proofs=q->required_proofs,.completed=true};
 if(m->calls==m->fail_at){switch(m->mode){case 0:return false;case 1:r->action.ordinal--;break;case 2:r->action.cpu^=1;break;case 3:r->action.plan.type_epoch--;break;case 4:r->action.plan.nonce--;break;case 5:r->action.eptp^=4096;break;case 6:r->proofs&=~NAUC_GATE_CLOSED;break;case 7:r->completed=false;break;default:assert(0);}}
 return true;}
static void *ap(void *v){struct model *m=v;actual_cpu=1;assert(!pthread_mutex_lock(&m->lock));for(;;){while(!m->pending&&!m->stop)assert(!pthread_cond_wait(&m->changed,&m->lock));if(m->stop)break;m->ok=operation(m,&m->request,&m->receipt);m->pending=false;m->done=true;assert(!pthread_cond_signal(&m->changed));}assert(!pthread_mutex_unlock(&m->lock));return NULL;}
static bool execute(void *v,const struct nauc_request *q,struct nauc_receipt *r){struct model *m=v;assert(actual_cpu==0);if(q->cpu!=1)return operation(m,q,r);assert(!pthread_mutex_lock(&m->lock));assert(!m->pending);m->request=*q;m->pending=true;m->done=false;assert(!pthread_cond_signal(&m->changed));while(!m->done)assert(!pthread_cond_wait(&m->changed,&m->lock));*r=m->receipt;bool ok=m->ok;assert(!pthread_mutex_unlock(&m->lock));return ok;}
static void one(unsigned at,unsigned mode){struct model m={.plan=plan(),.fail_at=at,.mode=mode};assert(!pthread_mutex_init(&m.lock,NULL));assert(!pthread_cond_init(&m.changed,NULL));pthread_t t;actual_cpu=0;assert(!pthread_create(&t,NULL,ap,&m));bool ok=nauc_run(&m.c,&m.plan,execute,&m);if(!at){assert(ok&&m.c.state==NAUC_PUBLISHED&&m.calls==40&&m.c.completed_actions==40&&m.published);}else{assert(!ok&&m.c.state==NAUC_POISONED&&m.calls==at&&m.c.completed_actions==at-1&&m.retained&&m.c.retention_attempted);assert(m.published==(at==40));assert(!nauc_run(&m.c,&m.plan,execute,&m)&&m.calls==at);}assert(!pthread_mutex_lock(&m.lock));m.stop=true;assert(!pthread_cond_signal(&m.changed));assert(!pthread_mutex_unlock(&m.lock));assert(!pthread_join(t,NULL));assert(!pthread_cond_destroy(&m.changed));assert(!pthread_mutex_destroy(&m.lock));}
int main(void){one(0,0);for(unsigned at=1;at<=40;at++)for(unsigned mode=0;mode<8;mode++)one(at,mode);puts("PASS independent N46B 321 real two-thread controller executions, physical operations modeled, exact CPU/root barriers and uncertain failure retention");return 0;}
