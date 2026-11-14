# Legacy transport/state-machine host pilot

`transport.c`/`transport.h` are independent host-only scaffolding around the
single-head backend. No kernel build, real PCI bus, CF8/CFC path, OVMF driver or
Linux kernel calls them. `transport_test.c` provides local synthetic transactions.

Normative references: Virtio 1.2 legacy PCI interface §4.1.4.10 (including register
layout/configuration offsets), legacy PCI initialization §4.1.5.1.1, device status
§2.1 and feature negotiation §2.2, block feature/read-only semantics §5.2.5–6;
see the [OASIS specification](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html).
The exact observed OVMF/Linux transaction sequences still require measurement.
This is an explicitly limited profile, not a general PCI compliance claim.

Implemented:
- 256-byte synthetic type-0 configuration header: 1af4:1001, revision 0,
  class/subclass 01/00, subsystem 1af4:0002. No capability list or IRQ pin/line.
  Reserved/read-only bytes read zero (except fixed IDs); their writes ignored.
- Byte/word/dword aligned config reads, command IO/BUS_MASTER/INTX_DISABLE masks.
  BAR0 64-byte IO allocation, DWORD probe=ffffffc1, restore and relocation within
  16-bit IO space. Other BARs unimplemented. BAR decode disabled during probe.
  Partial BAR writes are explicitly unsupported terminal failures; this limitation
  must be removed or producer-proven before claiming a general enumerable device.
- Legacy register widths enforced: features/PFN DWORD; queue size/select/notify
  WORD; device status/ISR BYTE. Capacity bytes support byte/word/dword
  reads, no MSI-X displacement, little endian. Queue0 has 128 descriptors, other
  queues read size/PFN zero. Host features offer only RO; guest feature writes
  must be a subset. Not accepting RO never grants permission to write the disk.
- Status ACK/DRIVER/DRIVER_OK/FAILED. Monotonic nonzero status with required bits: DRIVER requires ACK,
  DRIVER_OK requires DRIVER; legacy does not require modern FEATURES_OK. This
  pilot rejects modern/reserved status bits rather than masquerading as modern.
  DRIVER_OK additionally requires an admitted queue in this narrow profile.
- Feature/PFN writes require DRIVER and are frozen after DRIVER_OK or first use. PFN must be
  nonzero, queue0, assigned once until status-zero reset. PFN-zero queue disabling
  and live reconfiguration are deliberately unsupported. Queue admission uses
  backend read/write ownership checks; no DMA execution occurs until notify.
- Notification requires DRIVER (with ACK), no FAILED, IO and BUS_MASTER enabled
  and queue0. Legacy §3.1.2 first use before DRIVER_OK is supported, including
  default zero guest features if no feature write occurred. First notify freezes
  features/PFN even if no request is pending. DRIVER_OK may follow first use.
  Pre-ACK/DRIVER traffic remains an explicit unsupported profile restriction. It invokes the existing polling/single-head backend. ISR remains zero;
  this profile rejects any completion requesting an interrupt. There is no INTx
  generation despite exposing the config command's disable mask.
- Status-zero reset drops device features/PFN/select/status/backend private state and first-use flag
  while preserving PCI BAR/command. It does not wipe guest memory. Caller must
  stop guest and all callbacks, revoke stale accesses, and reset guest producer
  indices before re-enabling. A latched terminal implementation failure cannot
  be cleared via guest reset; the VM must stop. Device FAILED before a terminal
  operation can still be cleared by status reset.

Trusted platform obligations: route only this function's decoded IO transactions;
verify BAR allocations do not collide with UART/PIC/PM/fw_cfg/other devices;
provide lifetime-stable admitted guest RAM and immutable disk; ensure exclusive
vCPU/device ownership; implement real DMA publication barrier; and terminally
stop if enclosing IO instruction commit fails after backend mutation. None of
these obligations is discharged by a passing host test.

Validation: strict freestanding C11/Werror and ASan+UBSan host tests for fixed IDs,
class/subsystem, BAR probe/restore, widths, feature selection, capacity, absent
queue, complete synthetic IN through notify, ISR polling, reset preservation,
invalid status/feature, post-ready feature rewrite, BUS_MASTER gate, FAILED gate,
invalid BAR address and disabled IO decode. Backend tests still run separately.
No native firmware/Linux storage validation has occurred. Batch ownership,
interrupt routing, actual bus enumeration and producer compatibility remain open.

Review-driven additions: synthetic first-use IN before DRIVER_OK without any
feature write; first-use RO OUT with zero accepted features still IOERR; reject
feature/PFN rewrites after first use; reset and fresh queue restart; legal
FAILED-to-reset; inaccessible PFN; BAR relocation/new-port decode and maximum
16-bit base; command byte masks; unaligned capacity reads and boundary rejection;
unsupported partial BAR write. Old-port routing exclusion remains a platform-bus
obligation, not a tested bus feature. No observed OVMF/Linux producer trace is
claimed by these synthetic cases.
