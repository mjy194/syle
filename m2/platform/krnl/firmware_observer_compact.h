#ifndef SYLE_FIRMWARE_OBSERVER_COMPACT_H
#define SYLE_FIRMWARE_OBSERVER_COMPACT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define FOC_MAX_CAPACITY ((size_t)2097152)
#define FOC_DICT_MAX 512u
#define FOC_TEXT_MAX 512u
#define FOC_OVERFLOW_MARKER "[compact trace overflow: incomplete; decoded retained prefix follows]\n"
struct foc_entry {uint32_t offset;uint16_t length;};
/* No heap/time/source sampling. Private serialized stable state/buffer, bounded
 * disjoint readable text input copied on first definition. Sink must not mutate
 * state/buffer/input and must treat slices as length-delimited (not C strings).
 * Definitions emit their text event; refs emit same owned bytes; hex emits19.
 * Marker is out-of-band before deferred prefix. Any failed forbids guest entry.
 * Sinkfalse may have sent a prefix: never retry. No native UART ACK claim.
 * Counters top-level calls only, saturating overflow is fatal. dropped_bytes
 * excludes uncertain sink transmission; attempted includes marker. Encoded
 * bytes are retained after one-shot flush; decoded counts successful callbacks.
 */
struct foc_trace {
 uint8_t *buffer;size_t capacity,used;void *context;
 bool(*sink)(void*,const char*,size_t);
 struct foc_entry dictionary[FOC_DICT_MAX],decode_dictionary[FOC_DICT_MAX];unsigned dict_count;
 uint64_t text_calls,hex_calls,requested_bytes,dropped_bytes,encoded_bytes;
 uint64_t sink_attempted_bytes,decoded_events,decoded_bytes;
 uint64_t accepted_events,accepted_bytes;
 bool ready,deferred,failed,incomplete,busy,flushed,sink_failed,marker_attempted;
};
bool foc_init(struct foc_trace*,void*,size_t,void*,bool(*)(void*,const char*,size_t));
bool foc_defer(struct foc_trace*);
bool foc_text(struct foc_trace*,const char*,size_t);
bool foc_hex(struct foc_trace*,uint64_t);
bool foc_flush(struct foc_trace*);
#endif
