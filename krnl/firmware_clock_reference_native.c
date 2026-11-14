#include <stdint.h>
#include <stdbool.h>
/* Exact read-only Hv#1 reference counter, only after feature admission.
 * Reuse the existing armed RDMSR #GP/#UD fixup, never a naked RDMSR.
 * Independent target avoids changing/recompiling the frozen generic native. */
extern volatile uint64_t lvn_armed_rip;
extern const uint8_t lvn_exception_hook_present __attribute__((weak));
extern bool lvn_read_raw(void*,uint32_t,uint64_t*);
bool lvn_read_clock_reference(uint64_t*v) {
 uint16_t cs;uint64_t flags;
 __asm__ volatile("mov %%cs,%0":"=r"(cs));
 __asm__ volatile("pushfq;pop %0":"=r"(flags));
 if(!v||(cs&3)||(flags&512)||lvn_armed_rip||!&lvn_exception_hook_present||lvn_exception_hook_present!=1)return false;
 return lvn_read_raw(0,0x40000020,v);
}
