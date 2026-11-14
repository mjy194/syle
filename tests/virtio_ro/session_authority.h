#ifndef SYLE_SESSION_AUTHORITY_H
#define SYLE_SESSION_AUTHORITY_H
#include "create_context.h"
#include "../../krnl/guest_memory_role.h"
/* One-shot stable CREATE context: never zero/reuse while old PA callbacks
 * exist. Native map/WB implementation and exclusion are deliberately absent. */
struct vsa {struct vtc owned;struct vtc_entry *storage;unsigned capacity;bool begun;};
bool vsa_begin(void *);
bool vsa_end(void *);
uint64_t vsa_allocate(void *,enum sg_memory_role);
bool vsa_release(void *,uint64_t);
bool vsa_query(struct vsa *,uint64_t,enum vlm_kind *);
#endif
