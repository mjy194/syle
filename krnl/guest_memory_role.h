#ifndef SYLE_GUEST_MEMORY_ROLE_H
#define SYLE_GUEST_MEMORY_ROLE_H
#include <stdint.h>
/* Trusted allocation-site purpose, not guest input or ownership proof.
 * The allocator must bind the role to its fresh allocation capability before
 * returning a PA. Release/map remain tied to the same stable owner context. */
enum sg_memory_role { SG_MEMORY_RAM, SG_MEMORY_EPT, SG_MEMORY_VMXON, SG_MEMORY_VMCS };
typedef uint64_t (*sg_allocate_role_fn)(void *,enum sg_memory_role);
#endif
