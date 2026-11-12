#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../simple_guest.h"
static struct sg_image image={.entry=0x10000,.count=1,.segments={{.address=0x10000,.filesz=0x1000,.memsz=0x1000,.flags=5}}};
static struct vmx_guest_registers regs;
static uint64_t next;
static uint8_t character;
static void reject(struct sg_run_state *s,uint64_t reason,uint64_t rip,uint64_t length) {
    struct vmx_guest_registers old=regs;
    next=0xdeadbeef;character=0xa5;
    assert(sg_dispatch(s,&image,reason,rip,length,&regs,&next,&character)==SG_REJECT);
    assert(!memcmp(&old,&regs,sizeof regs));assert(next==0xdeadbeef&&character==0xa5);
}
int main(void) {
    struct sg_run_state s={0};
    regs=(struct vmx_guest_registers){.rax=1,.rbx=0x141,.r15=0x1234};
    assert(sg_dispatch(&s,&image,18,0x10100,3,&regs,&next,&character)==SG_PUTCHAR);
    assert(next==0x10103&&character=='A'&&s.exits==1&&s.characters==1&&regs.r15==0x1234);
    regs.rax=0;regs.rcx=0;
    assert(sg_dispatch(&s,&image,10,0x10200,2,&regs,&next,&character)==SG_RESUME);
    assert(regs.rax==1&&regs.rbx==0x656c7953&&regs.r15==0x1234&&next==0x10202);
    regs.rax=2;regs.rbx=0;next=7;character=8;
    assert(sg_dispatch(&s,&image,18,0x10300,3,&regs,&next,&character)==SG_FINISH);
    assert(s.finished&&!s.failed&&s.exit_code==0&&next==7&&character==8);
    reject(&s,10,0x10000,2);assert(!s.failed);
    s=(struct sg_run_state){0};regs.rax=2;regs.rbx=42;
    assert(sg_dispatch(&s,&image,18,0x10000,3,&regs,&next,&character)==SG_FINISH);
    assert(s.finished&&s.failed&&s.exit_code==42);
    const uint64_t reasons[]={0,1,2,12,28,29,30,31,32,48,49,52,UINT64_C(0x8000000a),UINT64_MAX};
    for(unsigned i=0;i<sizeof reasons/sizeof reasons[0];i++) {
        s=(struct sg_run_state){0};reject(&s,reasons[i],0x10000,2);assert(s.failed&&s.exits==0);
        reject(&s,10,0x10000,2);
    }
    for(unsigned len=0;len<17;len++) if(len!=2) {
        s=(struct sg_run_state){0};reject(&s,10,0x10000,len);assert(s.failed);
    }
    for(unsigned len=0;len<17;len++) if(len!=3) {
        s=(struct sg_run_state){0};regs.rax=1;reject(&s,18,0x10000,len);assert(s.failed);
    }
    const uint64_t bad_rips[]={0,0xffff,0x10fff,0x11000,UINT64_MAX-1,UINT64_MAX};
    for(unsigned i=0;i<sizeof bad_rips/sizeof bad_rips[0];i++) {
        s=(struct sg_run_state){0};reject(&s,10,bad_rips[i],2);assert(s.failed);
    }
    s=(struct sg_run_state){0};regs.rax=3;reject(&s,18,0x10000,3);assert(s.failed);
    s=(struct sg_run_state){0};
    for(unsigned i=0;i<SG_EXIT_LIMIT;i++) {
        regs.rax=1;regs.rbx=i;
        assert(sg_dispatch(&s,&image,18,0x10000,3,&regs,&next,&character)==SG_PUTCHAR);
    }
    assert(s.exits==SG_EXIT_LIMIT&&s.characters==SG_EXIT_LIMIT);
    reject(&s,18,0x10000,3);assert(s.failed);
    s=(struct sg_run_state){.characters=SG_EXIT_LIMIT};regs.rax=1;
    reject(&s,18,0x10000,3);assert(s.failed&&s.exits==0);
    s=(struct sg_run_state){.exits=SG_EXIT_LIMIT-1};regs.rax=2;regs.rbx=0;
    assert(sg_dispatch(&s,&image,18,0x10ffd,3,&regs,&next,&character)==SG_FINISH);
    assert(s.finished&&!s.failed&&s.exits==SG_EXIT_LIMIT);
    s=(struct sg_run_state){0};image.segments[0].flags=4;
    reject(&s,10,0x10000,2);assert(s.failed);
    puts("simple_guest_dispatch: PASS (lifecycle, budgets, unsupported exits, atomic rejection)");
}
