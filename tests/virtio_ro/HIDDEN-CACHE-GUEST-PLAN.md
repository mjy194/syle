# Hidden-segment evidence plan (not executed)

Keep native map panic. Neither a zero refusal marker nor current GDT bytes
establish cached CS/SS/TR state. Reuse existing guest evidence where meaningful,
then isolate any extra diagnostic from production and the frozen NMI run.

## Tier 1: inspect already planned NMI snapshots

The QMP runner records `info registers` before/after the isolated guest NMI.
QEMU's x86 register dump includes segment selector, cached base/limit/attributes,
TR and descriptor tables. Independently decode the actual dump format/version.
Compare TR cached base/limit/type with the candidate's actual TSS address and
current GDT descriptor. Compare IDTR/GDTR with retained symbols and tables.

Before injection the manager is CPL3, so CS/SS are USER cached states, not the
post-VM-exit host state. After NMI the interrupt/IST entry may have changed CS/SS.
These snapshots can establish selected TR/table observations for that virtual
CPU and event; they cannot prove the immediately-post-VM-exit host CS/SS values.
Do not relabel them as native physical-host proof or a complete lifecycle audit.

## Tier 2: separate bounded post-return observation candidate

Only after the NMI experiment and firmware queue finish, consider a NEW isolated
candidate that adds a bounded assembly-only observation window on the successful
VM-exit common return AFTER LGDT/LIDT and BEFORE return to C/user mode. It must:
- run only when the real entry-status indicates a completed VM exit, not entry
  failure; preserve all registers/stack slots/return value and saved flags;
- use a finite decrement/PAUSE loop, no callbacks, allocation, segment loads,
  descriptor writes, raw pointer borrowing, or guest-state emulation;
- publish a private diagnostic stage marker and expose exact loop RIP bounds;
- remain wholly compile-time opt-in, with source/binary hashes and assembly
  review. No patching a live kernel or changing default VMX restore semantics.

The single-VM QMP runner can stop the dedicated guest, require RIP inside that
exact window and CPL0, and read cached CS/SS/TR and GDTR/IDTR. If it misses the
window, the run is inconclusive, not a reason to guess state or inject faults.
Resume only the same admitted guest to let normal host restoration/cleanup
continue, and verify expected completion. No NMI is needed in this second test.

Do NOT install host debug registers, patch live instruction bytes, use physical
NMI/sysrq/BMC, or change OS settings. A GDB/breakpoint-based experiment would need
its own perturbation analysis and is not part of this proposal. QMP observation
is still evidence about the isolated virtual CPU, not arbitrary hardware.

## Conditions for interpreting observations

Compare actual VMCS host-field source values, table bytes, register dump and
stage/RIP together. An apparent match is one observed execution, not proof of
future immutability. Bootstrap table generation, all later descriptor writes,
VM-exit semantics, NMI fail-stop, AP exclusion, mapping lifetime/aliases and
TOCTOU synchronization must still be accounted for before mapping admission.

No Tier-2 code has been installed or VM launched by this document.

## Tier-1 evidence now available

Two frozen NMI guest runs are complete and independently checked; see
NMI-GUEST-OBSERVATIONS.md. They show cached TR/table values matching that
artifact, with no ownership cleanup, for READY/RUNNING CPL3 interruptions.
They do not capture the immediately-post-VM-exit CS/SS point. Tier 2 remains
only a proposal; no post-return observation window is installed.
