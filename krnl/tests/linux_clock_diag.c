#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "../linux_clock_slice.h"
#include "../linux_clock_diag.h"
static unsigned texts,hexes;
void log_text(const char*s){assert(s&&strlen(s));++texts;}
void log_hex(uint64_t v){(void)v;++hexes;}
int main(void){
 lcd_reset();assert(!lcd_exits&&!lcd_preempt&&!lcd_entry_ticks);
 lcd_exit(52,100);lcd_exit(30,25);assert(lcd_exits==2&&lcd_preempt==1&&lcd_entry_ticks==125);
 (void)lcd_tsc();lcd_report();assert(texts==5&&hexes==4);
 lcd_reset();assert(!lcd_exits&&!lcd_preempt&&!lcd_entry_ticks);return 0;
}
