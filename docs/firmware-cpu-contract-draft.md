# Minimal firmware startup CPU profile

This profile is for bounded normal-path OVMF observation, not complete x86 or
machine-check virtualization. The older discovery-only draft was not run and
is superseded. Every advertised leaf now has a deterministic implementation.

## CPUID contract

- Leaf 0: maximum basic leaf 1, GenuineIntel synthetic identity.
- Leaf 1: synthetic family 6/model 0, one logical processor, APIC ID 0.
  EDX: FPU, MSR, PAE, MCE, CX8, CMOV, MMX, FXSR, SSE, SSE2.
  ECX: zero. No TSC/APIC/MTRR/PAT/PGE/PSE/MCA/VMX/XSAVE/AVX advertised.
- Extended maximum: 0x80000008. Extended leaf 1: LM only; no NX,
  SYSCALL/SYSRET, RDTSCP, 1 GiB pages or virtualization extensions.
- Extended leaves 2 through 7: reserved/optional fields are zero.
- Extended leaf 8: 32-bit guest physical and 48-bit virtual address widths,
  all remaining fields zero. Guest addresses are not host physical addresses.
- Out-of-range leaves return the highest basic leaf, matching Intel's
  out-of-range CPUID rule. These are non-indexed leaves; ECX is ignored.
  Do not interpret bits in an out-of-range response as the requested leaf.
- All outputs are 32-bit zero-extended values, never raw host CPUID results.
  No host cache, topology, serial or model identifiers are exposed.

## Enforcement and limits

Host startup checks every advertised basic/extended feature, physical width
32..52 and virtual width48..57. Missing capabilities stop before allocation or
VM entry, not by silently changing the guest profile. Existing native entry
saves/restores x87/MMX/XMM/MXCSR through FXSAVE/FXRSTOR; XSAVE and CR4.OSXSAVE
remain unavailable.

CR0.ET stays one. CR4 permits only PAE/MCE/OSFXSR/OSXMMEXCPT; VMX hardware
fixed-bit checks remain before mutation. Hardware CR4.VMXE is hidden through
the mask/read-shadow, not exposed as guest VMX. Both hardware fields and
software readback use the same normalized state. CR0.TS/EM may disable FPU
features normally; exceptions remain explicit stop points in this observer.

Guest EFER writes allow only LME/LMA fields, with LMA derived/read-only through
the mode model. SCE/NXE writes are rejected because those features are hidden.
No SYSCALL or NX capability is falsely promised. EFER is the only modeled MSR
in this observer; unsupported MSRs stop, never pass through to the host.

MCE and MCA are separate: MCE is advertised consistently with the observed
CR4.MCE write; MCA is not. No MCG/MCi banks or machine-check values are invented.
Any machine-check event or unimplemented machine-check MSR terminates the
probe rather than resuming the guest. This is an explicit hardware-error
termination policy, NOT transparent architectural #MC delivery. No physical
machine-check injection/testing is performed.

APIC, timer, fw_cfg, PCI, flash programming, storage and display are separate
platform milestones. Their absence may stop OVMF even with a consistent CPU
feature table. Unknown I/O/MMIO remains fail-stop. Budget stays64 exits and
successful cleanup still returns diagnostic35, never firmware boot success.

## Tests / acceptance

Host tests cover feature masks, maxima and every advertised extended leaf,
non-indexed semantics, out-of-range fallback, missing-host-bit rejection,
width rejection, output unchanged on invalid host and hidden EFER rejection.
Mode/reset-plan and direct-Linux planner tests remain separate regressions.
After independent review, run the unchanged tiny guest and the new bounded
OVMF observer serially, retaining every previous failure log.

VMX enforcement: primary control bit12 forces RDTSC exits; the observer does
not dispatch them, so a hidden-TSC instruction terminates observation rather
than leaking host time. RDTSCP secondary enable is absent, and exception exits
also terminate. Likewise any VMX operation exit is not dispatched. No hidden
extension is enabled by CPUID alone: CR4 feature bits, EFER and VMX controls
remain the enforcing gates. CR0/CR4 VMX fixed0/fixed1 checks are retained before
VMCS writes; default guest page faults still are NOT intercepted.

CPUID hiding is not universal ISA disabling: instructions such as SSE3/AES
without a distinct CR enable may still execute if host hardware supports them.
This contract is for the fixed, CPUID-following OVMF normal startup path, not
full instruction-set restriction against arbitrary hostile guest binaries.
