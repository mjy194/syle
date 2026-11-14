# Boot producer source seam: isolated host candidate

`sh tests/virtio_ro/boot_source.sh` compiles actual legacy linux_boot.c and an
exact-count generated candidate side by side. No repository core source is
modified. Candidate writer gets an explicit source classification at each
producer callsite, including generated bp[4096] and q[8]/last q[2].

This classification is trusted compiler-internal provenance, NOT an unforgeable
source capability or permission to accept a caller's arbitrary stack pointer.
The fixture admits image/initrd/cmd only within its own private resident inputs. Image is copied from an external
fixture array; initrd and cmd are constructed directly in private arrays. The
private input exists before both inspect calls and remains unchanged through load;
mutating the original input afterwards does not change the private input.
Snapshot acquisition was single-threaded and NOT an atomic capture of live
user/guest memory. An actual controller must supply that lifetime/exclusion.

Generated parameters/entry values are immediately copied to private scratch,
then synchronously consumed using bounded_span page-local chunks. No generated
stack pointer is retained past the callback, nor returned to another consumer.
All these destinations are guest RAM, including initial CR3/GDT/TSS writes.
The fixture has no guest concurrent writers. It does not map real owned pages,
resolve capabilities, grant EPT permission or implement DMA.

The test verifies all 128 MiB match the original boot producer byte-for-byte,
plan equality, all entry-state fields (compared individually, not padding), all five provenance classes,
wrong-buffer/overrun/source-width rejections, and 86 sequential transfer-failure
positions, each BEFORE writing that chunk, not mid-memcpy. Each injected failure latches terminal no-write state; partial output
is not retried or published as complete. This is source/geometry behavior, not
LGM allocator rollback evidence. The later actual LGM integration must retain
capabilities on every failure and leave prepared/eptp unpublished.

ASan/UBSan and strict warnings are enabled. Leak checking is disabled in the
ptraced runner; fixture heap buffers are explicitly freed. Neither native
mapping nor the default loader path is enabled or changed.
