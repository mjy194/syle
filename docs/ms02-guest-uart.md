# Milestone 2: polled guest console foundation

## Requirement boundary, checked 2026-11-12

The confirmed direction is a single-vCPU simple guest system before Windows,
with guest memory, interrupts/timing, then firmware, storage and display.
Ring 3 management belongs in milestone 2; early networking and multiple vCPUs
are not required. An independent assembly diagnostic is a useful intermediate
check, not completion of the simple-system milestone.

The current milestone target is a single-vCPU lightweight Linux interactive
command line, with Ring 3 guest start/stop management. Firmware, storage and
display remain milestone work; direct bzImage boot is an intermediate step.
The TX-only UART foundation below does not meet interactive input requirements.
The earlier implementation and runtime evidence are retained as bounded
prototype evidence, not current milestone acceptance.

## Reusable capability added

The independent-image runtime now emulates a minimal polled COM1 register
interface at ports `0x3f8..0x3ff`. Ordinary guest `IN AL, DX` and `OUT DX, AL`
work without a private print hypercall. This is useful for early boot console
output across boot protocols; it does not select a final OS or virtual device
platform. The implementation is pure C and never passes guest I/O through to
the host hardware.

The device supports 8N1 output, DLAB divisor-register aliases, a scratch
register, immediate transmit completion and no-interrupt-pending status. FIFO
enable/clear/trigger register settings are accepted, but there is no queued
data. Divisor and modem-control values are stored without timing/line effects.
`LSR` is `0x60`, RBR and MSR are zero. IER must stay zero. Unsupported framing,
loopback, nonzero interrupt enables and writes to LSR/MSR are rejected.

This is **not a complete 16550**, and has no input, interrupts, baud-rate
timing or interactive terminal. NUL is represented distinctly from no emitted
byte by the model and dispatcher, but the existing host text logger does not
display NUL. It is an early text-output device, not a binary stream service.

The runtime keeps unconditional I/O exiting enabled and I/O bitmaps disabled.
It accepts only byte DX-form accesses, length one, a matching qualification
port and DX low word, and a full instruction span in file-backed executable
ELF bytes. String, REP, prefixed, immediate, wider and unknown-port I/O exits
are rejected. Reads change only AL, preserving the high RAX bits. Device and
register changes are committed only after validation. Unsupported access
makes failure sticky without partially changing device state or GPRs.
The existing 256-entry/output limits and per-entry preemption budget remain.

## Build and focused verification

```sh
. efi/scripts/cloud-env.sh  # workspace-specific GNU-EFI location
make -C efi simple-guest-uart
bash efi/tests/simple_guest_uart_host.sh
```

The new target creates `efi/build/simple-guest-uart-esp`, using the same EFI,
kernel and configuration as the original `simple-guest` target, but a separate
`guest/uart/main.S` ELF. The original VMCALL-printing sample is unchanged.
Normal `make all` does not depend on either opt-in guest target.

The UART sample checks its provisional boot record, BSS, divisor aliasing,
FIFO status, scratch storage and IN AL preservation, prints
`Hello via a virtual COM1 UART!`, and finishes with status zero via VMCALL.
It remains trusted, integer-only assembly under the provisional ELF ABI.

Focused ASan/UBSan tests cover all 256 byte values, register aliasing,
unsupported accesses, invalid qualification bits, atomic rejection, executable
spans and budgets shared with CPUID/VMCALL. The actual built UART ELF passes
the unchanged strict image/memory loader tests. The first sample build had an
empty RW PT_LOAD due to the shared linker layout; the loader correctly rejected
it. Adding a real BSS zero-fill/write/read self-check fixed the sample. The
failed log is retained; the loader and its acceptance policy were not relaxed.

## Actual VM-entry verification

The exact delivered EFI/kernel and independently built images were run on
archbook with QEMU 11.1.3, nested KVM and one vCPU. Each run used fresh OVMF
variables and a private ESP, with no networking, host disks, QMP or GDB:

- UART sample, 256 MiB: full success chain and UART greeting, exit 33.
- UART sample, 512 MiB: full success chain and UART greeting, exit 33.
- Original VMCALL sample with the same new kernel, 256 MiB: original greeting
  and full success chain, exit 33.
- Independent unknown-port guest (`OUT DX, AL`, DX=0x80): actual entry, I/O
  exit reason 30 rejected, VMX round trip and incomplete marker, exit 35.
- Independent nonzero-IER guest: actual entry, I/O exit reason 30 rejected,
  VMX round trip and incomplete marker, exit 35.

All required markers appeared exactly once and in order. No success marker
appeared in the two rejection cases. Full logs, commands, input hashes and
negative-image sources were independently checked, including all 38 evidence
manifest entries. Existing R1 failure and R2 evidence remain preserved.
This establishes the bounded UART device path, not an OS or milestone pass.

The isolated source delivery is aligned with dev commit
`338fe034a3c345e082ef644653b29707ffc8c8b0`. Its 702-file base was recomputed
and matched the remote tree `51f52754dd967be3c04c355fb4dd81d0b0004d15` before
integration. The M1 supplement contains only tests/docs/runner changes; all
production guest, kernel and EFI inputs and binary hashes used above remain
unchanged. The normal kernel's and original sample's PT_LOAD runtime bytes
also match R2. The cumulative prototype patch is checked against that exact
dev base. No push or modification to the existing dev checkout is performed.

## Remaining work

Linux loading/boot, RX and IRQ delivery, lifecycle management, firmware,
storage and display require separate implementation and verification. These
UART prototype tests do not establish Linux or milestone completion.
