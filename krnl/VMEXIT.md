# CPUID and HLT exit dispatch (diagnostic profile)

`make vmx-exits` produces `build/vmx-exits-esp`, separate from the existing
`vmx-guest` resume diagnostic and `vmx-ept` permission diagnostic. Default boot
remains read-only VMX probing. It uses private EPT guest backing, repeats the
normal RW and three EPT permission tests, then executes four CPUID instructions
and one terminal HLT. The nine-exit sequence is bounded.

CPUID exits unconditionally in VMX; HLT exiting (primary control bit 7) is
explicitly requested and capability-gated. `vmexit.c` provides a pure dispatcher
for exit reasons 10/12, exact lengths 2/1 and caller-owned code-range validation.
The diagnostic additionally requires exact per-phase RIP, reason and stack.
Unexpected reasons, VM-entry failures, lengths or addresses reject without
mutating state. Only successfully handled instructions advance RIP. EPT faults
continue to retry the same RIP after INVEPT, never use this CPUID/HLT path.

## Deliberately restricted synthetic CPUID contract

No host CPUID output is forwarded. Input leaf/subleaf use low EAX/ECX; outputs
zero-extend EAX/EBX/ECX/EDX. Other GPRs and VMCS RFLAGS are unchanged.

| Leaf, subleaf 0 | Output |
| --- | --- |
| 0 | maximum basic leaf 1; vendor `SyleSyleSyle` |
| 1 | ECX hypervisor bit 31; EDX PAE bit 6; other fields zero |
| 0x80000000 | maximum extended leaf 0x80000001 |
| 0x80000001 | EDX long-mode bit 29 only |
| 0x40000000 | maximum hypervisor leaf 0x40000000; vendor `SyleSyleSyle` |
| unknown leaf or nonzero subleaf | all four outputs zero |

VMX, APIC, SIMD/XSAVE/AVX, TSC guarantees, SYSCALL and NX are not advertised.
This is an integer-only bring-up contract for a guest already entered in long
mode, **not** an OS-compatible CPU model. Hiding a feature from CPUID does not
itself intercept its instructions: arbitrary guest code remains out of scope.

## HLT semantics

A validated HLT advances the saved RIP once, terminates this diagnostic and
returns to host cleanup. It never enters a VMRESUME busy loop. It does not
implement an interruptible halted-vCPU state, scheduler, wakeups or event
injection. These must be designed before using HLT for general guests.

Success requires guest-side checks of vendor/features/unknown leaves and
32-bit zero-extension, validated HLT marker, and host verification of EPT
backing data. Require `VMEXIT.DIAGNOSTIC_PASS cpuid=4 hlt=1 ept_violations=3`
and the subsequent VMXOFF pass, not merely smoke exit status.

`tests/vmexit.sh` tests the pure contract with ASan/UBSan, compiles the image,
and verifies safe TCG skip.

On 2026-11-09 the new image passed two isolated QEMU/KVM runs on archbook
(Core Ultra 9 185H, one vCPU, 256 MiB and 512 MiB, fresh OVMF variables).
Both runs reported four reason-10 CPUID exits, terminal reason-12 HLT, all
three reason-48 EPT violations repaired by INVEPT/retry, the exact diagnostic
pass marker above, VMX root roundtrip and smoke pass. QEMU exited normally;
no host configuration or other VM was changed. Raw logs are retained in
Library `libfile_fdcb28ad2a1881919c16a4dc73726b43` (`exits-evidence.tar.gz`).
This validates nested execution under KVM, not bare-metal or Windows/Hyper-V
guest compatibility.
