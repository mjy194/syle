# Isolated vector-2 cleanup guard candidate

## Observed source path, not a demonstrated PMM reentry

`efi/src/exceptions.S:exception_2` sets nmi_entered and goes to exception_common;
exception_common calls `exception_panic` in exceptions.c. That function calls
ring3_fault before begin_panic/fatal-stop. ring3_fault currently filters only
saved CS privilege, then calls guest_owner_exit. Thus a first NMI interrupting
CPL3 management can initiate guest cleanup in NMI context. An NMI interrupting
CPL0 returns from ring3_fault without cleanup. This finding does not demonstrate
that an NMI interrupted an active PMM operation or that actual PMM reentry ran.

## Narrow candidate and default impact

`nmi_guard_candidate.py` generates isolated copies only, with exact-anchor
checks. In exceptions.c vector 2 skips ring3_fault. In ring3.c the shared hook
also rejects vector 2 defensively. Dispatcher then continues existing fatal
handling; owner resources remain retained. This is a deliberate potential
change from baseline CPL3-NMI cleanup to fatal resource retention, not a
recoverable NMI path. No public source or exceptions.S/IST/frame code is changed.
The default repository build and existing firmware packages are unaffected.

PF (14) and GP (13) user-fault cleanup remains unchanged. This does NOT intercept
or handle a guest PF. The candidate is only vector 2. Machine check (18), double
fault (8), unhandled external vectors (>=32), and debug/trap policy deserve
separate audit: the existing general hook has not been proven cleanup-safe for
those classes. Do not broaden the guard without deciding their intended fatal
behavior. The NMI handler still reaches logging/console panic paths; eliminating
this cleanup call does not prove those paths NMI-safe or create a native
allocation/copy context gate.

## Reproduction and limits

`sh tests/virtio_ro/nmi_guard_candidate.sh` compiles actual exception/ring3/
guest_control logic in baseline and candidate copies. Host-only substitutions
replace CR2 read with zero, CLI with no-op, and exception_stop with a counting
longjmp. READY and RUNNING owners are tested: baseline CPL3 vector2 stops/frees
as appropriate; candidate retains resources and dispatcher still reaches the
fatal hook. Direct hook calls are also rejected by the candidate. CPL0 vector2
never cleans up; CPL3 PF/GP retain their existing cleanup.

This does not execute an actual NMI, IST switch, CLI, CR2 read or HLT. The host
halt substitute observes counts/ownership then exits the process; it does not
resume or call cleanup after a fatal event. Separate kernel-ABI compilation
uses copies with the original privileged instructions restored.

Optional static final-link profile: NMI_GUARD_LINK=1. Exact patched C files are
retained in the artifact's nmi-candidate directory. No boot/install is performed.
This remains a review candidate, not a merge into shared exception sources.
