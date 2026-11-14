#include <assert.h>
#include <stdio.h>
#include <string.h>
#define ECG_HOST_TEST
#include ECG_SOURCE
static struct table_ptr observed_idt,observed_gdt;
static UINT16 observed_cs,observed_ss,observed_tr;
static UINT64 observed_flags,observed_apic;
static unsigned marker,calls,flip_at;
static int ap_started;
void ecg_test_capture(struct table_ptr *i,struct table_ptr *g,UINT16 *cs,UINT16 *ss,UINT16 *tr,UINT64 *f,UINT64 *apic){*i=observed_idt;*g=observed_gdt;*cs=observed_cs;*ss=observed_ss;*tr=observed_tr;*f=observed_flags;*apic=observed_apic;}
unsigned exceptions_fatal_context(void){calls++;return flip_at&&calls>=flip_at?1:marker;}
int paging_ap_start_attempted(void){return ap_started;}
void exception_2(void){}
void *exception_stubs[256];
void ecg_test_cli_cld(void){observed_flags=2;}
void load_gdt_segments(const struct table_ptr *g){observed_gdt=*g;observed_cs=8;observed_ss=16;gdt[1]|=(UINT64)1<<40;gdt[2]|=(UINT64)1<<40;}
void ecg_test_ltr(UINT16 tr){assert(tr==0x18&&((gdt[3]>>40)&255)==0x89);observed_tr=tr;gdt[3]|=(UINT64)1<<41;}
void ecg_test_lidt(const struct table_ptr *i){observed_idt=*i;}
static void bootstrap(void){
 memset(idt,0,sizeof idt);memset(gdt,0,sizeof gdt);memset(&tss,0,sizeof tss);panicking=0;marker=0;calls=0;flip_at=0;ap_started=0;
 observed_cs=0;observed_ss=0;observed_tr=0;observed_apic=(UINT64)1<<8;
 for(unsigned i=0;i<256;i++)exception_stubs[i]=(void*)exception_2;
 exceptions_init();assert(exceptions_owned_context());
 exceptions_user_setup(0x4000,exception_2);assert(tss.rsp[0]==0x4000&&exceptions_owned_context());
}
static void reset(void){
 memset(idt,0,sizeof idt);memset(gdt,0,sizeof gdt);memset(&tss,0,sizeof tss);panicking=0;marker=0;calls=0;flip_at=0;ap_started=0;
 observed_idt=(struct table_ptr){sizeof idt-1,(UINTN)idt};observed_gdt=(struct table_ptr){sizeof gdt-1,(UINTN)gdt};observed_cs=8;observed_ss=16;observed_tr=0x18;observed_flags=2;observed_apic=(UINT64)1<<8;
 UINT64 addr=(UINTN)exception_2;idt[2]=(struct gate){addr&0xffff,8,3,0x8e,(addr>>16)&0xffff,addr>>32,0};
 UINT64 base=(UINTN)&tss;gdt[3]=(sizeof(tss)-1)|((base&0xffffff)<<16)|((UINT64)0x8b<<40)|(((base>>24)&255)<<56);gdt[4]=base>>32;gdt[1]=0x00af9b000000ffffULL;gdt[2]=0x00cf93000000ffffULL;tss.ist[2]=(UINTN)(nmi_stack+sizeof nmi_stack);tss.iomap=sizeof tss;
}
#define BAD(change) do{reset();change;assert(!exceptions_owned_context());}while(0)
#define main lvh_fixture_main
#define reset lvh_fixture_reset
#include "../../krnl/tests/linux_vcpu_host.c"
#undef main
#undef reset
#include "../../krnl/linux_vcpu_backend.h"
static bool lifecycle_capture(void *ctx,struct lvh_host *h){
 if(!capture(ctx,h))return false;
 h->gdt_base=(UINTN)gdt;h->gdt_limit=sizeof(gdt)-1;h->idt_base=(UINTN)idt;h->idt_limit=sizeof(idt)-1;
 h->cs=8;h->ss=h->ds=h->es=16;h->fs=h->gs=0;h->tr=0x18;
 assert(lvb_descriptor((const uint8_t*)gdt,sizeof(gdt),sizeof(gdt)-1,h->tr,&h->tr_access,&h->tr_limit,&h->tr_base));
 assert(h->tr_base==(UINTN)&tss&&h->tr_access==0x8b&&h->tr_limit==sizeof(tss)-1);
 return true;
}
static void lifecycle_vmcs(void){
 assert(!lvh_fixture_main());lvh_fixture_reset();bootstrap();ops.capture=lifecycle_capture;
 assert(lvh_open(&c,&ops,&ram,0x80000,0x400000));
 assert(value(0xc02)==8&&value(0xc04)==16&&value(0xc0c)==0x18);
 assert(value(0x6c0a)==(UINTN)&tss&&value(0x6c0c)==(UINTN)gdt&&value(0x6c0e)==(UINTN)idt);
 assert(lvh_close(&c));
 puts("actual bootstrap table construction + actual descriptor parser + actual lvh VMCS host-field writer PASS; simulated loads/VMX");
}
int main(void){lifecycle_vmcs();bootstrap();reset();assert(exceptions_owned_context()&&calls==2);
 BAD(observed_cs=0x2b);BAD(observed_cs=16);BAD(observed_ss=24);BAD(observed_flags|=0x200);BAD(observed_flags|=0x400);BAD(observed_apic=0);BAD(ap_started=1);BAD(marker=1);BAD(marker=2);BAD(panicking=1);
 BAD(observed_idt.base++);BAD(observed_idt.limit--);BAD(observed_gdt.base++);BAD(observed_gdt.limit--);BAD(observed_tr=0x1b);BAD(observed_tr=0x1c);
 BAD(idt[2].offset_low^=1);BAD(idt[2].selector=16);BAD(idt[2].ist=0);BAD(idt[2].flags=0xee);BAD(idt[2].reserved=1);
 BAD(gdt[3]^=(UINT64)1<<41);BAD(gdt[3]^=1);BAD(gdt[4]^=1);BAD(gdt[1]^=(UINT64)1<<53);BAD(gdt[2]^=(UINT64)1<<45);BAD(tss.ist[2]-=16);BAD(tss.iomap--);BAD(flip_at=2);
 reset();gdt[1]&=~((UINT64)1<<40);gdt[2]&=~((UINT64)1<<40);assert(exceptions_owned_context());
 puts("actual private IDT/GDT/TSS context gate candidate: descriptor/CPU/marker/refusal matrix PASS; synthetic captures, no native admission");return 0;
}
