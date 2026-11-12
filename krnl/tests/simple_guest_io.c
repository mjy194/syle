#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../simple_guest.h"
#include "../guest_uart.h"
static struct sg_image image={.entry=0x10000,.count=1,.segments={{.address=0x10000,.filesz=16,.memsz=4096,.flags=5}}};
static struct guest_uart uart;
static struct vmx_guest_registers regs;
static struct sg_run_state state;
static uint64_t next;
static uint8_t character;
static uint64_t q(unsigned port,bool in) {return ((uint64_t)port<<16)|(in?8:0);}
static void reset(void) {
    guest_uart_init(&uart);
    uint32_t config=3;bool emitted=false;uint8_t byte=0;
    assert(guest_uart_access(&uart,3,1,true,&config,&emitted,&byte));
    state=(struct sg_run_state){0};
    regs=(struct vmx_guest_registers){.rax=UINT64_C(0x123456789abcde41),.rdx=0x3f8,.r15=0x5678};
    next=0xdead;character=0xa5;
}
static enum sg_action call(uint64_t qualification,uint64_t rip,uint64_t length) {
    return sg_dispatch_io(&state,&image,&uart,qualification,rip,length,&regs,&next,&character);
}
static void reject(uint64_t qualification,uint64_t rip,uint64_t length) {
    struct guest_uart old=uart;struct vmx_guest_registers r=regs;
    uint64_t n=next;uint8_t c=character;unsigned exits=state.exits,chars=state.characters;
    assert(call(qualification,rip,length)==SG_REJECT);
    assert(!memcmp(&old,&uart,sizeof uart)&&!memcmp(&r,&regs,sizeof regs));
    assert(next==n&&character==c&&state.exits==exits&&state.characters==chars);
}
int main(void) {
    reset();uint64_t original=regs.rax;
    assert(call(q(0x3f8,false),0x10000,1)==SG_PUTCHAR);
    assert(character=='A'&&next==0x10001&&regs.rax==original&&state.exits==1&&state.characters==1);
    regs.rdx=UINT64_C(0x12345678000003fd);character=0xa5;
    assert(call(q(0x3fd,true),0x1000f,1)==SG_RESUME);
    assert(regs.rax==((original&~UINT64_C(0xff))|0x60));
    assert(regs.r15==0x5678&&next==0x10010&&character==0xa5&&state.characters==1);
    /* Every forbidden qualification bit individually, including reserved upper bits. */
    for(unsigned bit=0;bit<64;bit++) if(bit!=3 && !(bit>=16&&bit<=31)) {
        reset();reject(q(0x3f8,false)|(UINT64_C(1)<<bit),0x10000,1);assert(state.failed);
        reject(q(0x3f8,false),0x10000,1);
    }
    for(unsigned size=1;size<8;size++) {reset();reject(q(0x3f8,false)|size,0x10000,1);}
    const unsigned ports[]={0,0x80,0x3f7,0x400,0xffff};
    for(unsigned i=0;i<sizeof ports/sizeof ports[0];i++) {
        reset();regs.rdx=ports[i];reject(q(ports[i],false),0x10000,1);assert(state.failed);
    }
    reset();regs.rdx=0x3f9;reject(q(0x3f8,false),0x10000,1);
    for(unsigned length=0;length<=16;length++) if(length!=1) {
        reset();reject(q(0x3f8,false),0x10000,length);
    }
    const uint64_t rips[]={0xffff,0x10010,0x10100,UINT64_MAX};
    for(unsigned i=0;i<sizeof rips/sizeof rips[0];i++) {reset();reject(q(0x3f8,false),rips[i],1);}
    reset();image.segments[0].flags=4;reject(q(0x3f8,false),0x10000,1);image.segments[0].flags=5;
    reset();state.finished=true;reject(q(0x3f8,false),0x10000,1);assert(!state.failed);
    reset();state.failed=true;reject(q(0x3f8,false),0x10000,1);
    reset();state.exits=SG_EXIT_LIMIT;reject(q(0x3f8,false),0x10000,1);assert(state.failed);
    reset();state.characters=SG_EXIT_LIMIT;reject(q(0x3f8,false),0x10000,1);assert(state.failed);
    /* Model rejects writes to read-only LSR; router preserves all state. */
    reset();regs.rdx=0x3fd;reject(q(0x3fd,false),0x10000,1);assert(state.failed);
    /* A non-emitting register write advances without changing caller byte. */
    reset();regs.rdx=0x3fb;regs.rax=3;
    assert(call(q(0x3fb,false),0x10000,1)==SG_RESUME);
    assert(character==0xa5&&state.characters==0&&state.exits==1&&regs.rax==3);
    /* Reads remain possible when output budget alone is exhausted. */
    reset();state.characters=SG_EXIT_LIMIT;regs.rdx=0x3fd;
    assert(call(q(0x3fd,true),0x10000,1)==SG_RESUME&&state.characters==SG_EXIT_LIMIT);
    /* Exit and character counts are shared with CPUID and VMCALL. */
    reset();state.exits=SG_EXIT_LIMIT-2;regs.rax=0;regs.rcx=0;
    assert(sg_dispatch(&state,&image,10,0x10000,2,&regs,&next,&character)==SG_RESUME);
    regs.rax='Z';regs.rdx=0x3f8;assert(call(q(0x3f8,false),0x10002,1)==SG_PUTCHAR);
    assert(state.exits==SG_EXIT_LIMIT&&state.characters==1);reject(q(0x3f8,false),0x10003,1);
    reset();state.characters=SG_EXIT_LIMIT-1;regs.rax=1;regs.rbx='V';
    assert(sg_dispatch(&state,&image,18,0x10000,3,&regs,&next,&character)==SG_PUTCHAR);
    regs.rax='U';reject(q(0x3f8,false),0x10003,1);
    puts("simple_guest_io: PASS (strict qualification, atomic rejection, spans, shared budgets)");
}
