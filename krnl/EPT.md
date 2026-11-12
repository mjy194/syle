# Minimal opt-in EPT diagnostic

Build `make vmx-ept` in `efi`; ESP is `build/vmx-ept-esp`. The ordinary boot and
existing `vmx-guest` diagnostic are unchanged. This is bounded trusted CPU
bring-up code, **not** a production VM or an untrusted-code sandbox. It does
not implement general I/O/MSR mediation, interrupts, scheduling, SMP, DMA,
VT-d, guest firmware, Windows or nested Hyper-V.

## Memory model

Eleven independently allocated, zeroed, WB-checked backing pages are the only
EPT leaf targets. Four additional pages hold the EPT hierarchy and are never
mapped into guest physical memory. Capability admission requires four-level
EPT walks, WB EPTP type and single-context INVEPT. Four-KiB leaves only; no
execute-only mappings, A/D EPT mode, 2-MiB pages, MBEC or VPID.

Guest GPA/GVA layout:

- 0x1000–0x4000: private four-level guest page tables (EPT RW/NX for A/D updates).
- 0x5000: guest GDT, TSS at 0x5100, empty IDT at 0x5800.
- 0x6000: copied position-independent integer payload, EPT RX.
- 0x7000: guest stack, top 0x8000, EPT RW/NX.
- 0x8000: ordinary data, EPT RW/NX.
- 0x9000: allocated backing initially absent from EPT.
- 0xa000: allocated data initially EPT read-only/NX.
- 0xb000: copied return stub initially EPT read-only/NX.

The guest CR3, descriptors, stack and instruction addresses are all guest
addresses. No host CR3, GDT, IDT, stack or payload virtual address is reused.
Guest paging intentionally allows access so negative tests reach EPT rather
than being stopped first by guest #PF. EPT exposes no other physical pages.

## Bounded exit sequence

1. Normal data write/read followed by a validated VMCALL.
2. Read of missing GPA 0x9000: require reason 48, exact RIP/GPA and qualification.
3. Write to RO GPA 0xa000: require the matching write violation.
4. Execution at NX GPA 0xb000: require the matching execute violation.
5. Final VMCALL after the payload verifies data/return marker; host also checks
   ordinary data and remediated backing values before success.

Each accepted violation updates only its owned leaf, performs single-context
INVEPT and resumes the **same RIP**. Violation instruction length is not used,
and RIP is never skipped. Only the initial accepted VMCALL advances RIP by its
validated length. Unexpected exits, wrong access/state, VM-entry failure or
INVEPT failure stop the loop. `EPT.DIAGNOSTIC_PASS violations=3` requires the
whole sequence; generic smoke success is not evidence of EPT execution.

Teardown does VMCLEAR, INVEPT, VMXOFF, then frees backing and EPT pages. Failure
to clear/invalidate/leave VMX panics while retaining possibly live pages. PMM
allocation count must return to the pre-fixture baseline. Partial allocation
failures unwind before the context is made active.

## Current validation boundary

`tests/ept.sh` runs ASan/UBSan host fixture tests (strict leaf targets and
permissions, capability/qualification rejection, every allocation-failure
point, WB/mapping/invalidation failures), builds the separate image and checks
that TCG safely skips unavailable VMX. These host/TCG tests do **not** execute EPT or INVEPT.

On 2026-11-09, the unchanged diagnostic was also executed twice on archbook
(Intel Core Ultra 9 185H, QEMU/KVM, `-cpu host`, 1 vCPU), with fresh OVMF
variables and 256/512 MiB RAM. Both runs completed the five-exit sequence:
normal RW, absent read at GPA 0x9000, RO write at 0xa000, NX execution at
0xb000, then normal VMCALL end. Violation qualifications were 0x581, 0x58a,
and 0x58c; low nine bits matched the expected access/translation checks.
All three remediations completed INVEPT and resumed the same RIP. Both
`EPT.GUEST_NORMAL_END_BACKING_VERIFIED` and `EPT.DIAGNOSTIC_PASS violations=3`
were followed by successful VMXOFF and unchanged PMM allocation baseline.
This is nested-KVM hardware execution of this bounded diagnostic, not
a claim of general isolation, bare-metal coverage or nested Hyper-V support.
Evidence: Library `libfile_efac63a8ada48191bfdb1147c0d7a9b8` (`ept-evidence.tar.gz`).
