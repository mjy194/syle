#include "linux_host_clock_source.h"
bool lhc_frequency(const struct lhc_cpuid*c,uint64_t*out){
 if(!c||!out||c->max_basic<0x15||c->max_extended<0x80000007||!(c->power_edx&256)||!c->denominator||!c->numerator||!c->crystal_hz)return false;
 /* Two uint32 factors fit uint64 exactly. No nominal-MHz fallback. */
 uint64_t product=(uint64_t)c->crystal_hz*c->numerator;
 if(product%c->denominator)return false;
 uint64_t hz=product/c->denominator;
 if(hz<1000000||hz>UINT64_C(10000000000))return false;
 *out=hz;return true;
}
