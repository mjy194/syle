# M2 guest management control layer

Implemented: a single-BSP, single-bootstrap-manager lifecycle, connected to
Ring3's existing `int 0x80` dispatcher. The owner is kernel-derived (not a user
argument); no user pointer is dereferenced and no arbitrary guest image, memory
address or VMX controls are accepted. A kernel-selected profile (1) is the only
creation argument. Production backend registration is kernel-only, once, before
`ring3_start`.

ABI (rax operation, rdi handle, rsi argument; signed rax result):

| op | name | inputs | success |
|---|---|---|---|
|16|create|0, profile 1|positive generation handle|
|17|start|handle, 0|0|
|18|stop|handle, 0|0|
|19|status|handle, 0|state enum|
|20|destroy|handle, 0|0|
|21|reap owner slot|0, 0|0|
|22|step runnable guest|handle, 0|0|
|23|console send|handle, byte 0..255|0 accepted, -EBUSY full|
|24|console receive|handle, 0|byte 0..255, -EBUSY empty|

Console callbacks are optional and return ENOTSUP when absent. A committed queue
push/pop must not be reported as a retryable IRQ error: retry IRQ publication
separately so the user does not duplicate input.

The shared header defines errors. Handles never wrap; stale handles are rejected.
A failed create may not yield a handle: REAP retries cleanup for the trusted owner.
REAP is intentionally owner-wide, not a wildcard for other processes. Exiting or
faulting the manager triggers stop/release; successful exit revokes the owner.
Cleanup failure on manager exit fail-stops without declaring resources free.

Backend `prepare/start/stop/release` is not simulated by the controller. Without
registration, calls fail ENOTSUP. Prepare owns even partially allocated resources.
Failure transitions to quarantine. Start cannot reuse quarantine. Release must
not free guest RAM until relevant VMCS/EPT/VMX teardown is complete; failed cleanup
preserves ownership. Stop/release must be idempotent across partial initialization
and repeated cleanup. All callbacks must return with safe host state, including
error returns; unrecoverable host state requires fail-stop, not return to Ring3.

START and STEP are bounded execution slices. STEP requires RUNNING and continues
the retained VMCS; START is valid only for READY or STOPPED. There is no implicit
scheduler. Each successful call returns to the manager. No call is an infinite VM loop. RUNNING denotes a
logically runnable guest between slices; it does not claim Linux boot success.
This patch does not supply scheduling, timer preemption, multitasking, SMP locking,
a VMX adapter or a full manager UI. `user/mgmt/guest.h` provides the call wrapper;
the normal boot demo is unchanged and does not start Linux.

## Verification, 2026-11-12

Base: verified GitHub dev a082d8e736d8c141b7ecce19dd4d3046f6a0390a.

- `sh krnl/tests/guest_control.sh`: PASS, ASan/UBSan with leak checks disabled
  because this executor runs under ptrace (LSan reports unsupported). The
  controller itself allocates no heap memory.
- Tests cover owner/parameter validation, stale handles, non-wrapping generation,
  legal/illegal states, repeat stop, partial-prepare rollback, start failure,
  stop/release failure quarantine and recovery, reentry, owner exit and absent
  backend. The backend is an explicit host fake with allocation/running asserts.
- `make -C efi ... build/krnl.elf build/k-ring3-0.o`: PASS for kernel linkage and
  normal/diagnostic Ring3 compilation. No Linux, QEMU, VMX or Ring3 runtime test
  was performed in this change.

USER-02 is still incomplete until the real VMX backend is registered, its bounded
execution/cleanup is verified, and the Ring3 execution path is tested. These host
results do not meet Linux console, firmware, storage, display or input acceptance.
