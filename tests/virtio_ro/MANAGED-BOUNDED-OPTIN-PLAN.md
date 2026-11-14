# Minimal managed opt-in integration: no default activation

The next increment should use existing `lgs_prepare_with_path`, not extend the
legacy session struct or replace manager callbacks globally. The source/scoped
host fixture is not a general native adapter. Keep the normal manager entirely
unchanged and reuse the real session's release state machine.

## Concrete file ownership and changes

1. New independent adapter/header in tests/virtio_ro, fixed singleton binding to
   one actual lgs_session and one allocator/source epoch. No caller-selectable
   mapping function or native-enable boolean. Its immutable prepare path selects
   the bounded LGM candidate and a host-open refusal function for the native
   static build. A HOST_TEST variant alone supplies mock VMX open for lifecycle
   tests. Source lifetime is acquired before the memory callback, checked against
   session image/initrd/cmd identity, and held until release/end completes.
2. Extend isolated source generation/build tooling to compile this adapter and
   bounded producer/LGM candidates with kernel ABI, preserving original inputs.
   Do not edit shared krnl/linux_boot.c, linux_guest_memory.c, session or manager.
3. Host integration must drive actual session/guest_control CREATE and REAP,
   not only call LGM directly. Compare successful mock-host construction, source
   identity rejection, partial memory failure, close failure, owner-release
   failure and end failure. Reuse existing controller busy/failed/base-released
   semantics instead of inventing a second state machine.
4. Static final ELF may contain the new fixed adapter but manager selection
   remains legacy. Assert native refusal symbols/calls remain present, default
   backend selection unchanged, symbol closure/ABI/no-LTO and input hashes.
   This proves linkage only. No native scoped copy or guest DMA is attempted.

## Later shared-core promotion (requires root ownership coordination)

When the isolated common paths are reviewed, minimal changes would be additive
source-aware writer entrypoints in linux_boot.{c,h} plus bounded-memory prepare
entrypoint in linux_guest_memory.{c,h}, preserving legacy structs/functions.
A new optional managed adapter then calls those actual entrypoints through the
already-existing lgs path seam. Do not copy an old algorithm into an unchecked
production fork or use dummy raw maps to satisfy old validation.

Native promotion still requires an implementation of immutable/resident source
acquisition, stable mapping/alias policy, scoped lifetime and hardware consumer
pins, and context/NMI/fatal handling. A native refusal adapter is deliberately
not advertised as a bootable managed profile. A successful host mocked CREATE
cannot remove its refusal. The single-lease pre-publication test design must
not be attached to already hardware-pinned pages.

## First managed increment implemented (still not core/native activation)

`managed_bounded_candidate.py` exact-transforms actual role binding to select
bounded memory and a fixed host-open entrypoint, retaining the real binding's
prepare/release/base-released state machine. It adds a prepare-done call after
actual lgs_prepare_with_path returns to close short-operation admission. The
host fixture holds source reference until real session close/RAM cleanup and
successful allocator end. Failed end retains the source and base-released flag;
retry does not repeat host close or release already-released owners.

`managed_bounded_host.sh` currently has four processes: complete prepare with
mock host-open, and partial memory operations 1/35959/68792. Each tests end
failure/retry; normal also tests host-close and owner-release failure. Actual
session/LGM/PMM are used, VMX is mocked and no guest START occurs. These calls
currently drive the backend directly, NOT guest_control CREATE/REAP dispatch;
controller route coverage is a remaining integration step, not inferred here.

`MANAGED_BOUNDED_LINK=1` in managed_final_link.sh adds generated objects and
`managed_bounded_refusal.c` to an isolated final ELF. The native memory functions,
host-open and prepare-done all unconditionally return false without target
access. This is refusal, not a working native adapter. No caller flag enables
mapping; default manager still selects its original backend. Existing native
mapping candidate's unconditional panic is unchanged. Linked symbols and final
ELF construction are build evidence only; no VM boot or native use was performed.

Source private-buffer alias/immutability and pre-publication single-lease limits
remain unchanged. A before-memory source identity gate and real controller
CREATE/REAP routing should be added before claiming managed-profile readiness.

### Controller/identity follow-up fixture

`managed_bounded_controller.sh` now separately links actual guest_control.c and
drives CREATE/DESTROY/REAP. begin checks all session kernel/initrd pointer+length
and command pointer identities against private artifact owners BEFORE allocator
start or first inspect. Wrong identities include invalid address 1 and mismatched
lengths; CREATE must reject without source initialization, allocation or byte
access and complete cleanup. begin recursively requests CREATE/REAP and verifies
real controller EBUSY. Partial prepare keeps quarantine until owner release and
end retries succeed. These identity checks are this trusted fixed fixture's
precondition, not a claim that matching pointers imply immutable native backing.

The generated sidecar is unchanged; source/allocator begin/end remain trusted
integration hooks. Host-open/close are still mocked and no START/VM entry occurs.
The previous direct-backend four-process evidence remains a separate earlier
scope, not retroactively real controller coverage.
