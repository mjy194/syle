# Next isolated OVMF diagnostic: enumerate only, DMA refused

Not built into a frozen kernel or run. Current firmware RDTSC work has priority;
its isolated source/hash must first be frozen. Do not modify that candidate.

The next diagnostic should reveal the pinned OVMF's actual configuration reads,
writes and resource choices without permitting any guest-memory operation by
our block backend. `pci_probe_nodma.c` provides one explicit initializer for
this purpose. All admit/read/write callbacks unconditionally return false without
dereferencing addresses/buffers. PFN setup fails terminally before a queue is
accepted. The512-byte zero placeholder only permits a capacity register to exist;
no request can read it. It is not a usable disk or source lifetime proof. Its
empty publication function is unreachable through valid queue initialization,
not a DMA fence; it must never be combined with real-memory callbacks.

## Proposed exact isolated changes after current firmware freeze

- Replace the private observer's `static fpp_state pci` with a private vfb object,
  and direct current clock_sync/logging references to its one `board.pm` and
  `board.pci.address`. No duplicate CF8 or timer state.
- Replace only fpp_init/io calls with the no-DMA initializer and vfb_io, preserving
  existing CS/encoding/owned-fetch/clock/VMCS-write admission, stop behavior and
  unrelated IRQ/RDTSC functionality. Existing gate false must stop, not retry.
- Capture bounded metadata for PCI config and BAR attempts: address/width/direction,
  old/new register values, result/RIP and terminal reason. Avoid per-exit serial
  flooding; RAM trace ring dumped on terminal exit, no performance claim.
- Build with exact kernel ABI; inspect final ELF refusal callbacks and callsites.
  Host shadow test the actual observer dispatch seam, not just a model.
- Run only after independent review, complete artifact hashes and root scheduling.
  No QMP memory/register writes, host devices, host NMI or concurrent firmware VM.

## Known reasons this is not ready to boot yet

The originally added PCI command.IO double gate was stricter than the selected
QEMU PIIX4 legacy PM behavior. Source review and host tests corrected that
opt-in profile to PMBA/PMIOSE decode, without forcing command reset1. Actual
BaseRomAcpiTimerLib-style PMBA→PMIOSE→timer reads now pass with command0.
See PIIX-PM-DECODE-EVIDENCE.md. This is static/host evidence, not a new guest run.
PIIX7110/7113 identities are currently synthetic diagnostic choices; validate the
resulting pinned driver's probes before advertising them in native. Unknown PAM,
SCI/SMI/power registers remain explicit stop conditions. D000-only resource
allocation is diagnostic, not a claim the firmware chooses that aperture.

The first successful observation can at most show actual driver discovery and
configuration up to the refusal boundary. Disk block delivery, filesystems,
Linux shell, native mapping leases/fences, interrupts and multi-head queues are
separate acceptance steps. Default loader/profile stays unchanged throughout.
