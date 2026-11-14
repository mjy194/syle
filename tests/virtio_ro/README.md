# Isolated read-only virtio-blk C pilot

This is **host-only experimental code**, deliberately outside `krnl/` and all
normal build paths. It is not a PCI device, a firmware integration, or evidence
that OVMF or Linux can use storage. Run `sh tests/virtio_ro/run.sh`.

## Normative basis and deliberately narrow profile

Based on the accompanying reviewed `docs/m2-virtio-blk-plan.md` and the OASIS
[Virtio 1.2 specification](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html),
particularly split ring §§2.7.2, 2.7.4.2, 2.7.5.2, 2.7.8.2 and block requests
§§5.2.6.2–3. This is a partial implementation of those contracts, **not** a
conformance claim. The executable Python model is a comparison aid, not a trusted
admission gate or a substitute for the specification.

- Legacy, little-endian, N=128, PFN-based 8192-byte ring allocation.
- Only NEXT/WRITE; no INDIRECT or EVENT_IDX. Queue reserved allocation and every
  descriptor span must be owned RAM according to the trusted adapter.
- Canonical 16-byte readable header and one-byte writable final status;
  zero-length descriptors are rejected. Readables must precede writables.
- No buffer alias with any byte of the reserved ring allocation; writable buffers
  may not alias any other request buffer. Read/read aliases allowed.
- Immutable whole-sector disk; IN, GET_ID and explicit RO OUT error. Unknown
  commands return UNSUPP after chain admission. No host files/devices opened.
- **One outstanding request only**, avail.NO_INTERRUPT required. A batch is a
  terminal unsupported-profile failure, not silently dropped or partly processed.
  This restriction prevents claiming unimplemented batch/cross-head ownership.
  It is unsuitable for general Linux integration until expanded and retested.
- No advertised small data-size limit; bounded chain of 128 descriptors. The
  representable used length must fit uint32. Arithmetic rejects wrapping spans. This C pilot additionally requires the
  half-open end to fit uint64, rejecting a span containing byte UINT64_MAX;
  the Python model can represent that endpoint using arbitrary precision.
- Full data/status/element write success precedes release callback and used.idx.
  used.len is the contiguous written prefix of concatenated writable buffers:
  oversized GET_ID=20, exact20=21, unsuccessful IN with untouched data=0.

## Required integration contract, not implemented here

Caller proves offered RO, legacy readiness (including pre-DRIVER_OK first use), bus-master enable, valid
queue setup and polling-only operation before calling. Guest execution and all
other DMA writers must be excluded throughout the call. IO callbacks are trusted
and nonreentrant; admission proves stable backing ownership and permissions for
the entire span. They must not resume the guest or change disk/buffer metadata. Disk backing must never be mapped as guest DMA
RAM. These are trusted adapter obligations, not ownership proofs supplied by
this host test. Offered RO requires rejecting OUT regardless of whether the
driver accepted that feature; this backend always rejects OUT.
`release` must implement the platform DMA publication barrier. The host test only
checks the event trace data → status → used element → release → used.idx for
a successful IN, not hardware visibility. RAM page mapping/allocation, VMX/EPT,
status/features, interrupts, PCI, notification-instruction RIP commit, transport
reset and vCPU lifecycle are deliberately absent.

A failing store may already have changed **all** requested bytes, including
used.idx. Every such failure permanently latches `failed`; callers must not retry
or resume the guest. Failure completing the enclosing notification instruction
must likewise terminally stop the integration even after backend success. There
is no rollback claim. Reset/reinitialization requires exclusive ownership with
no callback in flight, a newly reset guest queue, and revoked old request access;
calling `vro_init` on a live queue is invalid.

## Host verification

Strict C11 `-Wall -Wextra -Werror -pedantic -ffreestanding` object build and
ASan+UBSan test executable. LeakSanitizer is disabled because this executor is
traced (the code performs no allocations). Cases include successful read,
GET_ID exact/oversized/undersized, disk-range IOERR, RO OUT with immutable disk,
unknown request, partial-effect failures at data/status/used-element/used.idx,
no retry after failure, ring/data alias, wrapping address, loop, indirect,
unsupported batch, uint16 rollover, idle no-repeat completion, six metadata/header
read failure points, scatter/gather IN/GET_ID, direction inversion for unknown
requests and missing polling suppression.

Still required: independent review, adversarial admission failure injection,
full queue ownership/reset tests, producer compatibility, real DMA integration,
PCI/IRQ routing, firmware driver binding and Linux mount/shell tests.
