#ifndef SYLE_LINUX_PT_DIAG_H
#define SYLE_LINUX_PT_DIAG_H
#include <stdint.h>
#include <stdbool.h>
struct lpt_trace {uint64_t address[4],entry[4],gpa;unsigned count,status;};
/* Diagnostic only: no guest writes. Status 1 structurally resolved only (not permission/access validation), 2 not present,
 * 3 unsafe/unsupported encoding, 4 unavailable RAM. NX permissions are not checked; CR3 cache flags conservatively reject. */
void lpt_walk(uint64_t cr3,uint64_t va,uint64_t ram,void*,bool(*read64)(void*,uint64_t,uint64_t*),struct lpt_trace*);
#endif
