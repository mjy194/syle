#ifndef SYLE_NATIVE_MAP_CANDIDATE_H
#define SYLE_NATIVE_MAP_CANDIDATE_H
#include "scoped_access.h"
struct vnm {struct vsc *scope;uintptr_t external;size_t external_size;uint64_t root;bool configured;};
/* Configure stable private binding before vsc_init; external is an admitted
 * resident host buffer, never an untrusted guest pointer. Does not map memory.
 * Native transfer is deliberately fail-stop until real NMI context gating is
 * integrated. HOST_TEST uses real paging walk with synthetic CR registers. */
bool vnm_bind(struct vnm *,struct vsc *,void *,size_t,struct vsc_ops *);
#endif
