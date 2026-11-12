#include "../linux_apic_msr.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct guest_apic apic;static struct lams_state tx;static uint64_t g[15];
static unsigned writes,notifies,effects;static bool failwrite,failnotify,reenter;static uint64_t lastdomain,lastseq;
static bool wr(void*c,uint32_t f,uint64_t v){(void)c;assert(f==0x681e&&v==0x1002);writes++;if(reenter)assert(lams_retry(&tx)==LAMS_REJECTED);return !failwrite;}
static bool notify(void*c,uint64_t domain,uint64_t sequence,uint8_t v){(void)c;assert(v==0x44);notifies++;
 if(domain!=lastdomain||sequence!=lastseq){effects++;lastdomain=domain;lastseq=sequence;}
 if(reenter)assert(lams_retry(&tx)==LAMS_REJECTED);
 return !failnotify;
}
static void setup(bool x2){memset(&tx,0,sizeof tx);memset(g,0,sizeof g);guest_apic_init_mode(&apic,0,x2);writes=notifies=effects=0;failwrite=failnotify=reenter=false;lastdomain=lastseq=0;assert(lams_init(&tx,&apic,42,0,notify));}
static enum lams_result run(unsigned reason,uint32_t id,uint64_t val,uint64_t now){g[2]=0xdead00000000ull|id;g[0]=(uint32_t)val;g[3]=(uint32_t)(val>>32);return lams_apply(&tx,g,0x1000,0,reason,2,now,1u<<21,1u<<9,true,0,wr);}
static void active_level(void){struct guest_apic_effect e;assert(guest_apic_write(&apic,0x80f,0x1ff,0,&e));assert(guest_apic_enqueue(&apic,0x44,true)&&guest_apic_accept(&apic,0x44));}
int main(void){
 setup(false);assert(run(31,0x1b,UINT64_MAX,0)==LAMS_COMMITTED&&g[0]==0xfee00900&&g[3]==0);
 assert(run(31,0x803,0,0)==LAMS_REJECTED);assert(run(32,0x1b,0xfee00d00,0)==LAMS_COMMITTED&&apic.x2apic);
 assert(run(31,0x803,UINT64_MAX,1)==LAMS_COMMITTED&&g[0]==0x60014&&!g[3]);
 setup(true);active_level();struct guest_apic old=apic;failwrite=true;
 assert(run(32,0x80b,0,0)==LAMS_FATAL&&tx.failed&&!memcmp(&old,&apic,sizeof old)&&!notifies);assert(run(31,0x803,0,0)==LAMS_REJECTED);
 setup(true);active_level();failnotify=reenter=true;
 assert(run(32,0x80b,0,0)==LAMS_EFFECT_PENDING&&tx.pending&&tx.sequence==1&&effects==1);
 assert(!(apic.irq.isr[2]&(1u<<4)));old=apic;unsigned w=writes;
 assert(run(32,0x80b,0,0)==LAMS_REJECTED&&writes==w);
 assert(lams_retry(&tx)==LAMS_EFFECT_PENDING&&effects==1&&!memcmp(&apic,&old,sizeof old));
 failnotify=false;assert(lams_retry(&tx)==LAMS_COMMITTED&&!tx.pending&&effects==1&&notifies==3&&writes==w);
 assert(lams_retry(&tx)==LAMS_COMMITTED&&notifies==3);
 setup(true);struct guest_apic_effect e;assert(guest_apic_write(&apic,0x80f,0x1ff,0,&e));assert(guest_apic_enqueue(&apic,0x44,false)&&guest_apic_accept(&apic,0x44));assert(run(32,0x80b,0,0)==LAMS_COMMITTED&&!notifies&&!tx.sequence);
 setup(true);active_level();tx.sequence=UINT64_MAX;old=apic;assert(run(32,0x80b,0,0)==LAMS_REJECTED&&!writes&&!memcmp(&old,&apic,sizeof old));
 setup(true);assert(run(32,0x80f,0x1ff,0)==LAMS_COMMITTED);assert(run(32,0x832,0x45,0)==LAMS_COMMITTED);assert(run(32,0x838,1,0)==LAMS_COMMITTED);
 uint64_t deadline=apic.timer.deadline;assert(deadline);assert(run(31,0x803,0,deadline)==LAMS_COMMITTED&&(apic.irq.irr[2]&(1u<<5)));
 old=apic;assert(run(31,0x803,0,deadline-1)==LAMS_REJECTED&&!memcmp(&old,&apic,sizeof old));
 setup(true);g[2]=0x803;old=apic;
 for(unsigned c=1;c<=3;c++)assert(lams_apply(&tx,g,0x1000,c,31,2,0,1u<<21,1u<<9,true,0,wr)==LAMS_REJECTED);
 for(unsigned len=0;len<=16;len++)if(len!=2)assert(lams_apply(&tx,g,0x1000,0,31,len,0,1u<<21,1u<<9,true,0,wr)==LAMS_REJECTED);
 assert(lams_apply(&tx,g,0x1000,0,31,2,0,0,1u<<9,true,0,wr)==LAMS_REJECTED);
 assert(lams_apply(&tx,g,0x1000,0,31,2,0,1u<<21,0,true,0,wr)==LAMS_REJECTED);
 assert(lams_apply(&tx,g,0x1000,0,31,2,0,1u<<21,1u<<9,false,0,wr)==LAMS_REJECTED);
 assert(lams_apply(&tx,g,0x7fffffffffffull,0,31,2,0,1u<<21,1u<<9,true,0,wr)==LAMS_REJECTED);
 assert(lams_apply(&tx,g,UINT64_MAX,0,31,2,0,1u<<21,1u<<9,true,0,wr)==LAMS_REJECTED);
 assert(run(31,0x480,0,0)==LAMS_REJECTED);assert(run(32,0x803,0,0)==LAMS_REJECTED);assert(!writes&&!memcmp(&old,&apic,sizeof old));
 puts("linux_apic_msr: staged model/GPR/RIP, token-dedup effects, timer and rejection PASS (host only)");
}
