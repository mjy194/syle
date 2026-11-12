#include "../nested_operand.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t g[16];
static struct nested_operand decode(const uint8_t *c,size_t n,uint64_t rip,unsigned reason) {
    struct nested_operand o;
    assert(nested_decode_operand(c,n,rip,g,reason,&o));
    assert(o.length==n);
    for(size_t i=0;i<n;i++) assert(!nested_decode_operand(c,i,rip,g,reason,&o));
    for(unsigned r=19;r<=27;r++)
        if(r!=reason) assert(!nested_decode_operand(c,n,rip,g,r,&o));
    return o;
}
static void reject(const uint8_t *c,size_t n,uint64_t rip,unsigned reason) {
    struct nested_operand a,b;
    memset(&a,0xa5,sizeof a); b=a;
    assert(!nested_decode_operand(c,n,rip,g,reason,&a));
    assert(!memcmp(&a,&b,sizeof a));
}
#define BAD(reason,...) do {const uint8_t c[]={__VA_ARGS__};reject(c,sizeof c,0x1000,reason);}while(0)
int main(void) {
    uint64_t mr[16]={0};mr[0]=0xc810;
    uint64_t mi=(UINT64_C(2)<<7)|(UINT64_C(3)<<15)|(UINT64_C(1)<<22);
    assert(nested_memory_exit_matches(mi,0,mr,0xc810));
    assert(!nested_memory_exit_matches(mi,0,mr,0xc818));
    assert(!nested_memory_exit_matches(mi&~(UINT64_C(7)<<7),0,mr,0xc810));
    assert(nested_memory_exit_matches(mi|(UINT64_C(1)<<27),0xc810,mr,0xc810));

    for(unsigned i=0;i<16;i++) g[i]=0x2000+i*0x1000;
    const uint8_t on[]={0xf3,0x0f,0xc7,0x30};
    const uint8_t clear[]={0x66,0x0f,0xc7,0x30};
    const uint8_t ld[]={0x0f,0xc7,0x30},st[]={0x0f,0xc7,0x38};
    struct nested_operand o=decode(on,sizeof on,0x1000,27);
    assert(o.op==NVMX_ON && o.memory && o.ea==g[0] && o.regA==255);
    assert(decode(clear,sizeof clear,0x1000,19).op==NVMX_CLEAR);
    assert(decode(ld,sizeof ld,0x1000,21).op==NVMX_PTRLD);
    assert(decode(st,sizeof st,0x1000,22).op==NVMX_PTRST);
    for(unsigned i=0;i<3;i++) {
        uint8_t c[]={0x0f,0x01,0xc2+i};
        o=decode(c,sizeof c,0x1000,(unsigned[]){20,24,26}[i]);
        assert(!o.memory && o.regA==255 && o.regB==255);
    }
    /* Exhaust all reg forms and REX bits: fixed width, no source reversal. */
    for(unsigned rex=0x40;rex<=0x4f;rex++)
    for(unsigned m=0xc0;m<=0xff;m++)
    for(unsigned op=0x78;op<=0x79;op++) {
        uint8_t c[]={rex,0x0f,op,m};
        o=decode(c,4,0x1000,op==0x78?23:25);
        assert(o.regA==((m&7)|((rex&1)?8:0)));
        assert(o.regB==(((m>>3)&7)|((rex&4)?8:0)));
        assert(o.op==(op==0x78?NVMX_READ:NVMX_WRITE));
    }
    const uint8_t rel[]={0xf3,0x41,0x0f,0xc7,0x35,0x10,0,0,0};
    assert(decode(rel,sizeof rel,0x1000,27).ea==0x1019);
    const uint8_t sib[]={0x43,0x0f,0xc7,0x74,0xa5,0xf8};
    assert(decode(sib,sizeof sib,0x1000,21).ea==g[13]+g[12]*4-8);
    const uint8_t abs[]={0x41,0x0f,0xc7,0x34,0x25,0,0x20,0,0};
    assert(decode(abs,sizeof abs,0x1000,21).ea==0x2000);
    const uint8_t neg[]={0x0f,0xc7,0x34,0x25,0,0xf0,0xff,0xff};
    assert(decode(neg,sizeof neg,0x1000,21).ea==UINT64_C(0xfffffffffffff000));
    const uint8_t disp[]={0x0f,0xc7,0xb0,0xf0,0xff,0xff,0xff};
    assert(decode(disp,sizeof disp,0x1000,21).ea==g[0]-16);
    BAD(21,0x67,0x0f,0xc7,0x30); BAD(21,0xf0,0x0f,0xc7,0x30);
    BAD(27,0xf3,0x66,0x0f,0xc7,0x30); BAD(27,0x41,0xf3,0x0f,0xc7,0x30);
    BAD(21,0x41,0x41,0x0f,0xc7,0x30); BAD(21,0x0f,0xc7,0xf0);
    BAD(21,0x0f,0xc7,0x28); BAD(22,0xf3,0x0f,0xc7,0x38);
    BAD(23,0x0f,0x78,0x00); BAD(25,0x0f,0x79,0x00);
    BAD(23,0x66,0x0f,0x78,0xc0); BAD(20,0xf3,0x0f,0x01,0xc2);
    BAD(20,0x0f,0x01,0xc1); BAD(21,0x0f,0x89,0x30);
    g[0]=0x2ff8; assert(decode(ld,sizeof ld,0x1000,21).ea==0x2ff8);
    g[0]=0x2ff9; reject(ld,sizeof ld,0x1000,21);
    g[0]=UINT64_C(0x0000800000000000);reject(ld,sizeof ld,0x1000,21);
    g[0]=UINT64_MAX;reject(ld,sizeof ld,0x1000,21);
    g[0]=0;reject(disp,sizeof disp,0x1000,21);
    g[12]=UINT64_MAX;reject(sib,sizeof sib,0x1000,21);
    g[12]=1;g[13]=UINT64_MAX-1;reject(sib,sizeof sib,0x1000,21);
    reject(ld,sizeof ld,UINT64_MAX,21);
    reject(ld,sizeof ld,UINT64_C(0x7fffffffffff),21);
    assert(!nested_decode_operand(NULL,0,0,g,21,&o));
    assert(!nested_decode_operand(ld,3,0,NULL,21,&o));
    assert(!nested_decode_operand(ld,3,0,g,21,NULL));
    /* INVEPT uses mandatory 66 and the three-byte opcode map. */
    for(unsigned rex=0x40;rex<=0x4f;rex++)
    for(unsigned reg=0;reg<8;reg++) {
        uint8_t c[]={0x66,rex,0x0f,0x38,0x80,(reg<<3)};
        g[0]=g[8]=0x2001; /* Unaligned is legal to decode. */
        o=decode(c,sizeof c,0x1000,50);
        assert(o.op==NVMX_INVEPT && o.memory && o.ea==0x2001);
        assert(o.regA==255 && o.regB==(reg|((rex&4)?8:0)));
    }
    const uint8_t ie[]={0x66,0x0f,0x38,0x80,0x08};
    g[0]=0x2ff0;assert(decode(ie,sizeof ie,0x1000,50).ea==0x2ff0);
    g[0]=0x2ff1;reject(ie,sizeof ie,0x1000,50);
    g[0]=UINT64_C(0x800000000000);reject(ie,sizeof ie,0x1000,50);
    g[0]=UINT64_MAX-7;reject(ie,sizeof ie,0x1000,50);
    const uint8_t ier[]={0x66,0x44,0x0f,0x38,0x80,0x0d,0x10,0,0,0};
    o=decode(ier,sizeof ier,0x1000,50);
    assert(o.ea==0x101a && o.regB==9);
    const uint8_t ies[]={0x66,0x43,0x0f,0x38,0x80,0x4c,0xa5,0xf8};
    g[12]=2;g[13]=0x2000;
    assert(decode(ies,sizeof ies,0x1000,50).ea==0x2000);
    BAD(50,0x0f,0x38,0x80,0x00);
    BAD(50,0xf3,0x0f,0x38,0x80,0x00);
    BAD(50,0x66,0x0f,0x38,0x80,0xc0);
    BAD(50,0x66,0x0f,0x38,0x81,0x00);
    BAD(50,0x66,0x67,0x0f,0x38,0x80,0x00);
    BAD(50,0x66,0x48,0x48,0x0f,0x38,0x80,0x00);
    BAD(50,0x66,0xf0,0x0f,0x38,0x80,0x00);
    puts("PASS restricted nested VMX operand decoder including INVEPT");
}
