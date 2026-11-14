#ifndef RAMFB_CONFIG_H
#define RAMFB_CONFIG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct ramfb_config {uint64_t address,span;uint32_t width,height,stride;};
/* Host-only validation. Caller supplies a trusted single RAM interval, a stable input snapshot
 * and valid output storage disjoint from input. Failure leaves output unchanged;
 * this is not a concurrent/atomic publication guarantee.
 * Does not prove ownership, pin pages, map memory, or accept guest writes.
 * Explicit narrow profile: XRGB8888, flags0, dimensions16..1024x16..768,
 * nonzero stride >= width*4. Span covers last visible pixel, not padding. */
bool ramfb_parse(const uint8_t*,size_t,uint64_t ram_base,uint64_t ram_size,
 struct ramfb_config*);
#endif
