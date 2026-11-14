#ifndef SYLE_LINUX_GUEST_MEMORY_H
#define SYLE_LINUX_GUEST_MEMORY_H
#include "simple_guest.h"
#include "guest_memory_role.h"
#include "linux_boot.h"
#define LGM_RAM_SIZE (128u*1024u*1024u)
#define LGM_PAGE_COUNT (LGM_RAM_SIZE/4096u)
#define LGM_PT_COUNT 64u
#define LGM_TABLE_COUNT 67u
/* Fresh zero-initialized object required. Large object: static/heap only, never kernel stack. Allocator is trusted to
 * return distinct exclusively owned pages. Inputs/ops immutable during prepare.
 * Saved callbacks/context must remain alive until all releases succeed. */
struct linux_guest_memory {
 struct sg_memory_ops owner;
 uint64_t pages[LGM_PAGE_COUNT],tables[LGM_TABLE_COUNT],eptp;
 struct lb_plan plan;
 struct lb_entry_state entry;
 bool prepared;
};
bool lgm_prepare(struct linux_guest_memory*,const struct sg_memory_ops*,
 const void*,size_t,const void*,size_t,const char*);
/* Additive opt-in: explicit RAM/EPT roles at the real allocation sites.
 * Uses owner.context for allocate_role; owner.allocate may be NULL. This API
 * does not establish DMA permission, mapping leases or lifecycle exclusion. */
bool lgm_prepare_roles(struct linux_guest_memory*,const struct sg_memory_ops*,
 sg_allocate_role_fn,const void*,size_t,const void*,size_t,const char*);
/* If ever installed: caller proves VMCLEAR, INVEPT and VMXOFF completed and
 * no vCPU references pages. Failed releases retain exact ownership tokens. */
bool lgm_release(struct linux_guest_memory*);
#endif
