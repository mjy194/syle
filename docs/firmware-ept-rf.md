# Completing a decoded MOV after an EPT exit with RF=1

Evidence: native SVR-read exit48 atRIP FFFCFBE6 had RFLAGS10093,
interruptibility0, pending-debug0 and CS38. The first guard rejected RF before
MMIO decoding. RF in this case is expected architectural VM-exit state.

Intel SDM Vol3C §28.3.3, Saving RIP, RSP and RFLAGS: for APIC-access, EPT
violation/misconfiguration and related exits, when IDT-vectoring.valid=0,
the saved RF value is1. It is not evidence that firmware enabled a debugger.
Intel SDM Vol3B, instruction-breakpoint conditions: RF is cleared at the
start of an instruction after checks for instruction breakpoints, CS limits
and FP exceptions; task switch and IRET have separate RF-image semantics.
Sources retrieved from Intel:
https://cdrdv2-public.intel.com/671506/326019-sdm-vol-3c.pdf
https://cdrdv2-public.intel.com/671427/253669-sdm-vol-3b.pdf

## Narrow completion policy

Only reason48 with fully validated ordinary32-bit MOV to the registered
xAPIC window may reach the new policy. No string, REP, IRET/task switch,
unknown opcode or unsupported operand is completed. Page/ownership/EPT and
source-free APIC checks all finish before any architectural write.

TF must be0; interruptibility and pending-debug must be0; actual guest DR7
must equal400. This is deliberately stricter than checking only L0..L3 and
G0..G3 enable bits: it also rejects GD and other nonbaseline debug state.
No single-step/data-breakpoint/#DB injection is claimed. VM-entry event and
IDT-vectoring must remain clear. Other exit types with RF still stop.

After a valid MOV, preserve all saved RFLAGS bits except RF, which is cleared.
Do not clear RF simply upon exit or on a rejected emulation. For loads into
RSP, commit RSP first, then RFLAGS if RF was set, then RIP. Other loads/stores
commit RFLAGS(if needed) then RIP. Publish staged device/GPR only after all
VMWRITEs succeed. RF0 avoids an unnecessary flags write.

If any write fails, earlier RSP/RFLAGS may already have changed in the VMCS.
This is NOT hardware rollback. A fatal latch forbids retry or guest re-entry,
software/device state remains unpublished, and normal VMCLEAR/INVEPT/VMXOFF
cleanup precedes releasing owned memory. Changed guest RSP is not host RSP.
Thus there is no continuation with a half-committed instruction.

Tests cover RF1 read/store and ESP load, every two/three-write failure
position, only-RF clearing10093→93, preserved other flags, RF0 original path,
TF+RF, DR7 local/global/GD, interruptibility/pending debug, unknown decode and
rejection with no writes. Native validation must show the next actual stop;
host tests do not prove a completed native APIC transaction by themselves.
