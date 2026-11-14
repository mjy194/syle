# Session-bound operation scope: host experiment design

Two gaps in the LGM fixture motivate this separate experiment: source access
preceded the target borrow, and PA-only dispatch could resolve an old caller's
PA against a new allocation. Do not change the LGM seam or native defaults yet.

The controller is a fresh, one-shot object bound to an actual PMM epoch, fixed
source/transfer callbacks, and private metadata. Initialization returns an
operation handle containing controller identity and epoch. A copy also takes
an actual allocation capability, not a PA. Validate handle identity/epoch,
controller phase, actual allocation generation/kind, and acquire actual PMM pin
before invoking ANY source-admission or transfer callback. Count one borrow
covering both. Resolve PA internally only after authority validation.

The handle is an in-process typed capability, not a cryptographic credential
against arbitrary kernel memory writers. Controller storage, handles and ops
must be inaccessible to the guest, not manually zeroed or rebound while a stale
reference exists. init rejects initialized objects. A stopped old controller
cannot operate, and its handle cannot be passed to a distinct/new controller.
Actual PMM epoch/generation additionally rejects a reused PA's stale allocation.

Close first revokes new operations. Existing synchronous operations may complete;
close returns false while busy. End is not performed by this module: the caller
must establish global hardware quiescence and release owners separately.
Source admission may recursively request close, but cannot start a new operation
or mutate the pinned allocation/callback/controller. A false transfer means
known synchronous completion with possibly partial bytes; latch terminal fault,
release only its short operation pin, and do not retry. Fatal/unknown completion
must never return false or run finally cleanup: retain busy/borrow/pin forever
until process/system disposal, not graceful close. Host longjmp is observation
only, never proof of actual NMI exclusion.

Tests will use actual PMM authority, same-PA new epoch, old operation handle and
old allocation token negatives, source-callback reentry/drain, partial transfer,
and nonreturning fatal at source/transfer entry. Source callbacks use private
fixed buffers; no assertion of atomic acquisition of external guest buffers.
All counters are single-threaded/non-atomic. No NMI/AP locking, native mapping,
DMA authorization, raw pointer revocation or hardware close is added.

Review constraints: this profile is pre-publication only: actual authority has
one lease/page, so it cannot take an operation pin on a hardware-lifetime-pinned
page. Future live integration must borrow the existing controller-owned lease,
never double-pin or unpin another holder. An unpin failure leaves exact lease,
busy/borrow and terminal fault retained; it must not make cleanup look complete.
Source rejection has no target write and may retry after successful unpin;
transfer false latches terminal fault. Bounds use checked subtraction, reject
zero-length operations, and initialization failure leaves output unchanged.
Controller/ops/source metadata/output storage must be disjoint from copy spans
and immutable within scope. Validation, pin and internal PA use share the same
single-thread serialized scope, not a native TOCTOU proof. Test both same-epoch
PA reuse with new generation and cross-epoch PA reuse.

## Implemented isolated experiment

`session_scope.c/h` implements this pre-publication controller; it does not
replace the frozen actual-LGM fixture. `session_scope.sh` links the real optional
PMM authority, and runs normal, partial transfer, drain during source admission,
unpin failure after transfer, unpin failure after source rejection, and two
nonreturning host observation cases. Unpin failure is injected via a linker
wrapper returning false without effect; it does not corrupt real authority.
Both generation reuse in one epoch and same-PA reuse in a new epoch are tested.
An existing lease prevents admission, without entering source callbacks.

A successful css_init only fixes private identity/configuration; it is not proof
that the epoch is currently active. Every write consults actual authority before
any source callback. The source fixture is a private static byte array, not a
source snapshot of live guest memory. The pin is held at source admission and
through transfer; recursive writes fail and drain may defer close until the
already admitted copy finishes. Fatal longjmp cases only observe and exit,
leaving actual owner/pin/busy retained, never simulate safe recovery.

To integrate into LGM later, an immutable per-session sidecar must carry the
css_handle and generation-bearing page capabilities at actual callsites. Do not
recreate a current handle from a PA request, or replace the epoch in a reused
context. The old LGM PA-only candidate is still only a separate comparison test.
This experiment does not itself grant a source buffer lease or arbitrate NMI.

Fatal tests do request css_close after observation; it sets closing and returns
false without unpin/free. They do not perform recovery or resume a copy.

## Next actual-LGM integration: whole-source lifetime

Per-destination-operation scopes do not freeze image headers/cmd between
lb_inspect calls. The LGM controller must acquire a single source-session
reference BEFORE its first inspect, retain immutable image/initrd/cmd backing
through inspection, allocations, generated parameters, load and entry preparation,
and release it only after all synchronous operations return. A fatal nonreturn
retains that reference too. Reacquiring/checking a source per chunk cannot be
substituted: headers and payload could otherwise belong to different versions.

The first host integration can use a one-shot source owner of private arrays,
identity+generation handles, no API that mutates/resizes/releases while retained,
and a fixed pointer-free span descriptor for each artifact. This proves only
the controller API's lifecycle, not that arbitrary aliases cannot write arrays.
Actual source acquisition from mutable user/guest data needs a stable snapshot
protocol before the reference is published. Generated bp/q remain synchronous
producer-private values and must be copied into scratch inside the destination
scope. They are not artifacts to retain after the producer returns.

The isolated LGM branch must receive immutable controller/source handles once
at prepare entry and use saved handles, not synthesize current identities from
owner.context on every write. A sidecar holds each returned allocation token
alongside the hardware PA; operation resolution checks both against those saved
identities. Whole-source close is distinct from each target operation drain and
from hardware quiescence. Host tests must attempt source retirement/replacement
between the first inspect and lb_load, while allocations are underway, and in
source callbacks; all must be refused until prepare terminates. Old handles must
fail after release/new owner even if address storage is reused.
