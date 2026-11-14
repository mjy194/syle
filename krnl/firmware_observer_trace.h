#ifndef SYLE_FIRMWARE_OBSERVER_TRACE_H
#define SYLE_FIRMWARE_OBSERVER_TRACE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Pure host-testable byte trace, no clocks/budgets/timer or guest operations.
 * Caller-owned stable disjoint buffer, capacity<=2MiB, fresh serialized state.
 * Sink false may have emitted a prefix: never retry that invocation. Native
 * UART adapters without acknowledgement cannot claim delivery from true.
 * Overflow drops whole event, preserves earlier buffered events, emits one
 * immediate OUT-OF-BAND marker (before deferred prefix is eventually drained).
 * Caller must forbid every later VM entry on failed; flush is still required.
 * Flush drains retained prefix once even after overflow; recursive/sink failure
 * prevents further sink invocations. Text length explicit, no strlen/unbounded
 * read. Counts cover top-level events only; rejected recursive calls are not
 * events. Counters saturate at UINT64_MAX and terminally fail on overflow.
 * dropped_bytes counts rejected events, not uncertain sink transmission;
 * buffered_bytes is cumulative accepted buffer input; used remains retained
 * prefix length after flush. sink_attempted_bytes includes marker, not ACKs.
 * Caller proves readable input, nonalias state/buffer and sink stability.
 */
#define FOT_MAX_CAPACITY ((size_t)2097152)
#define FOT_OVERFLOW_MARKER "[trace overflow: incomplete; deferred prefix follows]\n"
struct fot_trace {
 char *buffer;size_t capacity,used;
 void *context;bool (*sink)(void*,const char*,size_t);
 uint64_t text_calls,hex_calls,requested_bytes,buffered_bytes;
 uint64_t sink_attempted_bytes,dropped_bytes;
 bool ready,deferred,failed,incomplete,busy,flushed,sink_failed,marker_attempted;
};
bool fot_init(struct fot_trace*,char*,size_t,void*,bool(*)(void*,const char*,size_t));
bool fot_defer(struct fot_trace*);
bool fot_text(struct fot_trace*,const char*,size_t);
bool fot_hex(struct fot_trace*,uint64_t);
bool fot_flush(struct fot_trace*);
#endif
