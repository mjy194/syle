# RAMFB offline host contract prototype

This is isolated host-only parsing and validation, not runtime integration. No VM, DMA, fw_cfg guest-write handler, mapping, guest framebuffer read, rendering, or execution-review bypass.

Fixed OVMF module and source proof: ../ovmf-spike/ramfb-offline-findings.md. Driver config is packed28 bytes, big-endian address64 and format/flags/width/height/stride32. Pinned driver emits XRGB8888 0x34325258, flags0, stride=width*4; modes640x480,800x600,1024x768. No claims of GOP installation or display acceptance.

The prototype accepts one exact28-byte buffer, one trusted nonempty nonoverflowing RAM interval and a disjoint output struct. Narrow profile: width16..1024,height16..768, XRGB8888 only, flags0, explicit nonzero stride>=width*4. Smaller dimensions/padded rows are accepted within these bounds for validation testing; these are not extra advertised GOP modes. Zero-stride QEMU shorthand is intentionally rejected (fixed driver emits explicit stride). Visible byte extent is stride*(height-1)+width*4, not trailing unused padding. All arithmetic is uint64 with checked multiplication/addition and subtraction-based range checks. Failed parse leaves output unchanged. Host does not access the supplied address.

Passing interval bounds is NOT owned-memory validation: caller must still prove guest RAM ownership, nonaliasing, page permissions, pinning/lifetime, and frozen snapshot rules. No DMA transport or full fw_cfg configuration transaction is implemented. No renderer or framebuffer lifecycle, no GOP or Linux framebuffer takeover validation. Current noDMA behavior unchanged.

Build/test:
cc -std=c11 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined ramfb_config.c test.c -o test
ASAN_OPTIONS=detect_leaks=0 ./test

Tests: fixed modes and >32bit BE address, short/long input, nulls, exact-fit/one-byte-short spans, padded stride, invalid format/flags, zero/underflow/huge dimensions, zero/short stride, address/range overflow, empty range, huge stride multiplication beyond32bit, rejection result unchanged and input bytes unchanged.

ASan/UBSan run PASS. LeakSanitizer disabled because runtime ptrace is incompatible; parser allocates no memory. This does not claim a separate leak-check pass.

Independent review: fresh ASan/UBSan compile/run PASS; no arithmetic/bounds issue found. Input must be a stable snapshot and output valid/disjoint; failure publication protection is not a concurrent atomic commit. No allocation or mapping occurs.
