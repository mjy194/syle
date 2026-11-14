#ifndef SYLE_LINUX_CLOCK_DIAG_H
#define SYLE_LINUX_CLOCK_DIAG_H
/* Private singleton diagnostics: no per-exit serial I/O, no guest ABI change.
 * Aggregate counts cover the whole session, not individual clock commands.
 * Nonserialized host RDTSC is a trend counter, not a calibrated duration. */
extern void log_text(const char *);
extern void log_hex(uint64_t);
static uint64_t lcd_exits,lcd_preempt,lcd_entry_ticks;
static uint64_t lcd_tsc(void){uint32_t lo,hi;__asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));return ((uint64_t)hi<<32)|lo;}
static void lcd_reset(void){lcd_exits=0;lcd_preempt=0;lcd_entry_ticks=0;}
static void lcd_exit(uint64_t reason,uint64_t ticks){++lcd_exits;if(reason==52)++lcd_preempt;lcd_entry_ticks+=ticks;}
static void lcd_report(void){
 log_text("\nSYLE.CLOCK_DIAG bits=");log_hex(LGS_PREEMPT_BITS);
 log_text(" exits=");log_hex(lcd_exits);log_text(" preempt=");log_hex(lcd_preempt);
 log_text(" entry_tsc_sum=");log_hex(lcd_entry_ticks);log_text("\n");
}
#endif
