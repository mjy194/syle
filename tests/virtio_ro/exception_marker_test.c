#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
extern unsigned exceptions_fatal_context(void);
/* Test-only objcopy exports private bytes; production object does NOT. */
extern unsigned char nmi_entered,exception_entered;
unsigned char tss[128];
void exception_panic(void *p){(void)p;abort();}
unsigned char lvn_fault_fixup(uint64_t vector,uint64_t *rip){(void)vector;(void)rip;abort();}
int main(void){for(unsigned n=0;n<=1;n++)for(unsigned e=0;e<=1;e++){nmi_entered=n;exception_entered=e;assert(exceptions_fatal_context()==(n|e));assert(nmi_entered==n&&exception_entered==e);}puts("actual assembly read-only refusal helper: flag combinations and no writes PASS; no NMI/IST executed");return 0;}
