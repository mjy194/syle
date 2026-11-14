# Independent active xAPIC observer target

This is a separate opt-in target. It does not replace or modify the validated
disabled-APIC observer. It has not passed native KVM until an explicit run log
shows the result. No firmware boot/IRQ/timer support is implied by host tests.

Build with Makefile, firmware-probe.mk, firmware-ovmf-observe.mk and
firmware-ovmf-apic.mk; target firmware-ovmf-apic. Supply the same pinned
OVMF_CODE/OVMF_VARS inputs. Output is build/krnl-ovmf-apic.elf. Use the previously
reviewed5MiB-file-cap outer loader in a fresh ESP and private outer VARS.

The CPU profile is independently gated on host APIC support and adds only
CPUID1.EDX.APIC to the minimal profile, including Intel highest-leaf fallback.
x2APIC stays hidden. The virtual device starts hardware-enabled fixed xAPIC
atFEE00000/BSP; IA32_APIC_BASE=FEE00900 reads and identical writes only. This
explicit active profile is not a transition from the disabled observer.

The FEE page stays unmapped in EPT. On reason48 the observer obtains actual
VMCS CR0/3/4/EFER, CS selector/access/base, RSP and DS/ES/SS bases, GLA/GPA,
vectoring, entry-event, RFLAGS, interruptibility and pending-debug fields.
TF/RF, any blocking and pending debug reject instead of silently skipping
architectural debug delivery or STI/MOVSS-blocking completion. It does not trust EPT instruction length.
Firmware memory reader permits only owned RAM page tables and owned RAM/CODE
instruction fetch. The decoder requires a full supported MOV and32-bit APIC
access; reason/qualification/target/GLA/translation must agree. Unknown mode,
CS, A/D conditions or instruction stops; real evidence is needed before any
change to these conservative gates.

The transaction stages the source-free APIC guard. For a load into RSP it
writes RSP then RIP; other loads write RIP then update software GPR. Any failed
write latches fatal and prohibits re-entry; model state is not published after
uncertain partial VMWRITE. On success it publishes the staged device. Pending
entry events/vectoring and non-exclusive ownership reject. No pending IRQ
reservation exists; no IRQ is generated or injected by this profile.

All timer starts, ICR/self-IPI and source-unmasking are rejected. Timer state
is genuinely unstarted, so current-count0 does not pretend time is advancing.
There is no dependency on host clock frequency, VMX timer rate or CPUID15.
The preemption timer is only the existing bounded supervisor safety budget.
Maximum64 exits and all cleanup behavior remain. Completion still means
“observation stopped”, never firmware boot success.

Host tests: firmware_active_cpu, firmware_apic_guard,
firmware_mmio_memory and firmware_apic_transaction scripts. They cover feature
coupling, owned ROM fetch, mapping evidence, source-free operations and
VMWRITE failure atomicity. The original tiny, disabled observer and Linux
paths remain separate regressions and native comparison points.
