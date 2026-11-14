# Independent Linux6.12 serial disk-root configuration proposal

Resolved using the actual Linux6.12 source and existing conf binary from syle-linux-rebuild-20261113.he23fF/build-3. allnoconfig with separate fragment/config/log. Every requested setting matches resolved output. No kernel compilation, VM, block-image creation or device access.

Built-in chain: BLOCK, BLK_DEV, PCI, PCI_DIRECT, VIRTIO, VIRTIO_PCI, VIRTIO_PCI_LEGACY, VIRTIO_PCI_LIB_LEGACY, VIRTIO_BLK, MSDOS_PARTITION, EFI_PARTITION, EXT4_FS, JBD2, CRYPTO_CRC32C=y. MODULES=n; no initramfs module loader is required for these drivers. Existing serial8250 console, external initramfs and userspace capabilities retained. EFI_PARTITION is GPT parsing and does NOT imply EFI stub. ACPI remains n; EFI not enabled; PCI_MSI=n, so actual INTx routing/delivery must work. This is separate from graphics drafts and not their silent merge.

Source dependency evidence: drivers/virtio/Kconfig VIRTIO_PCI depends PCI/selects VIRTIO and LIB; LEGACY depends PCI driver/selects LIB_LEGACY; drivers/block/Kconfig VIRTIO_BLK depends VIRTIO/selects SG_POOL; fs/ext4/Kconfig selects BUFFER_HEAD,JBD2,CRC16,CRYPTO,CRYPTO_CRC32C,FS_IOMAP. block/partitions/Kconfig defines MBR/GPT choices; EFI_PARTITION selects CRC32. Resolved config is authoritative, not fragment intent.

Proposed image layout, NOT created:
For a firmware+disk route use a regular dedicated image, GPT, p1 FAT32 EFI System Partition with an actual EFI loader, p2 ext4 Linux root, fixed recorded disk/partition IDs and hashes. Align partitions at1MiB boundaries and compute sizes from actual artifacts; protect GPT backup header. This serial draft has no EFI stub, so copying its bzImage to BOOTX64.EFI is invalid: use a verified loader or a separately resolved EFI-capable kernel. Firmware boot-media access and Linux virtio root-device access are separate acceptance gates. Linux need not mount ESP for this root-only test; FAT driver is not included here.
For a direct-loader-only isolation test, one ext4 p1 may suffice; that does not validate OVMF disk boot. Never use a host physical disk or existing filesystem for this draft.

Proposed root argument once a real image and identity exist:
console=ttyS0,115200 earlyprintk=serial,ttyS0,115200 nokaslr root=PARTUUID=<actual-ext4-partition-id> rootfstype=ext4 rootwait=10 ro
Linux6.12 init/do_mounts.c supports bounded rootwait=; block/early-lookup.c resolves PARTUUID and /dev/vda2-style names. Prefer recorded PARTUUID rather than unstable enumeration. Filesystem UUID= is not equivalent to PARTUUID and requires appropriate userspace support. All placeholders remain unresolved until image creation.

Root acceptance pitfalls: existing initramfs /init and rdinit=/init can keep the shell in RAM. To prove disk root, either boot without that initramfs after confirming built-ins and /sbin/init, or explicitly mount and switch_root in a separately reviewed init. Ensure ext4 contains /sbin/init or a chosen executable init, interpreter/libraries when nonstatic, console/device setup and usable shell. Initial ro mount is not a no-write guarantee: ext4 journal recovery can issue writes; use a clean image and define noload/read-only backend policy if strict no-writes are required. No filesystem features/identity/content were validated here.

Platform gaps: noDMA PCI discovery is not a functioning virtio block backend. Need actual BAR assignment/decode, queue/features/status negotiation, trusted descriptor/buffer ownership, bounded block IO/error/reset handling and interrupts before mounting root. Configuring these drivers does not relax any current noDMA or execution restriction.

Original baseline unchanged: bzImage fc7e0e06e1b92768859b38fdf4b0e3bde80b9ec0511aaa40f2c952c3105952e1; config03b49a3ed3ce103d337df4fca29e6003ea15fc3c6245204e5eccce9340916a58. New LOCALVERSION=-syle-diskroot-draft1. Kconfig resolution is not boot, disk, root, shell or display acceptance.
