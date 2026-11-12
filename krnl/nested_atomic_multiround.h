#ifndef SYLE_NESTED_ATOMIC_MULTIROUND_H
#define SYLE_NESTED_ATOMIC_MULTIROUND_H
#include "nested_atomic_retrack.h"
/* N43 coordinator. The N42B one-shot API is unchanged. Stable storage, one
 * caller per physical owner, explicit consecutive generations. Both previous
 * consumers must finish before any ticket or TAM evidence can be replaced.
 * Tags never wrap or recycle; the diagnostic exercises two rounds only. */
#define NAMR_TAG_STRIDE UINT64_C(8)
#define NAMR_MAX_GENERATION (UINT64_MAX/NAMR_TAG_STRIDE)
struct nested_atomic_multiround {
 struct nested_atomic_retrack *runtime;
 uint64_t active,claimed[2],arrived[2],bound[2],command,ack[2],released,completed[2];
};
bool namr_empty_ready(struct nested_owner_atomic *,void *);
bool namr_round(struct nested_atomic_multiround *,unsigned cpu,uint64_t generation);
/* Exact-generation timeout arbitration, serialized against final commit.
 * True means that this generation already committed; stale requests do not
 * touch a newer round. A current uncommitted timeout poisons and retains. */
bool namr_release_timeout(struct nested_atomic_multiround *,uint64_t generation);
#endif
