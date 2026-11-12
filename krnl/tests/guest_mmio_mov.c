#include "../guest_mmio_mov.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t r[16];
static struct guest_mmio_mov dec(const uint8_t *b,size_t n,uint64_t rip) {
    struct guest_mmio_mov m;
    assert(guest_mmio_mov_decode(b,n,rip,r,&m));
    assert(m.length==n);
    for(size_t i=0;i<n;i++) {
        struct guest_mmio_mov sentinel,old;
        memset(&sentinel,0x55,sizeof sentinel);memcpy(&old,&sentinel,sizeof old);
        assert(!guest_mmio_mov_decode(b,i,rip,r,&sentinel));
        assert(memcmp(&sentinel,&old,sizeof old)==0);
    }
    return m;
}
static void reject(const uint8_t *b,size_t n,uint64_t rip) {
    struct guest_mmio_mov m,old;memset(&m,0x55,sizeof m);memcpy(&old,&m,sizeof m);
    assert(!guest_mmio_mov_decode(b,n,rip,r,&m));assert(memcmp(&m,&old,sizeof m)==0);
}
#define DECODE(...) dec((const uint8_t[]){__VA_ARGS__},sizeof((const uint8_t[]){__VA_ARGS__}),0x1000)
#define REJECT(...) reject((const uint8_t[]){__VA_ARGS__},sizeof((const uint8_t[]){__VA_ARGS__}),0x1000)
int main(void) {
    r[0]=0xfee00020;r[1]=0x1122334455667788;r[4]=0x12345678;r[12]=3;r[13]=0xfec00010;
    struct guest_mmio_mov m=DECODE(0x89,0x08);
    assert(m.linear==r[0] && m.width==4 && m.reg==1 && m.write && m.store_value==0x55667788);
    m=DECODE(0x66,0x89,0x08);assert(m.width==2 && m.store_value==0x7788);
    m=DECODE(0x48,0x89,0x08);assert(m.width==8 && m.store_value==r[1]);
    m=DECODE(0x66,0x48,0x89,0x08);assert(m.width==8);
    m=DECODE(0x88,0x20);assert(m.reg==0 && m.reg_shift==8 && m.store_value==0);
    m=DECODE(0x40,0x88,0x20);assert(m.reg==4 && m.reg_shift==0 && m.store_value==0x78);
    m=DECODE(0x45,0x8b,0x65,0x00);assert(m.reg==12 && m.linear==r[13] && !m.write);
    uint64_t loaded=123;
    assert(guest_mmio_mov_load_value(&m,UINT64_MAX,0xaabbccdd,&loaded) && loaded==0xaabbccdd);
    m=DECODE(0x8a,0x20);assert(guest_mmio_mov_load_value(&m,UINT64_MAX,0x12,&loaded) && loaded==UINT64_C(0xffffffffffff12ff));
    m=DECODE(0x66,0x8b,0x08);assert(guest_mmio_mov_load_value(&m,UINT64_MAX,0x1234,&loaded) && loaded==UINT64_C(0xffffffffffff1234));
    m=DECODE(0x48,0x8b,0x08);assert(guest_mmio_mov_load_value(&m,0,UINT64_MAX,&loaded) && loaded==UINT64_MAX);
    m=DECODE(0x8b,0x05,0x20,0,0,0);assert(m.linear==0x1026);
    m=DECODE(0x41,0x8b,0x05,0x20,0,0,0);assert(m.linear==0x1027); /* REX.B not R13 */
    m=DECODE(0xc7,0x05,0x20,0,0,0,1,2,3,4);assert(m.linear==0x102a && m.store_value==0x04030201);
    m=DECODE(0x48,0xc7,0x00,0,0,0,0x80);assert(m.width==8 && m.store_value==UINT64_C(0xffffffff80000000));
    m=DECODE(0x66,0xc7,0x00,0x34,0x12);assert(m.width==2 && m.store_value==0x1234);
    m=DECODE(0xc6,0x00,0xab);assert(m.width==1 && m.reg==255 && m.store_value==0xab);
    loaded=123;assert(!guest_mmio_mov_load_value(&m,0,0,&loaded) && loaded==123);
    m=DECODE(0x8b,0x04,0x25,0,0,0xff,0xff);assert(m.linear==UINT64_C(0xffffffffffff0000));
    m=DECODE(0x67,0x8b,0x04,0x25,0,0,0xff,0xff);assert(m.linear==0xffff0000);
    m=DECODE(0x42,0x8b,0x04,0xa0);assert(m.linear==r[0]+r[12]*4); /* R12 index */
    m=DECODE(0x8b,0x44,0x20,0xfc);assert(m.linear==r[0]-4); /* no index */
    r[0]=UINT64_C(0xfffffffffffffffc);r[1]=8;
    m=DECODE(0x67,0x8b,0x04,0x08);assert(m.linear==4); /* address32 wrap before access */
    m=dec((uint8_t[]){0x67,0x8b,0x05,0x20,0,0,0},7,UINT64_C(0xffff800000001000));
    assert(m.linear==0x1027);
    r[0]=0xfee00020;
    REJECT(0xf0,0x89,0x08);REJECT(0xf3,0x8b,0x08);REJECT(0x64,0x8b,0x08);
    REJECT(0x40,0x66,0x8b,0x08);REJECT(0x48,0x40,0x8b,0x08);
    REJECT(0x66,0x66,0x8b,0x08);REJECT(0x67,0x67,0x8b,0x08);
    REJECT(0x8b,0xc0);REJECT(0xc7,0x08,0,0,0,0);REJECT(0x44,0xc7,0,0,0,0,0);
    REJECT(0xa1,0,0,0,0,0,0,0,0);REJECT(0xa5);REJECT(0x0f,0xb6,0);REJECT(0x87,0);
    r[0]=0xfff;REJECT(0x8b,0); /* cross page */
    r[0]=UINT64_C(0x800000000000);REJECT(0x8b,0);
    r[0]=UINT64_MAX;REJECT(0x48,0x8b,0);
    r[0]=0xfffffffe;REJECT(0x67,0x8b,0); /* no per-byte 32bit wrap */
    reject((uint8_t[]){0x8b,0},2,UINT64_MAX);
    /* Deterministic address32 cases: base/index truncated as one expression. */
    uint64_t seed=1;
    for(unsigned i=0;i<10000;i++) {
        seed=seed*6364136223846793005ULL+1;r[0]=seed;
        seed=seed*6364136223846793005ULL+1;r[1]=seed;
        uint64_t ea=(uint32_t)(r[0]+(r[1]<<2)-16);
        uint8_t b[]={0x67,0x8b,0x44,0x88,0xf0};
        if((ea&0xfff)<=4092) {m=dec(b,sizeof b,0x1000);assert(m.linear==ea);}
        else reject(b,sizeof b,0x1000);
    }
    puts("PASS bounded MOV MMIO planner: prefixes, registers, addressing, immediates, atomic rejects");
}
