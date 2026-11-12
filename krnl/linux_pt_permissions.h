#ifndef SYLE_LINUX_PT_PERMISSIONS_H
#define SYLE_LINUX_PT_PERMISSIONS_H
#include "linux_pt_diag.h"
/* Fixed CPL0, 4-level, physical32 diagnostic. No SMEP/SMAP/PKU/LA57/PCID.
 * access: 0 read, 1 write, 2 execute. Checks every touched page; max64KiB.
 * Includes NXE reserved-bit semantics and CR0.WP. EPT remains caller-owned.
 * This is not by itself an exception-delivery approval. */
bool lpp_range(uint64_t cr3,uint64_t va,uint64_t size,unsigned access,uint64_t cr0,uint64_t cr4,uint64_t efer,uint64_t ram,void*,bool(*read)(void*,uint64_t,uint64_t*));
#endif
