# MS-01 boot candidate automation

The numbered scenarios remain draft candidates. The policies below are safe
implementation choices, not claims of user-confirmed product decisions.

## Production changes

- `exit_services.h` is the actual transaction called by the EFI adapter. It
  accepts only nonempty complete descriptors (minimum 40 bytes, version 1),
  permits at most three map-buffer allocations with 32-descriptor slack and a
  1 MiB capacity bound, and at most three ExitBootServices calls. It refreshes
  the key each time. Before the first EBS attempt, failed preparation cleans up
  and returns an error; after any EBS attempt, it never allocates/frees or returns
  to firmware. Only map/exit calls are allowed; final failure uses the loader's
  hardware-only serial/framebuffer output and halt. Growth after a failed EBS is
  deliberately fatal rather than using potentially unavailable services.
- `kernel_prepare.h` is the production allocation/load transaction. Invalid
  later segments cause no allocation; address conflicts cause no writes;
  successful allocation at an unexpected address is released and rejected.
- `read_bounded.h` is the production read core. A second read establishes EOF;
  errors, over-capacity input, and short reads with continuation fail without
  publishing a byte count. Missing-file open failures remain EFI-adapter logic.
- Existing menu policy retained: missing config uses normal/3 seconds; unreadable
  or invalid config is whole-file diagnostic/0 fallback. Unknown/duplicate keys
  are invalid. Printable ASCII title up to 80 characters and total file up to
  4096 bytes. No keyboard means immediate default. Zero timeout prints the menu
  but does not poll. Thirty seconds polls 300 times before each 100ms stall;
  a valid key in the final poll wins, a key arriving at/after expiry does not.

## Dynamic tests and candidate mapping

Run from repository root:

```
boot/tests/exit_services.sh
boot/tests/read_bounded.sh
boot/tests/policy.sh
boot/tests/kernel_elf.sh "$(pwd)/efi/build/krnl.elf"
```

All compile and execute the production cores with ASan/UBSan and strict warnings.
LSan is disabled because this executor runs under ptrace. Fake firmware callbacks
assert call ordering and reject allocation/free after an EBS attempt. They are
not physical firmware fault injection.

- 003/004/005: map growth succeeds, growth budget exhausts, arithmetic/capacity
  bounds reject, allocation/map/exit failures reject, malformed map rejects,
  stale keys refresh, retries exhaust, no allocation/free after failed exit.
- 007/008/009/010/011/012: actual parser/menu callbacks verify one launch,
  normal/diagnostic/Enter selection, unavailable input, missing/read-failed
  config, invalid fields/duplicates/unknown keys/non-ASCII, exact config/title
  limits, timeout 0/30, and 29.9s versus 30s key arrival. Config file opening and
  rendering on real firmware require integration tests.
- 013/015/016/017/019/020: actual ELF parser/loader verifies segment bytes/BSS,
  guard bytes, transactional invalid-late-segment rejection, corrupt headers,
  ranges/overlap/permissions/entry, dynamic format rejection, symbol metadata
  tolerance, exact 4 MiB file and 8 MiB span acceptance and one-byte-over reject.
  Also runs against the built kernel ELF.
- 018: actual allocation/load seam refuses occupied fixed address without writes,
  and rejects/releases an unexpected returned address. Host allocator injection,
  not a physical firmware reservation-conflict test.
- 014: actual stream-read core rejects I/O failures, short-read continuation and
  over-capacity files; EFI open-not-found and no-jump integration remain to be
  exercised by a QEMU fixture. A file truncated to a still-valid self-contained
  ELF is indistinguishable from an intentionally shorter valid file.
- 001/002: EFI build passes; actual firmware entry/EBS/kernel transfer require
  QEMU smoke automation (parent integration). 006: shared ABI validator belongs
  to the parent integration, not these boot-only tests.

These results do not certify all physical UEFI implementations or replace
hardware/QEMU tests of CPU state, page permissions, or exception delivery.
