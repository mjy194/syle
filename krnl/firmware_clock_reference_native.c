#include <stdint.h>
#include <stdbool.h>
/* Hv#1 reference counter and reference page MSRs, after feature admission.
 * Reuse the armed #GP/#UD fixup instead of issuing an unchecked MSR access. */
extern volatile uint64_t lvn_armed_rip;
extern const uint8_t lvn_exception_hook_present __attribute__((weak));
extern bool lvn_read_raw(void*,uint32_t,uint64_t*);
extern bool lvn_write_raw(void*,uint32_t,uint64_t);
static bool clock_msr_allowed(void) {
 uint16_t cs;uint64_t flags;
 __asm__ volatile("mov %%cs,%0":"=r"(cs));
 __asm__ volatile("pushfq;pop %0":"=r"(flags));
 return !(cs&3)&&!(flags&512)&&!lvn_armed_rip&&
        &lvn_exception_hook_present&&lvn_exception_hook_present==1;
}
bool lvn_read_clock_reference(uint64_t*v) {
 return v&&clock_msr_allowed()&&lvn_read_raw(0,0x40000020,v);
}
bool lvn_read_reference_tsc_page_msr(uint64_t*v) {
 return v&&clock_msr_allowed()&&lvn_read_raw(0,0x40000021,v);
}
bool lvn_write_reference_tsc_page_msr(uint64_t value) {
 return clock_msr_allowed()&&lvn_write_raw(0,0x40000021,value);
}
