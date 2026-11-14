#ifndef SYLE_BOUNDED_SPAN_H
#define SYLE_BOUNDED_SPAN_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Geometry only. Does not resolve PA, grant ownership, freeze guest bytes or
 * admit a source buffer. State and all output objects must be disjoint.
 * The half-open end is <= UINT64_MAX, excluding that final address byte. Caller binds every yielded index to a stable epoch's
 * immutable capability and holds its lifetime through the actual operation. */
struct vbs { uint64_t cursor, remaining; };
bool vbs_init(struct vbs *,uint64_t ram_size,uint64_t gpa,uint64_t size);
bool vbs_next(struct vbs *,uint64_t *page,size_t *offset,size_t *length);
#endif
