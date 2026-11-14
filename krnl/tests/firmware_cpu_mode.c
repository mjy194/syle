#include "../firmware_cpu_mode.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define RAM (128ull*1024*1024)
static void no(const struct fcm_state*s,enum fcm_register r,uint64_t v){struct fcm_state x,old;memset(&x,0x55,sizeof x);old=x;assert(!fcm_write(s,RAM,r,v,&x));assert(!memcmp(&x,&old,sizeof x));}
int main(void){
 struct fcm_state s={.cr0=0x60000010},ovmf;
 assert(fcm_write(&s,RAM,FCM_CR0,0x23,&ovmf));
 assert(ovmf.cr0==0x33&&!ovmf.ia32e&&ovmf.efer==0);
 assert(fcm_write(&ovmf,RAM,FCM_CR4,0x640,&ovmf)&&ovmf.cr4==0x640);
 assert(fcm_write(&ovmf,RAM,FCM_CR4,0x660,&ovmf)&&ovmf.cr4==0x660);
 assert(fcm_write(&ovmf,RAM,FCM_CR4,0x600,&ovmf)&&ovmf.cr4==0x600);
 no(&ovmf,FCM_CR4,0x680);no(&ovmf,FCM_CR4,0x2640);
 assert(fcm_write(&ovmf,RAM,FCM_CR0,0,&ovmf)&&ovmf.cr0==0x10);
 no(&s,FCM_CR0,0x80000000);no(&s,FCM_CR0,0x20000000);
 no(&s,FCM_CR0,1ull<<63);
 no(&s,FCM_CR0,0x80000010);no(&s,FCM_EFER,0x400);no(&s,FCM_CR3,RAM);no(&s,FCM_CR3,0x1001);
 assert(fcm_write(&s,RAM,FCM_CR0,0x60000011,&s));
 assert(fcm_write(&s,RAM,FCM_CR4,0x20,&s));
 assert(fcm_write(&s,RAM,FCM_CR3,0x1000,&s));
 assert(fcm_write(&s,RAM,FCM_EFER,0x100,&s));
 assert(!s.ia32e);assert(fcm_write(&s,RAM,FCM_CR0,0x80000011,&s));
 assert(s.ia32e&&s.efer==0x500);
 no(&s,FCM_EFER,0x400);no(&s,FCM_CR4,0);no(&s,FCM_CR0,0x80000010);
 no(&s,FCM_CR0,0x11);
 s=(struct fcm_state){.cr0=0x10};
 no(&s,FCM_CR0,0x20000010);assert(fcm_write(&s,RAM,FCM_CR0,0,&s)&&s.cr0==0x10);no(&s,FCM_CR4,0x2000);
 no(&s,FCM_EFER,1ull<<63);no(&s,(enum fcm_register)99,0);
 assert(!fcm_write(0,RAM,FCM_CR0,0,&s));assert(!fcm_write(&s,RAM,FCM_CR0,0,0));
 s.efer=0x100;s.cr4=0;no(&s,FCM_CR0,0x80000011);
 puts("firmware_cpu_mode: tiny reset/PE/PAE/LME/PG/LMA and rejection PASS (host only)");
}
