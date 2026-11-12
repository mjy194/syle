#ifndef PIT0_H
#define PIT0_H
#include <stdbool.h>
#include <stdint.h>
struct pit0 { uint64_t tick,start,edges; uint32_t reload,pending; uint16_t latch; uint8_t low; bool mode,running,high_byte,read_high,latched,latch_high; };
void pit0_init(struct pit0 *);
bool pit0_advance(struct pit0 *,uint64_t);
bool pit0_access(struct pit0 *,uint64_t,unsigned,bool,uint8_t *);
bool pit0_output(const struct pit0 *,bool *);
bool pit0_count(const struct pit0 *,uint16_t *);
#endif
