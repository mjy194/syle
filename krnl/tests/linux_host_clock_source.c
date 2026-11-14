#include "../linux_host_clock_source.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 struct lhc_cpuid c={0x16,0x80000008,2,160,38400000,256};uint64_t hz=1;
 assert(lhc_frequency(&c,&hz)&&hz==3072000000);
 c.denominator=7;hz=1;assert(!lhc_frequency(&c,&hz)&&hz==1);
 c.denominator=2;c.crystal_hz=0;assert(!lhc_frequency(&c,&hz));
 c.crystal_hz=38400000;c.power_edx=0;assert(!lhc_frequency(&c,&hz));
 c.power_edx=256;c.max_basic=0x14;assert(!lhc_frequency(&c,&hz));
 c.max_basic=0x15;c.numerator=UINT32_MAX;c.crystal_hz=UINT32_MAX;assert(!lhc_frequency(&c,&hz));
 puts("host CPUID exact-frequency admission PASS");return 0;
}
