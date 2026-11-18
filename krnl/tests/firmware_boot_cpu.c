#include "../firmware_boot_cpu.h"
#include "../firmware_cpu_mode.h"
#include <assert.h>
#include <stdio.h>
int main(void){assert(fboot_preemption_resume(52,0x33,0x202,0,0,0x400,0,0));assert(!fboot_preemption_resume(52,0x31,0x202,0,0,0x400,0,0));assert(!fboot_preemption_resume(52,0x33,0x202,0,0,0x400,0x80000020,0));

 struct fcp_host h={.basic_edx=0x0780a3fd,.extended_edx=0x20000800,.max_basic=1,.max_extended=0x80000008,.physical_bits=36,.virtual_bits=48};
 struct fcp_result r;assert(fboot_query(&h,true,1,0,&r)&&(r.d&0x2008)==0x2008);
 assert(fboot_query(&h,true,0,0,&r)&&r.a==0x15);
 assert(fboot_query(&h,true,0x15,0,&r)&&r.a==1&&r.b==1&&r.c==10000000);
 assert(fboot_query(&h,true,7,0,&r)&&!r.a&&!r.b&&!r.c&&!r.d);
 assert(fboot_query(&h,true,0x80000001,0,&r)&&(r.d&0x20000800)==0x20000800);
 h.extended_edx&=~0x800u;assert(!fboot_query(&h,true,1,0,&r));h.extended_edx|=0x800;
 assert(!fboot_query(&h,false,1,0,&r));assert(fboot_efer_allowed(0x501)&&!fboot_efer_allowed(0xd01));
 struct fcm_state s={.cr0=0x80010033,.cr3=0x1000,.cr4=0x660,.efer=0x500,.ia32e=true},n;
 assert(fcm_write(&s,128u<<20,FCM_CR4,0x30,&n)&&n.cr4==0x30);
 assert(fcm_write(&n,128u<<20,FCM_CR4,0xb0,&s)&&s.cr4==0xb0);
 assert(fcm_write(&s,128u<<20,FCM_EFER,0x501,&n)&&n.efer==0x501);
 assert(!fcm_write(&n,128u<<20,FCM_CR4,0x1030,&s));
 const uint8_t cr[]={0x0f,0x22,0xe1};assert(fboot_cr_encoding(0x104,3,cr,3));
 assert(!fboot_cr_encoding(4,3,cr,3));assert(!fboot_cr_encoding(0x104,4,cr,3));
 puts("firmware/Linux fixed CPU PSE/PGE/SYSCALL exposure and CR4/EFER gates PASS");
}
