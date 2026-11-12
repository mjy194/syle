# Bounded MOV MMIO planner

Host-only pure decoder, independent of the older diagnostic MOV32 decoder.
No VM launch, guest-memory read, physical access, register mutation or device
side effect. Decode success alone authorizes no access.

Supports 64-bit code mode MOV 88/89/8A/8B and C6/C7 /0, widths 1/2/4/8,
optional single 66 and 67 legacy prefixes before optional single REX. REX.W
wins over 66. Repeated prefixes, legacy prefixes after REX, segments including
FS/GS, LOCK/REP, moffs, strings, atomic operations and all other opcodes reject.
All ModRM/SIB addressing forms and signed disp8/disp32 are handled. REX changes
AH/CH/DH/BH to the low-byte register bank; REX.X selects R12 as a SIB index.

RIP-relative base includes full instruction length, including immediate bytes.
Address32 uses EIP-relative low32 and truncates the complete base/index/disp sum,
then zero extends. Address64 uses unsigned modular64 address arithmetic. This
is address formation only: actual operand span must not overflow64, cross the
canonical48 boundary or a 4KiB page. An access beginning near 4GiB never wraps
individual bytes back to address0. LA57 and 16/32-bit code modes unsupported.

The caller must check current mode/segmentation, safely fetch at most15 bytes
via guest page translation, validate actual GLA/GPA against EPT exit evidence,
restrict the target device and access width, and own the full transaction.
Do not trust EPT-exit instruction length. Device accesses may be read-sensitive;
clone/plan changes before RIP/GPR commits and never replay an already committed
side effect. `guest_mmio_mov_load_value` only computes a destination value;
it does not decide whether a MMIO transaction can commit.

Tests: supported variants, high8 vs REX low8, 16-bit partial and32-bit zeroextend,
SIB special cases, immediate signextend, RIP/EIP-relative, every truncation of
valid examples, reserved/unsupported instructions, atomic rejection, and10,000
address32 reference cases. ASan+UBSan and freestanding compilation pass.
No QEMU, VMX or Linux test has been run. This is not a general x86 decoder.
