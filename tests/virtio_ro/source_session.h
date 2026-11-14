#ifndef SYLE_SOURCE_SESSION_H
#define SYLE_SOURCE_SESSION_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
struct src_span {const void *p;size_t n;};
struct src_owner {struct src_span spans[3];uint64_t generation;bool initialized,held,retired;};
struct src_ref {struct src_owner *issuer;uint64_t generation;};
/* Single-thread private host owner; no defense against aliases or arbitrary
 * writes. Caller provides stable backing and immutable/disjoint metadata. */
bool src_init(struct src_owner*,const struct src_span[3]);
bool src_acquire(struct src_owner*,struct src_ref*);
bool src_contains(struct src_ref,unsigned,const void*,size_t);
bool src_release(struct src_ref);
bool src_replace(struct src_owner*,const struct src_span[3]);
bool src_retire(struct src_owner*);
#endif
