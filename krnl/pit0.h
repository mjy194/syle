#ifndef PIT0_H
#define PIT0_H
#include <stdbool.h>
#include <stdint.h>
struct pit0 { uint64_t tick,start,edges; uint32_t reload,pending; uint16_t latch,load_count; uint8_t low; bool mode,running,high_byte,read_high,latched,latch_high,mode4,strobe_done,load_low; };
void pit0_init(struct pit0 *);
bool pit0_advance(struct pit0 *,uint64_t);
bool pit0_access(struct pit0 *,uint64_t,unsigned,bool,uint8_t *);
bool pit0_output(const struct pit0 *,bool *);
bool pit0_count(const struct pit0 *,uint16_t *);
/* Next rising OUT edge; false when disabled/completed or no representable
 * deadline. Gate is fixed high; mode2 repeats, mode4 strobes only once. */
bool pit0_next_edge(const struct pit0 *,uint64_t *);
#endif
