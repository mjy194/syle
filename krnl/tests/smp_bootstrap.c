#include "../smp_cpu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) unsigned char page[4096];
static unsigned char before[4096];
static uint64_t read_le(unsigned offset,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)page[offset+i]<<(i*8);return v;}
int main(void) {
    void(*entry)(void*)=(void(*)(void*))(uintptr_t)0x4001000;
    assert(smp_bootstrap_prepare(page,0x8000,0x100000,0x4010000,entry,(void*)0x4020000));
    assert(read_le(0x200,4)==0x100000&&read_le(0x208,8)==0x4010000);
    assert(read_le(0x210,8)==0x4001000&&read_le(0x218,8)==0x4020000);
    assert(read_le(0x222,4)==0x8240);
    memcpy(before,page,4096);
#define BAD(phys,cr3,stack,ep) do{assert(!smp_bootstrap_prepare(page,phys,cr3,stack,ep,(void*)0x4020000));assert(!memcmp(before,page,4096));}while(0)
    BAD(0,0x100000,0x4010000,entry);BAD(0x100000,0x100000,0x4010000,entry);
    BAD(0x8001,0x100000,0x4010000,entry);BAD(0x8000,0x100000000ULL,0x4010000,entry);
    BAD(0x8000,0x100001,0x4010000,entry);BAD(0x8000,0x100000,0x800000000000ULL,entry);
    BAD(0x8000,0x100000,0x4010000,0);
    puts("PASS SMP bootstrap layout and atomic input rejection");
}
