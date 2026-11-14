# NoDMA observer integration envelope

`pci_observer_candidate.py` requires an exact source SHA and new output path.
It changes only the private observer's board state/initializer/IO dispatcher,
redirects clock/log fields to the same live board object and caps delegated
board cycles4096. Existing surrounding clock_sync, CS/owned instruction fetch,
VMCS RIP callback and stop-on-false remain unchanged. It does not edit in place.
The count includes PM reads and A20/hotplug calls delegated through this path,
not just PCI config. Exceeding the bound terminates this diagnostic. Earlier
samples may already have modified virtual config state, never target RAM.

The inherited PCI-cycle log is retained rather than adding per-instruction
verbose output. This finite transaction cap does not guarantee wall-clock UART
latency or absence of measurement perturbation. Original outer run timeout and
cleanup remain mandatory. Do not interpret time-sensitive performance results.

Next build must use the frozen RDTSC-corrected input selected by the firmware
owner, not whichever older source happens to exist. Copy five production test
modules (backend/transport/bus/config/nodma) and their headers into an isolated
include directory, with explicit include resolution, and add object prerequisites
to only the isolated quiet target. Never link firmware_ram_adapter: it is a host
mapping seam and its publication counter must not enter the native candidate.

Required gate: final ELF actual observer references vfb_init_nodma_probe/vfb_io,
all guest-memory callbacks are constant false, no host adapter/lifetime bypass
symbol is linked, and RAM borrow/DMA behavior remains unavailable. Board observer
still executes existing firmware RAM for OVMF itself; only block-device target
RAM callbacks are refused. A noDMA disk BDF is a discovery diagnostic, not a
functional storage device. Root schedules once after review and KVM cleanup.
