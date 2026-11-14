# Observe the real prepare root: proposed diagnostic-only next step

The successful source observation stopped at a CPL3 READY wait after CREATE.
Actual linux_guest_manager.prepare switches to paging_root() through mhc_enter
before lgs_prepare. Reading that other root while stopped in CPL3 would compare
trees but would NOT prove that callbacks actually execute under it.

## Candidate, not default change

Prefer a narrowly generated copy of linux_guest_manager.c in an isolated probe
image. Add one bounded observation window immediately AFTER successful
enter_context() and BEFORE calling lgs_guest_backend.prepare. No source/page
access, allocation, VMX operation, PF policy change or guest DMA belongs in this
hook. Normal manager source stays unchanged. Its real enter_context and later
prepare/release code remain byte-source-identical outside the hook.

An out-of-line diagnostic helper emits a unique marker, then executes a finite
counted PAUSE loop with named assembly range labels. It returns normally if not
observed. It must preserve the calling convention, all callee-saved registers
and caller state; no controller/session fields or page permissions are written.
The loop has a fixed iteration bound, never waits forever for an external flag.
It introduces diagnostic delay, so it is NOT described as identical runtime
timing. Keep interrupts/segment/control-register state as established by the
real path; do not force them to pass checks. No NMI or debug trap injection.

## Admission and one-shot observation

Reuse the reviewed fresh isolated QMP launch/identity/CONTROL/RAM whitelist
procedure, serialized after firmware work. After the unique marker, STOP with
acknowledgment. Accept only RIP at a decoded instruction boundary within the
helper's finite loop, CPL0, and expected EFLAGS context. Otherwise inconclusive,
quit and no automatic retry or window enlargement. The session must still be
pre-prepare (unprepared/disabled/no VMX control pages) and controller CREATE busy.
Read both actual CR3 and the uniquely identified 8-byte paging.c root variable
(the ELF also contains a different 4096-byte ring3 root symbol; never select by
name alone). Require exact match rather than manufacture the expected CR3.

Then apply the reviewed readonly source walk and exact source identity/hash
checks under captured actual CR3. Because lgs_prepare has not run yet, compare
configured source pointers/lengths and source backing, not READY flags. Retain
registers including visible CS/SS/TR and QEMU descriptor-cache fields; these
are observations at this point, not proof of all later VM-exit restoration.
Quit the isolated guest from STOP after capture. No guest entry is needed.

## Review gates before any execution

1. Root approves isolated source ownership/hook placement; shared default files
   are not edited. Freeze original input hashes and exact candidate delta.
2. Host test verifies the original path's early rejection skips the hook, valid
   enter reaches it before prepare, and helper returning does not alter state.
   Actual hardware register preservation requires ABI/disassembly inspection;
   host stubs alone are not hardware proof.
3. Static build exports exact hook label/instruction-boundary inventory; real
   compiled entry route and no caller-state writes independently reviewed.
4. Runner mode differs from READY mode: no copied READY/host-active assumptions.
   Native mapping gate remains refusal/panic. Only root schedules the one run.

This would prove one observed callback-entry context/source snapshot, not
mapping lifetime, NMI exclusion, EPT ownership, actual VMXOFF teardown or general
native source immutability. If root prefers no diagnostic instruction changes,
the alternative is a passive dual-root tree comparison; its weaker scope must
be reported instead of calling it actual prepare-entry observation.
