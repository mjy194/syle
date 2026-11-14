# LGM DMA bridge: audit only; native connection BLOCKED

## Actual source findings

`krnl/linux_guest_memory.h/.c` owns RAM physical-page tokens in `pages[]` and EPT
page tokens in `tables[]`, with preparation state and `eptp`. `lgm_prepare` builds
4K WB leaves (`page | 0x37`) and `lgm_release` clears prepared/eptp then releases
pages. Its lifetime contract requires completed VMCLEAR, INVEPT and VMXOFF before
release when installed. `sg_memory_ops` in `krnl/simple_guest.h` exposes only
allocate/release/map/writeback. Its documented allocator contract promises
unique/exclusive pages, but it exposes **no pin/lease, shared-owner query, epoch,
or revocation mechanism**. `writeback` is cacheability checking, not an ownership
proof or DMA publication barrier.

Therefore the platform pilot's `owner=42` style record must NEVER be substituted
for an allocator check, and an `m->prepared` check cannot authorize DMA while
another path can release or mutate memory. The current adapter barrier counter
is NOT usable in a live guest.

## Implemented read-only audit

`lgm_audit.c` includes the actual LGM header and checks one guest page against:
prepared/eptp, valid physical token, requested page token unique throughout the
RAM ledger, no EPT token in RAM, EPT-table token uniqueness, WB checks, and the
actual four-level EPT entries for that page. Read/write permissions and WB leaf
type must match the requested access and ledger token. Unsupported huge-page or
reserved-bit layouts reject. It accepts observed accessed/dirty bits, but does
not require those features to be enabled. It never maps/copies a guest data page,
changes EPT, pins anything, or returns a DMA capability. Output is diagnostic
physical token only. Output storage must be outside LGM/EPT snapshot storage;
this is a trusted caller precondition so writing the result cannot mutate the
audited ledger. Callers must freeze LGM/allocator/EPT/vCPUs during audit.

The duplicate check concerns the RAM ledger's tokens ONLY. It does not walk
other actual EPT leaves: a different GPA may already map this target page despite
having a distinct ledger token, and this audit still passes. It also cannot
exclude another VM's mapping or allocator-level sharing. Corruption of unrelated
RAM ledger entries is outside this per-page audit's scope. A passing result does
not prove global EPT alias freedom or native DMA safety.

It is deliberately expensive (full ledger/table cross-check per audited page),
not suitable in a live notify fast path. Tests use actual structure shape but
mock allocator table mappings. They establish consistency detection, not physical
ownership, map lifetime or hardware behavior. No hook into `lgm_prepare/release`
is added and no existing kernel source is changed.

## Proposed production admission transaction

1. Acquire a memory-session lease from the real guest-memory lifecycle owner,
   before reading `prepared`, the RAM ledger or EPT. The owner must atomically
   reject teardown/reconfiguration, pin all needed allocations, and bind the
   handle to this exact VM/session generation. A caller-created boolean/ID is
   insufficient. The same lock/epoch protocol must cover release, EPT remapping,
   reset and every vCPU/device writer.
2. Validate every descriptor/ring span, resolve GPA to ledger tokens, verify the
   EPT target and access, and query the allocator's authoritative allocation
   records. Require exclusive guest-RAM ownership and no shared host/page-table/
   ROM/VARS/MMIO backing. Map each page using that lease; verify stable mappings
   and host alias exclusion against the immutable disk and device metadata.
   All mappings for the complete request must exist BEFORE first mutation.
3. Snapshot descriptors/header and retain mapped spans and the session generation.
   While the guest and other writers stay stopped, perform data/status/used-element
   stores. Never re-resolve a page into a different allocation mid-commit.
4. Execute a real architecture-reviewed DMA store-publication primitive before
   used.idx. For this x86/WB-only target, require an explicit compiler memory
   barrier plus a documented hardware ordering primitive (e.g. reviewed MFENCE
   wrapper), and inspect generated code. No counter callback and no assumption
   that a cacheability check acts as a fence. Non-WB mappings are rejected; porting
   to other memory types/architectures needs a separate ordering contract.
5. Publish used.idx, complete transport state and the enclosing notification
   instruction while ownership is retained. Any uncertain store/publication/RIP
   outcome latches terminal state. Do not retry, roll back indexes or resume.
6. Drop mapping references/lease only after commit or terminal stop handling.
   Teardown waits for all leases to drain and follows existing VMCLEAR/INVEPT/
   VMXOFF rules. Device reset may start a fresh generation only under exclusion;
   stale handles fail even if physical addresses have been reused.

## Required work before enabling DMA

- Real lifecycle lease/allocator ownership interfaces do not yet exist in the
  audited `sg_memory_ops`/LGM API. This is an **integration blocker**, not something
  host mock tests can certify away.
- Implement and verify the native WB publication fence and instruction commit
  interface. The current counter barrier remains an **integration blocker**.
- Integrate failure injection for lease denied/revoked, EPT remap, stale epoch,
  shared allocation, partial mapping, abort and teardown/draining. These need
  concrete production lifecycle interfaces, not synthetic owner labels.
- Real PCI bus registration/IO reservation, IRQ routing, multi-head ownership and
  OVMF/Linux producer compatibility remain separate unresolved gates.

## Current validation

Strict freestanding/Werror plus ASan/UBSan audit tests: first/last guest page,
RAM-token duplication, RAM/EPT alias, wrong leaf target, missing write permission,
read-only admission, unsupported huge-page flag, map/WB failure at each EPT level,
RAM WB failure, unprepared/wrong EPTP and guest limit. Failed audits leave output
unchanged where asserted. These are read-only audit failures, not simulated
successful native DMA transactions.

Limitation regression tests deliberately assert PASS when an unvisited EPT leaf
aliases the target token despite a distinct ledger entry, and when an unrelated
RAM ledger entry is zero. These document the audit's scope, not acceptable native
mapping states. Production ownership/mapping admission must reject such unsafe
states through real authoritative interfaces and complete required checks.
