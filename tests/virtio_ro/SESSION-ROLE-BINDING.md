# Opt-in session lifecycle seam (host-only validation)

`linux_guest_session` now shares its existing CREATE implementation between
legacy allocation and a trusted prepare-path table. Existing session/platform
structures are unchanged. The default backend passes no table and invokes the
same legacy functions. `prepare_guest` and pre-entry hooks are preserved.

The independent `linux_guest_role_binding` sidecar fixes the role path to the
real LGM/VMX role-aware entrypoints. It is not installed by the default manager.
It marks an attempted allocator epoch active before calling begin, including
begin failure, so CREATE/START cannot bypass cleanup. STOP retains ownership.
Release calls the actual session release first: close VMX, disable host VMX,
release RAM. Only success permits allocator end. End failure retains the
sidecar and retries end alone, never re-releasing base resources. Start failure
also blocks reentry until cleanup. Dirty host handling remains fail-stop.

This is trusted kernel integration, not protection against corrupted callback
pointers. Configuration validates null callbacks and session state; callers
must supply fresh stable sidecar storage, immutable callbacks/ops/context and
exclude reentry/concurrent calls/direct bypass through the base backend. The
path table is copied once before callbacks. No nonnative caller may manufacture
native authority by attaching a label or returning true from begin/end.

`sh tests/virtio_ro/session_binding.sh` compiles the real session and sidecar
with strict warnings and ASan/UBSan. LGM/VMX and epoch callbacks are fixtures:
this verifies cleanup sequencing/retention, not actual page ownership, VMX
instructions, NMI exclusion, allocator leases or DMA. The existing session
test remains independently passing without linking role entrypoints.

Still required: a real manager-owned authority context, typed mapping and free
validation, end failure recovery, full capacity and final ELF checks, and
native exclusion evidence. No guest DMA callback is enabled. Hardware and NMI
quiescence are not inferred from a single BSP or disabled maskable interrupts.

## Authority epoch integration (separate experiment)

`session_authority.c` binds real allocation-site roles to the optional PMM
capability authority. RAM/EPT/VMX-control kinds are created immutably by the
authority, not accepted from a map caller. PA lookup checks current epoch,
allocation generation, exact PA, live record and authority kind. Release uses
the validated allocation token; records remain on failure. Its context is
**one-shot**: a subsequent CREATE needs fresh stable context storage, rather
than resetting an old PA callback identity. This is a current integration
limitation, not a generic repeatable manager implementation. Metadata budget
must not exceed actual authority capacity. It inherits CREATE tombstones and
bounded churn from create_context. No mapping callback is provided by the
adapter; a successful query alone does not establish mapping lifetime.

`session_authority.sh` runs actual PMM and actual session sequencing together
with miniature LGM/VMX fixtures. It checks immutable kind mismatch, ordinary
free refusal, stale epochs, pin-retained release and end retry. It does not
prove real RAM preparation or VMX execution. `session_lgm_epoch.sh` additionally
links actual 128 MiB LGM and actual boot-input parser, hash-gates the inputs,
and uses malloc-backed host maps checked against real authority records. Only
VMX is mocked in that combination. The host map's contiguous PA indexing is a
fixture property, not a native mapping algorithm or DMA lifetime proof.

The trusted controller must keep ops and contexts immutable and prevent direct
base-backend bypass/reentry. Null callbacks and wrong authority-kind records
are rejected; arbitrary executable callback corruption is NOT contained by
this code. The default manager still has no authority-selected path. A
production manager must solve stable per-CREATE identity/repeatable context
storage, mapping aliases/lifetimes, exclusion, and actual adapter wiring before
this can become a native lifecycle guarantee.

The combined epoch test now uses the actual guest controller. Callback attempts
at CREATE/owner-exit reentry return EBUSY; wrong owners return EPERM. This only
covers calls through that controller, not direct backend calls or NMI races.
REAP after allocator-end failure calls STOP again: the sidecar returns
idempotent success when base release already completed, so end can be retried.

A failed allocation admission may retain an unpublished authority token and
latch the context failed. `vsa_release` still allows cleanup of earlier
published pages after independently validating their live capability; it
continues rejecting pinned/quarantined entries. Otherwise base-release-before-end
would deadlock against the generic context's failed flag. Only subsequent
end cleans unpublished tokens after real session close/release succeeded.
The test injects failure on the second allocation's PA query (actual authority
otherwise): first RAM is published, second token unpublished, controller CREATE
fails and automatically disposes, ending EMPTY with actual PMM allocation count
zero. The wrapper is test-only, not a native authority failure simulator.

Published cleanup also resolves every live quarantine capability's PA first.
An unresolved lookup or alias to the published target blocks release, even if
that makes cleanup unrecoverable. Tests corrupt a quarantine record into a
published alias and then an invalid generation and verify allocation count is
unchanged. Real authority consistency remains trusted; this is not protection
against arbitrary kernel memory corruption. See NATIVE-MAPPING-LIFECYCLE-NEXT.md
for the next mapping/close boundary and the lifetime-pin/close dependency.
