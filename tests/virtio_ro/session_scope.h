#ifndef SYLE_SESSION_SCOPE_H
#define SYLE_SESSION_SCOPE_H
#include "native_candidate.h"
#include <stddef.h>
struct css;
struct css_handle {struct css *issuer;uint64_t epoch;};
struct css_ops {void *ctx;bool (*source)(void*,const void*,size_t);bool (*transfer)(void*,uint64_t,size_t,const void*,size_t);};
/* Pre-publication host experiment. Fresh one-shot private object, disjoint
 * handles/ops/buffers; no concurrent callers or metadata mutation. No DMA or
 * native NMI exclusion. A page with another lifetime lease is NOT admitted. */
struct css {uint64_t epoch;struct css_ops ops;struct vlm_handle lease;unsigned borrows;bool initialized,busy,closing,faulted,pinned;};
bool css_init(struct css*,uint64_t,const struct css_ops*,struct css_handle*);
bool css_write(struct css*,struct css_handle,struct vlm_allocation,enum vlm_kind,size_t,const void*,size_t);
bool css_close(struct css*,struct css_handle);
#endif
