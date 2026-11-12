# First-phase user program loading

The normal kernel path loads the file supplied in `boot_info.user_image` after
ExitBootServices. The boot layer reads it while firmware file services are still
available; the kernel never calls EFI filesystem services. `user_elf_load` validates
all load segments before copying or zero-filling the reserved user pages.

## Supported executable contract

- Little-endian ELF64, x86-64, ET_EXEC, at most 16 program headers.
- Exactly one nonempty RX PT_LOAD at `0x8000000000`, at most 4096 bytes.
- Optional one nonempty RW/NX PT_LOAD at `0x8000001000`, at most 4096 bytes.
- The entry must be in file-backed bytes of the RX segment.
- `filesz <= memsz`, offsets and sizes must lie inside the supplied file;
  alignment must be a power of two and file/virtual offsets congruent.
- PT_NULL is ignored and non-executable RW PT_GNU_STACK is accepted; all other
  segment types, overlapping segments and W+X permissions are rejected.
- The stack is provisioned by the kernel at `0x8000003000`, with a guard page
  below. No PIE, relocations, dynamic linker, shared libraries or arbitrary
  virtual layouts. This is intentionally a bounded loader, not general ELF support.

`user/mgmt/link.ld` produces a compatible ELF. The kernel uses its own CR3 and
kernel/syscall stacks; RX user code and RW/NX data/stack are separate from the
supervisor-only low identity map. A normal management exit prints `MGMT.EXIT`
and `KRNL.IDLE`, then halts rather than writing QEMU's debug-exit port. User
faults are fatal to this prototype; process recovery and scheduling are not yet
implemented. Interrupts remain disabled and there is one bootstrap CPU.

The existing diagnostic images still embed the flat program linked at address
zero, accept test selectors, assert exact exception details and exit QEMU. This
separate path deliberately keeps the five isolation regression tests unchanged.

## Loader unit checks

```sh
cc -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  krnl/user_elf.c krnl/tests/user_elf.c -o /tmp/syle-elf-test
ASAN_OPTIONS=detect_leaks=0 /tmp/syle-elf-test efi/build/mgmt-runtime.elf
```

`detect_leaks=0` is needed in the current ptrace-hosted executor; address and
undefined-behavior sanitizers remain enabled. Tests cover truncated headers,
invalid type/machine/class/endianness, program-header sizes and counts, overflow,
file bounds, segment sizes, alignment, entry point, permissions, duplicates,
zero filling and the real linked management ELF. Invalid input does not modify
the destination pages. QEMU end-to-end tests are a separate integration check.
