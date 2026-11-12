#include "../device_guest.h"
#include "../mmio_decode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t regs[16];
static void valid(const uint8_t *bytes,size_t n,uint64_t rip,
                   uint64_t ea,unsigned length,unsigned reg,bool write) {
    struct mmio_mov32 out={0};
    assert(mmio_decode_mov32(bytes,n,rip,regs,&out));
    assert(out.ea==ea && out.length==length && out.reg==reg && out.write==write);
}
static void invalid(const uint8_t *bytes,size_t n,uint64_t rip) {
    struct mmio_mov32 out={.ea=123,.length=99,.reg=88,.write=true},before;
    memcpy(&before,&out,sizeof out);
    assert(!mmio_decode_mov32(bytes,n,rip,regs,&out));
    assert(memcmp(&before,&out,sizeof out)==0);
}
int main(void) {
    assert(device_mmio_state_supported(0,0x202,0));
    assert(!device_mmio_state_supported(1,0x202,0));
    assert(!device_mmio_state_supported(2,0x202,0));
    assert(!device_mmio_state_supported(0,0x302,0));
    assert(!device_mmio_state_supported(0,0x202,1));

    assert(device_guest_ram_gpa(0x1000));
    assert(device_guest_ram_gpa((EPT_GUEST_PAGES+1)*4096-1));
    assert(!device_guest_ram_gpa(0xfff));
    assert(!device_guest_ram_gpa((EPT_GUEST_PAGES+1)*4096));
    assert(!device_guest_ram_gpa(UINT64_C(0x100000001000)));
    assert(!device_guest_ram_gpa(UINT64_MAX));

    regs[0]=0x2000;regs[1]=3;regs[3]=0x3000;regs[4]=0x4000;
    regs[5]=0x5000;regs[8]=0x8000;regs[12]=7;regs[13]=0xd000;
    const uint8_t direct[]={0x89,0x18}; /* [rax] <- ebx */
    valid(direct,sizeof direct,0x1000,0x2000,2,3,true);
    const uint8_t load[]={0x8b,0x08}; /* ecx <- [rax] */
    valid(load,sizeof load,0x1000,0x2000,2,1,false);
    const uint8_t extended[]={0x45,0x89,0x30}; /* [r8] <- r14d */
    valid(extended,sizeof extended,0x1000,0x8000,3,14,true);
    const uint8_t disp8[]={0x8b,0x45,0xf0}; /* eax <- [rbp-16] */
    valid(disp8,sizeof disp8,0x1000,0x4ff0,3,0,false);
    const uint8_t r13[]={0x41,0x8b,0x45,0}; /* eax <- [r13+0] */
    valid(r13,sizeof r13,0x1000,0xd000,4,0,false);
    const uint8_t disp32[]={0x89,0x98,0x00,0x01,0,0};
    valid(disp32,sizeof disp32,0x1000,0x2100,6,3,true);
    const uint8_t negative32[]={0x8b,0x80,0,0xff,0xff,0xff};
    valid(negative32,sizeof negative32,0x1000,0x1f00,6,0,false);
    const uint8_t sib[]={0x8b,0x54,0x8b,0x10}; /* edx,[rbx+rcx*4+16] */
    valid(sib,sizeof sib,0x1000,0x301c,4,2,false);
    const uint8_t rsp[]={0x89,0x04,0x24}; /* [rsp] <- eax, no index */
    valid(rsp,sizeof rsp,0x1000,0x4000,3,0,true);
    const uint8_t r12index[]={0x42,0x8b,0x04,0x63}; /* [rbx+r12*2] */
    valid(r12index,sizeof r12index,0x1000,0x300e,4,0,false);
    const uint8_t no_base[]={0x8b,0x04,0x8d,0,0x20,0,0};
    valid(no_base,sizeof no_base,0x1000,0x200c,7,0,false);
    const uint8_t no_base_rexb[]={0x41,0x8b,0x04,0x25,0,0x20,0,0};
    valid(no_base_rexb,sizeof no_base_rexb,0x1000,0x2000,8,0,false);
    const uint8_t absolute_negative[]={0x8b,0x04,0x25,0xf0,0xff,0xff,0xff};
    valid(absolute_negative,sizeof absolute_negative,0x1000,UINT64_MAX-15,7,0,false);
    const uint8_t negative_index[]={0x8b,0x04,0x0d,0xf0,0xff,0xff,0xff};
    valid(negative_index,sizeof negative_index,0x1000,UINT64_MAX-12,7,0,false);
    const uint8_t riprel[]={0x8b,0x05,0x10,0,0,0};
    valid(riprel,sizeof riprel,0x1000,0x1016,6,0,false);
    const uint8_t rip_rex[]={0x45,0x8b,0x3d,0xf9,0xff,0xff,0xff};
    valid(rip_rex,sizeof rip_rex,0x1000,0x1000,7,15,false);
    const uint8_t unused_tail[]={0x8b,0x08,0x48,0xff,0xff};
    valid(unused_tail,sizeof unused_tail,0x1000,0x2000,2,1,false);
    for(size_t n=0;n<sizeof no_base_rexb;n++) invalid(no_base_rexb,n,0x1000);
    for(size_t n=0;n<sizeof rip_rex;n++) invalid(rip_rex,n,0x1000);
    const uint8_t bad[][4]={
        {0x48,0x8b,0,0}, {0x66,0x8b,0,0}, {0x67,0x8b,0,0},
        {0xf0,0x89,0,0}, {0xf2,0x8b,0,0}, {0xf3,0x8b,0,0},
        {0x2e,0x8b,0,0}, {0x64,0x8b,0,0}, {0x65,0x8b,0,0},
        {0x40,0x41,0x8b,0}, {0x8b,0xc0,0,0}, {0x88,0,0,0},
        {0x8a,0,0,0}, {0xc7,0,0,0}, {0x0f,0xb6,0,0}
    };
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) invalid(bad[i],4,0x1000);
    regs[0]=0x2ffc;valid(load,2,0x1000,0x2ffc,2,1,false);
    regs[0]=0x2ffd;invalid(load,2,0x1000);
    regs[0]=UINT64_MAX;invalid(load,2,0x1000);
    regs[0]=UINT64_C(0x0000800000000000);invalid(load,2,0x1000);
    regs[0]=UINT64_C(0xffff7fffffffffff);invalid(load,2,0x1000);
    regs[0]=UINT64_C(0xffff800000000000);
    valid(load,2,0x1000,regs[0],2,1,false);
    invalid(load,2,UINT64_C(0x800000000000));
    invalid(load,2,UINT64_MAX-1);
    invalid(load,2,UINT64_C(0x7ffffffffffe));
    regs[0]=0;invalid(negative32,sizeof negative32,0x1000); /* underflow */
    regs[0]=UINT64_MAX-0x80;invalid(disp32,sizeof disp32,0x1000);
    regs[1]=UINT64_MAX;invalid(sib,sizeof sib,0x1000); /* scaled-index overflow */
    regs[1]=UINT64_MAX/4;regs[3]=0x100;invalid(sib,sizeof sib,0x1000); /* base+index */
    struct mmio_mov32 out;
    assert(!mmio_decode_mov32(NULL,2,0x1000,regs,&out));
    assert(!mmio_decode_mov32(load,2,0x1000,NULL,&out));
    assert(!mmio_decode_mov32(load,2,0x1000,regs,NULL));
    puts("PASS restricted MOV32 MMIO decoder and negative/addressing cases");
}
