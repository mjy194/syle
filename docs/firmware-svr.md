# Firmware SVR programming, not low-vector IRQ delivery

Observed EPT MOV32 toFEE000F0 value010F, RFLAGS10093, blocking/debug0.
Bit8 remains1: this does NOT disable the APIC. Existing shared LAPIC code
rejected vector<32. Matching EDK2 BaseXApicLib.c:641-646 deliberately sets
spurious vector0F and SoftwareEnable1. Earlier native log proves a successful
SVR write01FF; later read succeeded but its value was not logged, so the old
readback and timer deadline must not be invented. New before_mmio diagnostics
print current private SVR, reference/origin/last/deadline/period/divisor.

Intel SDM SVR description distinguishes spurious-vector register programming
from normal external/LVT delivery. P6/Pentium fixes low4 bits at1; our synthetic
family6 profile chooses this conservative register contract. Only fields1FF
and low nibbleF are accepted; focus/suppression/reserved bits are refused.
The default Linux/shared lapic.c vector policy is unchanged. Only firmware
fag_mmio stages this SVR value. No interrupt/spurious/error source is added,
and low-vector enqueue/delivery remains unsupported.

Current observed timer is masked periodic LVT30005, initialFFFFFFFF, armed1.
SVR write010F changes neither timer mode/count nor deadline/phase. The admitted
reference is sampled first; expired masked periods may advance normally before
programming. Unmasked expiry remains terminal and cannot be cleared by SVR.
Disabling SVR while armed still stops. Failed RF-clear/RIP VMWRITE permanently
stops and does not publish SVR; sampled time is not rolled back.

Host tests cover readback, low-nibble/reserved/width failures, actual armed
masked sequence, equality with independently sampled timer state, both partial
VMWRITE failures, armed disable and unmasked due event. These are not proof
of native spurious IRQ/error delivery. Native sequence remains pending.
