#ifndef SYLE_LGM_BOUNDED_CANDIDATE_H
#define SYLE_LGM_BOUNDED_CANDIDATE_H
#include "linux_guest_memory.h"
#include "boot_source_candidate.h"
#include "bounded_span.h"
/* Trusted host fixture seam, not a native callback contract or DMA API. */
bool cb_page_zero(void*,uint64_t,enum sg_memory_role,size_t,size_t);
bool cb_page_write(void*,uint64_t,enum sg_memory_role,size_t,enum cs_source,const void*,size_t);
bool cb_page_u64(void*,uint64_t,enum sg_memory_role,size_t,uint64_t);
bool cb_lgm_release(struct linux_guest_memory*);
bool cb_lgm_prepare(struct linux_guest_memory*,const struct sg_memory_ops*,const void*,size_t,const void*,size_t,const char*);
bool cb_lgm_prepare_roles(struct linux_guest_memory*,const struct sg_memory_ops*,sg_allocate_role_fn,const void*,size_t,const void*,size_t,const char*);
#endif
