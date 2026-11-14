#ifndef SYLE_FIRMWARE_MMIO_MEMORY_H
#define SYLE_FIRMWARE_MMIO_MEMORY_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "firmware_memory.h"
/* Frozen prepared ownership and immutable guest page tables are required for
 * the entire decode/validate/commit transaction. No VM execution here. */
struct fmm_memory {
 struct firmware_memory *memory;
 uint64_t cr0,cr3,cr4,efer,cs_base;
 uint32_t cs_ar;uint16_t cs_selector;
 bool advanced_ept_info; /* IA32_VMX_EPT_VPID_CAP bit22, captured by host opener. */
};
enum fmm_access { FMM_READ,FMM_WRITE,FMM_EXEC };
/* Physical32, CPL0, flat 64-bit CS, four-level paging only. 4K/2M pages;
 * no 1GiB/NX/PCID/LA57/SMEP/SMAP/PKU/CET. Requires hardware-set A throughout
 * and D on a write leaf; EFER exactly LME|LMA, CR4 allows PAE/MCE/
 * OSFXSR/OSXMMEXCPT/VMXE only. CR0.WP=0 supervisor writes are honored; never writes guest page tables. Output on success. */
bool fmm_translate(const struct fmm_memory*,uint64_t linear,enum fmm_access,uint64_t *gpa);
/* Read executable owned RAM or CODE RX (never VARS/MMIO), up to 15 bytes, without fetching beyond either
 * the 15-byte maximum or a permission/mapping boundary. Returns available prefix.
 * Decoder must prove it consumed a complete instruction within this count. */
size_t fmm_fetch(const struct fmm_memory*,uint64_t rip,uint8_t bytes[15]);
/* No read from the MMIO target. Data must lie in one 4K page and explicit
 * registered window wholly within the unmapped RAM-to-flash hole; validate decoder GLA against exit GLA and every translated
 * byte against exit GPA. Strict reason48/q=0x181(read) or0x182(write), advanced bit10
 * only when host capability bit22 is set and the owned page walk confirms
 * cumulative RW; when enabled, metadata must match (supervisor pages only); no EPT
 * permissions, no vectoring, no entry failure. Never trusts exit insn length. */
bool fmm_validate(const struct fmm_memory*,uint64_t decoded_linear,unsigned width,bool write,
 uint32_t exit_reason,uint64_t qualification,uint64_t exit_linear,uint64_t exit_gpa,
 uint32_t vectoring,uint64_t window_base,uint64_t window_size);
/* Separate immutable VARS profile, not a device hole. Exact byte write at a
 * guest-writable mapping; actual EPT links=7/leaf=owned token|31, unique backing.
 * q18A plus capability-validated RW metadata only. Does not execute the store. */
bool fmm_validate_vars_write(const struct fmm_memory*,uint64_t decoded_linear,unsigned width,
 uint32_t reason,uint64_t qualification,uint64_t exit_linear,uint64_t exit_gpa,uint32_t vectoring);
/* Safe immutable VARS read for probe admission, same owned/EPT checks; no write
 * API. Snapshot/mappings must remain frozen. Output only on success. */
bool fmm_read_vars_byte(const struct firmware_memory*,uint64_t gpa,uint8_t*);
/* Diagnostic only: observes raw guest entries from validated owned low RAM;
 * never treats these fields as execution approval or changes A/D/backing.
 * No host addresses returned. The independent translate results retain all
 * production checks, including D on writes; vars_backing_ok tests expected_gpa
 * separately and does NOT establish that translation matched that GPA.
 * Frozen snapshot and disjoint trusted output storage required. */
struct fmm_walk_entry { uint64_t table_gpa,entry_gpa,value; };
enum fmm_diag_stop { FMM_DIAG_NONE,FMM_DIAG_TABLE_RANGE,FMM_DIAG_TABLE_OWNER,
 FMM_DIAG_TABLE_READ,FMM_DIAG_ENTRY_SHAPE,FMM_DIAG_LEAF };
enum fmm_vars_stop { FMM_VARS_UNCHECKED,FMM_VARS_VALID,FMM_VARS_LAYOUT,
 FMM_VARS_OWNER,FMM_VARS_ALIAS,FMM_VARS_EPT_READ,FMM_VARS_PARENT,FMM_VARS_LEAF };
struct fmm_vars_diagnostic {
 enum fmm_vars_stop backing_stop;unsigned ept_count;
 uint8_t ept_permissions[4];bool ept_matches[4];
 struct fmm_walk_entry entries[4];unsigned count;enum fmm_diag_stop stop;
 uint64_t read_gpa,read_attributes,write_gpa,write_attributes;
 bool mode_ok,canonical,read_ok,write_ok,vars_backing_ok;
};
bool fmm_diagnose_vars_write(const struct fmm_memory*,uint64_t linear,uint64_t expected_gpa,
 struct fmm_vars_diagnostic*);
/* Read-only CODE RX ownership/EPT diagnosis. walk.vars_backing_ok remains
 * false; no admission/write/commit API is provided. Outputs contain no HPA. */
struct fmm_code_diagnostic {struct fmm_vars_diagnostic walk;bool code_backing_ok,byte_ok;uint8_t original;};
bool fmm_diagnose_code_write(const struct fmm_memory*,uint64_t linear,uint64_t expected_gpa,
 struct fmm_code_diagnostic*);
#endif
