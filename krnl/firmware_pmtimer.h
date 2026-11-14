#ifndef SYLE_FIRMWARE_PMTIMER_H
#define SYLE_FIRMWARE_PMTIMER_H
#include <stdint.h>
#include <stdbool.h>
/* Host-only PIIX4 PM subset draft; not a complete PCI function or ACPI table.
 * Same admitted Hv 10MHz reference/epoch as RTC/APIC, fixed 3579545Hz 24-bit
 * counter read as dword. No SCI, sleep, SMI or bus-master feature is enabled.
 * Caller owns lifecycle, stages register transactions, never rolls back fatal. */
struct fpm_state {
 uint64_t origin,last,epoch,max_gap,ticks;
 uint32_t pmba;uint8_t enable;bool ready,failed,timer_status;
};
bool fpm_init(struct fpm_state*,uint64_t epoch,uint64_t ref,uint64_t max_gap);
bool fpm_sample(struct fpm_state*,uint64_t epoch,uint64_t ref);
/* Only exact PMBA dword40 and PMREGMISC byte80. PMBA fixed selectable B000;
 * reset IO flag1/base0, decode disabled. Other registers/values rejected. */
bool fpm_config(struct fpm_state*,unsigned off,unsigned width,bool write,uint32_t*);
/* Only enabled B008 dword reads; never accept miscomputed port0006. */
bool fpm_io(struct fpm_state*,uint64_t epoch,uint64_t ref,unsigned port,unsigned width,bool input,uint32_t*);
#endif
