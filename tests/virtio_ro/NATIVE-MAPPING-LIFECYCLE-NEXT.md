# Next native mapping/lifecycle boundary (design, not implemented)

## What the host evidence establishes

Actual guest_control, session, 128 MiB LGM and PMM capability authority have been
exercised in combinations with host fixture mappings and mocked VMX. The PMM
query proves current allocation generation and kind only. It does not prove
that a host pointer is uniquely mapped, cannot escape, remains pinned, or is
unreachable by a device/NMI. No native map or guest DMA callback is installed.
The manager remains byte-identical to the pre-session task snapshot.

## Concrete missing interface

A mapping lease must belong to an allocation capability, not just a PA. A
controller-owned record needs epoch, allocation generation, immutable kind,
pinned lifetime, map span, and phase. The mapping callback must first validate
that record, then check the real paging translation and WB attributes, and
only then produce a bounded pointer. It must not accept a caller-provided
trusted/owned bit. Reject non-page alignment, overflow, wrong kind, missing
pin, stale epoch, unexpected aliases and any map outside the owned record.
Actual host aliases and EPT aliases need separate checks; PMM ownership alone
cannot establish either. A successful one-page audit is not a global proof.

The existing sg_memory_ops.map returns an unscoped raw pointer. Therefore a
strict implementation must either keep the allocation pinned for the full
CREATE lifetime, with trusted no-pointer-escape callers, or introduce a scoped
map/unmap API before claiming revocation. A DMA backend needs a separate
bounded admission/copy interface and cannot inherit authority from this raw
pointer API. Device/IOMMU isolation remains a blocker independent of MFENCE.

## Close ordering, including the pin/free dependency

Do not simply pin every page and unpin after lvh_close: lvh_close currently
frees VMXON/VMCS itself, so that ordering would deadlock protected release.
Proposed additive refactor, preserving the legacy close wrapper:

1. Split internal close into `quiesce` and `release_controls`. Quiesce marks
   ready=false and completes the current VMCLEAR, INVEPT and VMXOFF sequence
   without freeing any pages. Each failure retains ownership and blocks all
   later phases. Successful earlier instructions must not be invented/repeated
   indiscriminately on retry; retain exact close progress/launch state.
2. A trusted session controller calls quiesce, verifies its receipt belongs to
   THIS host context and epoch, and checks there is no in-flight entry, DMA
   transaction, scoped map or asynchronous producer. NMI/AP/device exclusion
   needs independent platform evidence. IF=0 or single BSP does not prove it.
3. Only then move the ownership phase to CLOSING and relinquish the relevant
   lifetime pins. A callback returning true is not itself evidence: receipts
   must arise from the actual VMX instruction path. Keep immutable receipt
   generation; never accept the manager's arbitrary boolean as proof.
4. Release VMX control capabilities, restore host VMX enable state, release
   EPT/RAM, then allocator epoch metadata. Partial free failure keeps exact
   remaining records. Already-released phases are idempotent; REAP cannot
   re-enter guest execution. Any unresolved quiescence stays fail-closed.

Until implemented and validated against actual lvh code, the existing host
close stub proves sequencing only, NOT VMCLEAR/INVEPT/VMXOFF success.

## Required next tests before native selection

- Actual lvh_close state-machine code with mocked instruction receipts: fail
  each VMCLEAR/INVEPT/VMXOFF; demonstrate no unpin/free after failure.
- Late control/RAM free failure after proven quiescence: retry frees remaining
  records only; no new CREATE/START; owner-exit follows the same quarantine.
- Actual paging/EPT inspection with stale generation, wrong type, host alias,
  EPT alias, absent mapping, non-WB and overflow inputs; no pointer published
  on failure. Synthetic page tables remain host evidence, not live lifetime.
- Map-use scope and in-flight counter failure tests, including rollback and
  reentry through the real controller. Direct calls and NMI concurrency need
  enforceable integration constraints, not merely documentation.
- Stable per-CREATE identity: current one-shot contexts cannot be reused. Use
  bounded fresh context slots or prove all old callback identities retired;
  make exhaustion explicit rather than resetting generation/context silently.
- Opt-in final ELF with the actual adapter and all cleanup symbols, no raw PMM
  bypass, correct kernel ABI and reserved memory budget. Then controlled native
  VMX checks with retained evidence. Only afterward consider virtio DMA.

No additional manager switch or native adapter is enabled by this design.

### Narrow implementation alternative under review

Prefer an inline close barrier over exporting an independently callable
`release_controls` or a mutable `quiesced=true` receipt. An additive
`lvh_close_with_barrier` could run the existing real quiesce sequence, invoke a
trusted idempotent callback only after its success, then perform existing
control-page frees. Legacy `lvh_close` passes no callback. The callback is the
control-flow boundary, not proof supplied by a caller. A failed callback
retains pages and forbids guest reentry; the controller binds it to the current
epoch and keeps it immutable. A caller able to corrupt host context/function
pointers is outside this trusted-kernel contract.

The optional role open path currently attempts legacy close on open failure.
If lifetime-pinned controls were allocated, that close may complete VMXOFF but
must fail their physical release. The later controller barrier close can then
unpin/release. Tests must demonstrate this retained failure case explicitly.
No kernel/API implementation of this alternative has been made yet.

### Host implementation update

The reviewed inline alternative is now implemented as the additive
`lvh_close_with_barrier`. Legacy close remains a null-hook wrapper, with no
struct/ops layout change. Tests `vmx_close_barrier.sh` and `vmx_role_host.sh`
exercise actual lvh code; the latter uses actual PMM lifetime pins. Covered:
clear/invept/off failures, post-off barrier retry without VMX reexecution,
partial unpin failure, one control freed while the other free fails, automatic
open-failure cleanup retained by pins, and VMXON definite failure (never active).
Unknown VMXOFF completion remains unrepresentable and must fatal-stop, not
return false. The callback must not free pages/change context/restore CR4;
CR4 restoration remains the session's subsequent platform.disable operation.

This is NOT yet selected by session release, whose default is still legacy
close. A later opt-in release-path seam and fixed controller/epoch binding are
needed; malloc maps and mocked VMX still do not prove native lifetime safety.
