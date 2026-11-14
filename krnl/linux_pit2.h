#ifndef SYLE_LINUX_PIT2_H
#define SYLE_LINUX_PIT2_H
#include <stdbool.h>
#include <stdint.h>
#define LPIT_HZ UINT64_C(1193182)
struct lpit2 {
 uint64_t ticks;uint32_t count,reload;uint8_t low;
 bool gate,speaker,out,programmed,write_high,read_high,loading,waiting,failed;
};
/* Virtual monotonic 1193182Hz ticks, not host cycles. Fresh model required.
 * 8254 channel2 mode0 binary low/high only; no channel0/1/refresh/NMI emulation. */
bool lpit2_init(struct lpit2*,uint64_t tick);
bool lpit2_access(struct lpit2*,uint16_t port,bool write,uint8_t*,uint64_t tick);
enum lpit_result {LPIT_REJECTED=-1,LPIT_FATAL=-2,LPIT_COMMITTED=0};
enum lpit_result lpit2_io(struct lpit2*,uint64_t[15],uint64_t rip,unsigned cpl,uint32_t reason,uint64_t qualification,unsigned length,uint64_t tick,void*,bool(*)(void*,uint32_t,uint64_t));
/* RDTSC reason16: TSC=tick*1000, virtual1193182000Hz, not host frequency.
 * CPL0/CPL3 supported; caller must reject CPL3 with CR4.TSD set.
 * Both paths require the SAME logical clock; multiplication overflow rejects. */
enum lpit_result lpit2_rdtsc(struct lpit2*,uint64_t[15],uint64_t rip,unsigned cpl,uint32_t reason,unsigned length,uint64_t tick,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
