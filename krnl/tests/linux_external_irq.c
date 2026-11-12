#include "../linux_external_irq.h"
#include <assert.h>
#include <stdio.h>
struct mock {int n,fail,accepts; bool accept_ok; uint32_t fields[4];};
static bool wr(void *p,uint32_t f,uint64_t v){struct mock*m=p;(void)v;m->fields[m->n++]=f;return m->n!=m->fail;}
static bool ac(void*p,uint64_t t,uint8_t v){struct mock*m=p;assert(t==7&&v==40);m->accepts++;return m->accept_ok;}
static struct lei_snapshot g={.rflags=0x202};
static void stage(struct lei_irq*s){struct mock m={0};lei_init(s);assert(lei_stage(s,40,7,&g,&m,wr));assert(m.fields[2]==0x4016);}
int main(void){struct lei_irq s;lei_init(&s);assert(lei_check(&s,31,&g)==LEI_REJECT);assert(lei_check(&s,256,&g)==LEI_REJECT);
 for(unsigned i=0;i<8;i++){struct lei_snapshot x=g;if(i==0)x.rflags=2;if(i==1)x.blocking=1;if(i==2)x.blocking=2;if(i==3)x.blocking=4;if(i==4)x.activity=1;if(i==5)x.entry_info=40;if(i==6)x.vectoring=0x80000020;if(i==7)x.pf_pending=true;assert(lei_check(&s,40,&x)==(i<3?LEI_DEFER_WINDOW:LEI_REJECT));}
 for(int f=1;f<=3;f++){struct mock m={.fail=f};lei_init(&s);assert(!lei_stage(&s,40,7,&g,&m,wr));assert(s.token==7&&!s.armed&&s.cancellable);assert(!lei_account(&s,&m,ac));m=(struct mock){0};assert(lei_cancel(&s,&m,wr));assert(s.phase==LEI_IDLE);}
 unsigned yes[]={10,28,30,31,32,12};for(unsigned i=0;i<6;i++){stage(&s);lei_observe(&s,false,true,yes[i],0);assert(s.phase==LEI_DELIVERED);struct mock m={0};assert(!lei_account(&s,&m,ac));assert(m.accepts==0);assert(lei_clear_delivered(&s,&m,wr));assert(!lei_account(&s,&m,ac));assert(s.phase==LEI_DELIVERED);assert(!lei_stage(&s,40,7,&g,&m,wr));m.accept_ok=true;assert(lei_account(&s,&m,ac));assert(m.accepts==2&&s.phase==LEI_IDLE);}
 unsigned no[]={0,1,48,52,7,0xffff};for(unsigned i=0;i<6;i++){stage(&s);lei_observe(&s,false,true,no[i],0);assert(s.phase==LEI_QUARANTINED&&s.token==7);}
 stage(&s);lei_observe(&s,false,true,10,0x80000028);assert(s.phase==LEI_QUARANTINED);
 for(int i=0;i<2;i++){stage(&s);lei_observe(&s,i==0,i!=0,i?0x80000021:0,0);assert(s.cancellable&&!s.armed&&s.token==7);struct mock m={.fail=1};assert(!lei_cancel(&s,&m,wr));assert(s.token==7);m=(struct mock){0};assert(lei_cancel(&s,&m,wr));}
 stage(&s);lei_observe(&s,true,true,10,0);assert(s.phase==LEI_QUARANTINED);
 for(int f=1;f<=3;f++){stage(&s);lei_observe(&s,false,true,10,0);struct mock m={.fail=f};assert(!lei_clear_delivered(&s,&m,wr));assert(!s.cleaned&&s.token==7&&s.phase==LEI_DELIVERED);assert(!lei_account(&s,&m,ac));}
 stage(&s);lei_observe(&s,false,true,0x80000021,0x80000028);assert(s.phase==LEI_QUARANTINED);
 stage(&s);lei_observe(&s,true,false,0,0x80000028);assert(s.phase==LEI_QUARANTINED);
 stage(&s);lei_observe(&s,false,true,0x80000029,0);assert(s.phase==LEI_QUARANTINED);
 puts("external IRQ host tests PASS");return 0;}
