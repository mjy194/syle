# Port92 fixed-A20 observation profile

Actual stop: IN8 port92, EC at RIP82D00B. Matching EDK22025.02
OvmfPkg/Library/PlatformInitLib/Platform.c:330-333 performs IoOr8(92,BIT1)
to disable A20 masking. It reads the current byte then writes bit1 set.

The virtual platform has A20 permanently enabled. Intel SDM Vol3C VMX
restrictions state VMXON fails in A20M mode and VMX operation blocks A20M;
A20M cannot be active in VMX. Our firmware-memory EPT maps each low-RAM GPA
to its owned backing without clearing address bit20, not a masked mirror.
Thus returning bit1 set describes current execution, not emulating a gate
transition. The actual observer is executing long-mode code and validates
CS.L and owned instruction fetch before this scalar adapter.

Only IN8 returns02 (preserving upper56 RAX bits). Only OUT8 AL02 is accepted
idempotently; unused RAX bits do not belong to the operand. Bit0 requests a
fast reset and is refused terminally, never consumed as success. Clearing
bit1 or setting any other bit also stops; widths2/4, REP/string and other
forms are unsupported. No reset, A20-disable, host IO or persistent hardware
latch is introduced. NMI handling is unchanged: port70 private mask latch
and unsupported-event fail-stop; this model neither masks nor hides an NMI.

Integration uses existing owned instruction decoding, exclusive ownership,
time sampling, cloned value, RIP commit then RAX publication. Failed commit
forbids retry/reentry and does not publish a read value. Tests include all
byte writes including reset, wrong width/port/direction and partial-commit
failure. Native read-modify-write remains pending for this candidate.
