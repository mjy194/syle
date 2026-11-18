#include "../firmware_boot_gp.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t ram[65536];static uint64_t deny=UINT64_MAX;static enum fmm_access deny_access;
bool fmm_translate(const struct fmm_memory*m,uint64_t a,enum fmm_access access,uint64_t*p){(void)m;if(a>=sizeof ram||(a==deny&&access==deny_access))return false;*p=a;return true;}
bool fwm_read_ram(struct firmware_memory*m,uint64_t a,void*out,size_t n){(void)m;if(a>=sizeof ram||n>sizeof ram-a)return false;memcpy(out,ram+a,n);return true;}
struct sink{unsigned n,fail;uint32_t f[4];uint64_t v[4];};
static bool write(void*x,uint32_t f,uint64_t v){struct sink*s=x;assert(s->n<4);s->f[s->n]=f;s->v[s->n++]=v;return s->n!=s->fail;}
static void put(uint64_t a,uint64_t v){memcpy(ram+a,&v,8);}
int main(void){struct fmm_memory m={.cr3=0x1000,.cs_selector=8};struct fbg_exit e={.rip=0x1000,.rsp=0x8000,.idtr=0x2000,.idt_limit=4095,.gdtr=0x3000,.gdt_limit=255,.flags=2};
 ram[0x1000]=15;ram[0x1001]=0x32;put(0x3008,0x00af9b000000ffffULL);put(0x20d0,0x00008e0000084000ULL);put(0x20d8,0);
 struct fbg_state d={0};struct sink s={0};assert(fbg_inject(&d,&m,&e,&s,write));assert(d.count==1&&d.pending&&s.n==4&&s.f[0]==0x6820&&s.v[0]==0x10002&&s.f[3]==0x4016&&s.v[3]==0x80000b0d);
 assert(!fbg_inject(&d,&m,&e,&s,write));s=(struct sink){0};assert(fbg_after(&d,30,0,0,&s,write)&&!d.pending&&s.n==3);assert(!fbg_inject(&d,&m,&e,&s,write));
 for(unsigned i=0;i<4;i++){d=(struct fbg_state){0};s=(struct sink){.fail=i+1};assert(!fbg_inject(&d,&m,&e,&s,write)&&d.failed&&!d.pending);}
 d=(struct fbg_state){0};s=(struct sink){0};deny=0x7fd0;deny_access=FMM_WRITE;assert(!fbg_inject(&d,&m,&e,&s,write)&&!s.n);deny=0x4000;deny_access=FMM_EXEC;assert(!fbg_inject(&d,&m,&e,&s,write));deny=UINT64_MAX;
 put(0x20d0,0x00008f0000084000ULL);assert(!fbg_inject(&d,&m,&e,&s,write));put(0x20d0,0x00008e0000084000ULL);put(0x20d8,1ULL<<32);assert(!fbg_inject(&d,&m,&e,&s,write));put(0x20d8,0);
 m.cs_selector=11;assert(!fbg_inject(&d,&m,&e,&s,write));m.cs_selector=8;e.flags=0x102;assert(!fbg_inject(&d,&m,&e,&s,write));e.flags=2;
 for(unsigned i=0;i<3;i++){d=(struct fbg_state){.pending=true,.count=1};s=(struct sink){0};assert(!fbg_after(&d,i==0?52:i==1?48:0,0,0,&s,write)&&d.failed&&!s.n);}
 d=(struct fbg_state){.pending=true,.count=1};assert(!fbg_after(&d,30,0x80000000,0,&s,write));
 puts("#GP owned gate/handler/frame, same-RIP guard, event receipt and partial writes PASS");}
