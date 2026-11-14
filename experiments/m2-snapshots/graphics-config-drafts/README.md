# Linux6.12 graphics configuration drafts: Kconfig only

Resolved offline with the existing built scripts/kconfig/conf from syle-linux-rebuild-20261113.he23fF/build-3/objects, against its linux-6.12/Kconfig. Two independent allnoconfig runs, no kernel compilation or VM. Both fragments had zero requested-value mismatches. Per-profile fragment, resolved.config and resolve.log are preserved.

Source dependencies inspected directly: arch/x86/Kconfig EFI depends on ACPI; EFI_STUB depends on EFI and selects RELOCATABLE. FB_EFI needs built-in FB and EFI and selects SYSFB. DRM_SIMPLEDRM requires DRM/MMU; SYSFB_SIMPLEFB supplies generic system framebuffer; DRM_FBDEV_EMULATION supplies console support through FB_CORE. FRAMEBUFFER_CONSOLE needs FB_CORE and VT path. VT selects INPUT, not a working keyboard device.

Profiles:
- efifb: EFI/EFI_STUB/ACPI/FB/FB_EFI/VT/VT_CONSOLE/FRAMEBUFFER_CONSOLE=y; DRM and SYSFB_SIMPLEFB=n.
- simpledrm: EFI/EFI_STUB/ACPI/DRM/DRM_SIMPLEDRM/DRM_FBDEV_EMULATION/SYSFB_SIMPLEFB/VT/VT_CONSOLE/FRAMEBUFFER_CONSOLE=y; legacy FB_EFI disabled. FB_CORE is selected although legacy FB=n. This is intentional, not a missing fbcon dependency.
Both retain serial8250 console, earlycon, external initramfs, devtmpfs/proc/sysfs and existing userspace fixture capabilities. LOCALVERSION is separately named graphics-draft1. Original UART-only kernel/config remain unchanged: bzImage fc7e0e06e1b92768859b38fdf4b0e3bde80b9ec0511aaa40f2c952c3105952e1, config03b49a3ed3ce103d337df4fca29e6003ea15fc3c6245204e5eccce9340916a58.

Scope expansion: EFI on x86 requires ACPI=y in this source; actual ACPI tables/interpreter/device probes and EFI memory-map/runtime behavior need later verification, not assumed implemented by the current VT platform. Graphical features may consume more memory and add execution paths.

Root/boot gaps: both drafts still BLOCK=n, PCI=n, no virtio-blk/ext4 disk-root support. External initramfs support is not a boot-device transport. Need explicitly choose EFI loader/stub image delivery and initrd delivery (e.g. verified filesystem/LoadFile2 arrangement) or separately provision a real supported block/root path. Enabling virtio drivers alone would not implement missing device queues/DMA.

Display acceptance gaps: ramfb config transport/owned backing/render bridge; actual OVMF GOP; EFI stub GOP-to-screen_info handoff; driver bind; console output plus shell process tied to the correct VT; usable input. Keep serial diagnostic console and specify console ordering deliberately. Current configs do not change the old UART-only init or commandline.

This is Kconfig-resolved proposal only: no new bzImage, boot success, GOP, framebuffer output, keyboard, disk-root or graphical shell validation. No VM, DMA integration or safety-review bypass.
