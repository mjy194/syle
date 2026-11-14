# Bounded LGM callsites: next opt-in integration (design, not activation)

This is an implementation plan, not a claim that native mapping is safe. The
native mapping candidate continues to panic unconditionally. No default API,
struct layout, VMX path or PF policy is changed by this document.

## Actual pointer consumers and smallest replacement

`krnl/linux_guest_memory.c` currently has four distinct uses of raw maps:

1. `owned`: after allocation and WB admission, map and zero all 4096 bytes.
2. boot-writer `copy`: split GPA span into pages, map each and copy source bytes.
3. boot-writer `zero`: split GPA span into pages, map and wipe each part.
4. EPT construction: root/PDPT entry stores, PD entry stores, and 64 PTs with
   512 entries each. The `pd` pointer survives another map call for each PT.

`linux_vcpu_host.c` separately maps and initializes VMXON/VMCS revision pages.
Changing only LGM does not remove that consumer. EPT hardware references and
VMX control references outlive construction and are NOT short copy borrows.

The first opt-in seam should replace only these LGM *operations*, not return
an accessor object containing an address. Preserve the legacy implementation
and ABI. An independent sidecar owns ordered RAM/EPT capability arrays and a
fixed controller/session/epoch binding. The LGM PA arrays remain hardware
addresses, never authority. Every bounded operation resolves the corresponding
sidecar capability and checks issuer, generation, epoch, kind and actual PA.
A caller-supplied expected role alone does not authorize a page.

Proposed operations: zero(cap, offset, length), write(cap, offset, source-span),
and write_le64(cap, offset, value). Each rejects offset > 4096 or length >
4096-offset before arithmetic; no returned pointer, no callback receiving an
owned pointer, and no legacy-map fallback. A zero-length boot-writer request
can succeed without a borrow; individual scoped transfers remain nonzero.
Zero and LE64 should use adapter-private scratch/value storage rather than
requesting permission for arbitrary stack pointers. The operation must hold
its borrow before any host-buffer admission or mapping and until all accesses
complete. Holding a pin throughout construction does not replace that borrow.

## Source-side lifetime cannot be skipped

`lb_inspect` itself reads image and command data before any page writes.
`lb_load` and `lb_entry_prepare` also supply internally generated data to the
writer. Therefore simply bounding each destination copy does not cover all
input reads. Enumerate every producer first. Options are trusted permanently
resident immutable boot artifacts plus private fixed scratch, or an explicit
source capability retained across inspection and load. Neither a raw pointer
with length nor a callback returning true establishes resident/stable backing.
The first host fixture may use known arrays, explicitly not native proof.
Do not let a generic source-copy adapter silently authorize the kernel stack,
heap or arbitrary direct-map range. Reject overlap with authority metadata,
owned target pages, source bookkeeping and output records as applicable.

## Failure/publication protocol

Allocate and retain the capability before zeroing. On write/WB/admission failure
leave prepared=false and eptp=0. Completed partial writes fault the context and
cannot be retried as successful construction. Retain exact capabilities for
cleanup; unpublished allocation failure must not strand published pages behind
an unreachable end callback (the earlier session-authority cycle regression
must continue passing). No hardware consumer may receive EPTP until the entire
plan succeeds and the required publication ordering is established.

During cleanup: block admission, wait for counted operations to finish, complete
real VMX/EPT quiescence, then release lifetime pins and owners. A fatal interruption
is not a completed false-return: retain borrow/pin and never run recovery cleanup.
Partial end/free failure retains remaining records and can retry only known
unfinished releases. No old pointer can be revoked by decrementing a counter;
activation requires eliminating *all* raw consumers for those pages or proving
a separate lifetime for each one, including any other VM/direct-map aliases.

## Required host integration before any core seam is enabled

- Real LGM bootstrap and EPT contents match legacy bytes under identical input.
- Destination transfer logs prove page-local bounds and immutable role checks.
- No calls to legacy map: install a fail-fast map stub in the opt-in fixture.
- First/last byte, exact page end, crossing pages, RAM end, UINT64_MAX/SIZE_MAX,
  zero length and invalid source spans. Check subtraction before addition.
- Actual authority generation/epoch mismatch and replaced PA token rejection.
- Fail each zero, boot-copy and EPT-write position; eptp/prepared never publish.
- Full 128 MiB allocation plus rollback, extent-free failure and later retry.
- Recursive operation/close during source admission and transfer, partial copy
  failure and nonreturning fatal observation, with no fake recovery/unpin.
- Legacy aggregate, ABI and static final-ELF linkage remain separate evidence.

## Hidden-state and asynchronous activation blockers

Visible CS/SS/TR selectors plus current GDT/TSS bytes do not prove hidden caches.
The reviewed host chain proves construction/parser/VMCS-field writes only. The
actual two NMI guest observations show their stopped TR cache, not the desired
immediately-post-VMexit CS/SS/TR chain. A future isolated observation fixture may
record that point without changing host settings; repeating the same CPL3 NMI
samples adds no missing proof and is not required.

Even correct hidden state does not pin current page tables, exclude alias writes,
or provide NMI/AP/device exclusion. Double marker reads and compiler clobbers
are refusal checks, not an atomic lock. The narrow NMI default fix stops one
owner-cleanup entry; console/panic and other asynchronous paths still need their
own audit. Native activation stays blocked until a concrete lifetime controller,
source residency, stable mapping policy and fail-stop context are independently
verified. MFENCE alone supplies none of these ownership/exclusion guarantees.

### Independent design review refinements

Actual `linux_boot.c` producers include image header/payload, initrd and cmd;
inspection occurs both in prepare and again in lb_load. Generated sources include
lb_load's stack `bp[4096]` and lb_entry_prepare's reused `q[8]` (including a final
2-byte write). Existing writer callbacks cannot distinguish these trusted
producers from arbitrary stack addresses. A producer-specific scratch/source
seam must precede bounded LGM integration; accepting all stack spans is not an
acceptable shortcut. Guest initial CR3 page tables, GDT and TSS are RAM-role
writes; only `m.tables` is EPT-role. Operation width does not determine kind.

The current prepare requires map, and owned returns a pointer; use an explicit
v2/internal bounded branch, never a dummy map pointer to satisfy legacy checks.
Stage order: producer source seam; host bounded LGM parity/rollback; independent
bounded VMX-control initialization; then native activation prerequisites.

`bounded_span.c` is a first independent geometry primitive: validates a whole
GPA range by subtraction, yields only page index/offset/length and never maps
or grants authority. Its ASan/UBSan test covers all starts across two pages with
short lengths, boundaries near UINT64_MAX and the complete 128 MiB range.
Leak checking is disabled because this runner is ptraced; the test allocates no
heap memory. Capability binding and guest-byte freezing are deliberately absent.
A descriptor/header read must copy into private stable scratch once under its
proper source lifetime, then validate that snapshot; a geometry iterator does
not freeze guest contents between chunks. Future transfer admission must reject
or exclude guest concurrent writers, not treat the iterator as a TOCTOU fix.
