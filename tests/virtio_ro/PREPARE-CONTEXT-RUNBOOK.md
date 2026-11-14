# Isolated actual pre-prepare context observer: NOT RUN

This is a DIFFERENT kernel than the completed CPL3 READY source observation.
Kernel SHA256:
80ef29010473361af1bffc9feb2f31384ba8d1cf5d5f46d85116e333ea0d1c1c
The actual manager's isolated prepare callback keeps existing early checks and
service.reset, then real enter_context. It calls the diagnostic helper, ALWAYS
leave_context and return failure. It NEVER calls lgs_guest_backend.prepare.
There is no enable flag, success result or resume path into VMX preparation.
The default repository manager is unchanged. This changes only the isolated
probe's CREATE behavior and timing; it is not a bootable Linux acceptance test.

## Bounded helper and safe miss behavior

The helper emits SOURCE.PREPARE_CONTEXT_WINDOW followed by CRLF through a fixed
COM1 byte loop, maximum 10000 LSR polls per byte. It does not call framebuffer
console or read/write the audited source payload, allocator or session. Stack,
constant marker and guest serial-device accesses are real side effects. On UART
budget exhaustion it returns immediately; prepare STILL restores context and
fails. After a complete marker, an out-of-line SysV PAUSE/DEC/JNZ loop has exactly
10,000,000 iterations. It touches caller-saved ECX and arithmetic flags, not
IF/DF/CRs/segments; compiler/assembly preserve callee-saved registers and ABI.
No NMI/debug trap is injected. A missed window fails closed, never enters VMX.
The mode0 management program prints CREATE_FAIL then spins; it never issues START.

## Frozen media and admission

Use only fresh mode0 isolated QEMU media: kernel and management program are in
frozen-kernel-managers.tar.gz, paths kernel/authority-link-proof.elf and
managers/mgmt-nmi-0.elf. Keep the same reviewed EFI loader/OVMF media procedure.
Check CONTROL.sha256 before setup and again via runner startup validation.
run_prepare_observation.py 0 is the ONLY execution command for this candidate;
never run an old NMI or source READY runner against this kernel. Root/inputs must
serialize after OVMF's current KVM process has been cleaned up. No parallel VM.

The runner retains -S, private socket, exact PID/starttime/exe/argv, peercred,
CPU-thread association/device map and absolute deadline. It runs to complete
marker CRLF, STOP event plus running=false. Only decoded instruction starts
0x4003a99 (PAUSE), 0x4003a9b (DEC), 0x4003a9d (JNZ) are admitted. RET at 0x4003a9f
is NOT inside the window. CPL=0, CS=8, SS=0x10, SMM=0 and IF/DF=0 are mandatory.
No forced register writes, breakpoints or timing retries are allowed.

At the window, guest_control provisionally has state READY/handle1 but busy=1:
this is an in-progress CREATE, NOT a ready VM. Require session configured=1;
prepared/enabled/host.active/memory.prepared/state.ready=0; EPTP and both VMX
control addresses=0; every RAM/EPT ledger entry=0. Do not reuse old READY flags.
Read the uniquely identified 8-byte paging.c root symbol (a distinct 4096-byte
ring3 root shares its symbol name) and require captured CR3 exactly equals that
value. This observes real pre-lgs_prepare context, not execution of a memory
callback and not an EPT walk. service.reset already ran, so CREATE was not wholly
side-effect-free before this point.

## Read-only source proof and shutdown

Use actual stopped system FlatView pc.ram whitelist, exact offset/KVM grammar,
new-only pmemsave paths and HMP fullmatch from the reviewed source observer.
Audit current captured CR3 with the same conservative modes/table/page budgets.
Check configured kernel/initrd/cmd pointer identities and exact frozen lengths,
then recompute source hashes, effective permissions and reachable aliases.
The read is outer-QEMU guest virtual to guest physical, not Linux guest memory,
EPT or host physical memory. Unknown formats, errors, budget, missed loop or
state mismatch are INCONCLUSIVE; quit/reap, no blind retry or rule relaxation.
Never cont again after STOP: quit from the stopped observation, with complete
raw evidence. Native mapping refusal/panic remain enabled.

A successful result establishes only this one observed pre-prepare kernel-root
snapshot. It does not establish long-term immutability, mapping ownership,
external alias writers/DMA, complete NMI safety or actual VMX teardown.
