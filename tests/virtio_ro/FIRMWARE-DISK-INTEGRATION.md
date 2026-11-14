# Real firmware RAM / read-only virtio integration pilot

Host-only, no default source change, no device in native boot. This advances the
block path instead of extending source-snapshot claims. `firmware_disk.sh` links
actual firmware_memory/layout, actual legacy transport/backend and a new bounded
RAM adapter. 128MiB of firmware RAM is prepared through the real allocator/EPT
construction code using an aligned heap-backed allocator fixture. Disk backing
is a separate8192-byte immutable allocation, never part of firmware guest RAM.
No real physical allocator, hardware DMA fence, native VMX or PCI bus is proved.

The tests drive actual BAR assignment/command bits/legacy status/PFN/notify and
check IN sector bytes, including MBR signature, last sector, capacity overrun,
RO OUT failure without a guest feature write, split-page transfer and used.len.
One injected map failure in the real firmware copier occurs after6 bytes of a
512-byte target write. The request/device latch failure, status/used index remain
unpublished, remaining506 bytes stay unchanged, and no retry is accepted. A
successful completion followed by an outer-instruction abort also cannot replay.
Flash/out-of-RAM/arithmetic overflow is rejected. Before releasing the firmware
object the fixture aborts device access. Cleanup succeeds with no live pages.

## Strict trust boundary

The controller must exclude all guest execution, parallel device activity and
release, and keep callbacks/ledger/backing stable throughout notify and enclosing
instruction commit. The adapter's owned_hash membership is merely consistency
checking under that contract; it is not a pin, mapping lease, permission proof,
or defense against forged trusted metadata/EPT aliases. Host buffers, disk and
device metadata must be disjoint from guest mappings. The publication counter
is explicitly not a hardware memory fence. No native mapper is opened.

## Minimum integration sequence

1. Keep this isolated test while adding a single PCI bus composition preserving
   existing firmware_piix_pm/fpi and its one CF8 latch. Do not install a second
   independent configuration latch. Route only claimed BAR IO after conflicts
   against existing board ports are checked.
2. Validate producer behavior. Current backend requires one pending head and
   polling NO_INTERRUPT. This is not Linux virtio-blk compatibility; interrupt
   routing and multi-head ownership require separate implementation/validation.
3. Replace host-only mapping/publication with proven firmware-owned backing and
   lifecycle serialization. A source RO snapshot says nothing about this target
   lifetime. Allocate disk independently; do not squeeze it into the8MiB static
   kernel window or use host raw block devices.
4. Connect instruction-commit failure to terminal VM/device shutdown, never
   replaying a potentially published used entry. Retain uncertain resources.
5. Only after review, use a separately hashed storage-enabled Linux config and
   disk image. The direct-loader Linux baseline currently has CONFIG_PCI=n and
   must remain unchanged. No native disk boot was attempted here.
