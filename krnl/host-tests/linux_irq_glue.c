#include "../linux_irq_glue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Adapter fault fixture: actual device adapter is separately tested. */
static bool available=true,accept_fail,cancel_fail;static unsigned accepts,cancels,writes,failwrite;
bool lia_peek(const struct lia*a,uint8_t*v){if(!a->enabled||a->reserved||a->quarantined||!available)return false;*v=0x44;return true;}
bool lia_reserve(struct lia*a,uint8_t*v,uint64_t*t){if(!lia_peek(a,v))return false;*t=1;a->token=1;a->vector=*v;a->reserved=true;return true;}
bool lia_accept(void*p,uint64_t t,uint8_t v){struct lia*a=p;accepts++;assert(a->reserved&&t==a->token&&v==a->vector);if(accept_fail)return false;a->reserved=false;a->token=0;return true;}
bool lia_cancel(struct lia*a,uint64_t t,uint8_t v){cancels++;assert(a->reserved&&t==a->token&&v==a->vector);if(cancel_fail)return false;a->reserved=false;a->token=0;return true;}
void lia_quarantine(struct lia*a){a->quarantined=true;}
static bool wr(void*p,uint32_t f,uint64_t v){(void)p;(void)f;(void)v;return ++writes!=failwrite;}
static struct lig_state s;static struct lia a;static struct lei_snapshot snap;
static void init(void){s=(struct lig_state){0};a=(struct lia){.enabled=true};snap=(struct lei_snapshot){.rflags=0x202};available=true;accept_fail=cancel_fail=false;accepts=cancels=writes=failwrite=0;}
int main(void){
 init();assert(lig_before(&s,&a,&snap,0,wr)&&a.reserved);assert(lig_after(&s,&a,LIG_EXIT,10,0,0,wr)&&accepts==1&&!a.reserved&&s.event.phase==LEI_IDLE);
 init();snap.rflags=2;assert(lig_before(&s,&a,&snap,0,wr)&&!a.reserved&&!writes);
 init();snap.pf_pending=true;assert(lig_before(&s,&a,&snap,0,wr)&&!a.reserved&&!writes);
 for(unsigned i=1;i<=3;i++){init();failwrite=i;assert(!lig_before(&s,&a,&snap,0,wr)&&s.failed&&a.reserved&&s.event.token==1&&!accepts);assert(!lig_before(&s,&a,&snap,0,wr));}
 init();assert(lig_before(&s,&a,&snap,0,wr));assert(lig_after(&s,&a,LIG_NO_ENTRY,0,0,0,wr)&&cancels==1&&!accepts&&!a.reserved);
 init();assert(lig_before(&s,&a,&snap,0,wr));assert(lig_after(&s,&a,LIG_EXIT,0x80000021,0,0,wr)&&cancels==1&&!accepts);
 const unsigned bad[]={52,0,48,99};for(unsigned i=0;i<4;i++){init();assert(lig_before(&s,&a,&snap,0,wr));assert(!lig_after(&s,&a,LIG_EXIT,bad[i],0,0,wr)&&s.failed&&a.reserved&&a.quarantined&&!accepts);}
 init();assert(lig_before(&s,&a,&snap,0,wr));assert(!lig_after(&s,&a,LIG_UNKNOWN,0,0,0,wr)&&a.quarantined);
 init();assert(lig_before(&s,&a,&snap,0,wr));assert(!lig_after(&s,&a,LIG_EXIT,10,0x80000044,0,wr)&&a.quarantined);
 for(unsigned i=4;i<=6;i++){init();assert(lig_before(&s,&a,&snap,0,wr));failwrite=i;assert(!lig_after(&s,&a,LIG_EXIT,10,0,0,wr)&&s.event.phase==LEI_DELIVERED&&a.reserved&&!accepts);}
 init();assert(lig_before(&s,&a,&snap,0,wr));accept_fail=true;assert(!lig_after(&s,&a,LIG_EXIT,10,0,0,wr)&&s.event.phase==LEI_DELIVERED&&s.event.cleaned&&a.reserved);assert(!lig_before(&s,&a,&snap,0,wr)&&accepts==1);
 init();assert(lig_before(&s,&a,&snap,0,wr));cancel_fail=true;assert(!lig_after(&s,&a,LIG_NO_ENTRY,0,0,0,wr)&&s.event.token==1&&a.reserved);
 puts("linux_irq_glue: reservation/receipt/quarantine/failure PASS (host fixture only)");
}
