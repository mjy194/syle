# Guest GOP, Linux framebuffer and host presentation accepted (2026-11-18)

The Ring3-managed UEFI + writable disk runtime now has an end-to-end display
path. Native acceptance is
`efi/build/graphics-no-ps2-20261118/graphics-analysis.json` (`PASS`). Actual UEFI
GOP pixels and Linux `/dev/fb0` pixels were copied from private guest RAM into
the host boot framebuffer and independently checked in QMP screenshots. The
same run retained the previously accepted pause/resume, persistent disk and
complete destroy/recreate lifecycle.

## Implementation

The opt-in `FFC_RAMFB` firmware profile advertises fw_cfg DMA and one directory
entry, `etc/ramfb` (selector 0x20, 28 bytes). The existing default profile still
rejects DMA and has no files. The directory and DMA descriptor use the QEMU
wire layout. `firmware_ramfb.c` implements bounded READ/SELECT/SKIP and the
exact ramfb configuration WRITE through 32-bit I/O at 0x514/0x518. This copies
between emulator buffers and owned guest RAM; it does not program physical
DMA or assign a GPU.

Every request validates the big-endian descriptor, control bits, bounded
length (at most 64 bytes), guest ranges and ownership. The only writable file
is one complete 28-byte ramfb record with XRGB8888, flags zero, dimensions
16..1024 by 16..768 and explicit stride. The parser is promoted from the
previous host contract prototype. Descriptor/data overlap, overflowing or
non-RAM framebuffer spans and unsupported requests stop the session.

The DMA transaction stages the fw_cfg cursor and display configuration. Its
RAM writes reuse the owned firmware target validator, which also excludes
the current instruction and its page-walk pages. Descriptor control completion
is written before the architectural RIP commit; a partial RAM or VMCS failure
is terminal and never retries a completed transfer. Guest state is published
only on full success. The saved display configuration is part of the firmware
state covered by the existing STOP/START integrity digest.

`firmware_display.c` validates and reserves the host boot GOP target, then
reads guest scanlines only while its vCPU is stopped in a management callback.
It translates guest physical addresses through owned firmware RAM callbacks,
checks the framebuffer span, converts RGB/BGR channel order as needed, and
copies visible pixels. No guest address becomes a host pointer. Presentation
is throttled to at most ten regular frames per reference second, with a forced
final frame on STOP. Serial diagnostics remain independent; host text logging
is switched to serial so it cannot overwrite guest scanout.

DESTROY clears the host screen and revokes scanout before guest RAM is freed.
The next CREATE starts with empty display/DMA state. The currently admitted
host target is at least 1024x768 (native QEMU target: 1280x800); the acceptance
guest mode is 800x600. This is a bounded framebuffer implementation, without
3D acceleration or a general GPU device model. Input remains the previously
accepted Ring3 serial-console path.

Protocol references: [QEMU fw_cfg interface](https://www.qemu.org/docs/master/specs/fw_cfg.html),
[EDK2 QemuRamfbDxe](https://github.com/tianocore/edk2/blob/edk2-stable202502/OvmfPkg/QemuRamfbDxe/QemuRamfb.c),
[QEMU 10.0 ramfb implementation](https://github.com/qemu/qemu/blob/v10.0.0/hw/display/ramfb.c).
These explain the protocol; actual compatibility is established by the native
pinned OVMF binary and screenshots below.

## Native evidence

The actual pinned OVMF driver discovers ramfb through fw_cfg and installs GOP.
The disk EFI application locates GOP, selects its 800x600 BGRX mode, checks
metadata and writes four quadrants into the framebuffer. It prints
`SYLE_GOP_PATTERN_READY`, keeps the pattern for two guest seconds, then launches
the Linux EFI stub. QMP screenshots from both fresh guest generations contain
exact red, green, blue and white samples at (200,150), (600,150), (200,450),
(600,450). These are guest pixels, not host text rendering.

The new Linux 6.12.0-syle-gfx1 fixture enables EFI framebuffer, framebuffer
console and `/dev/fb0`, retains the ext2 disk root and empty initramfs, and uses
both `console=tty0` and serial output. Native Linux reports:

```text
efifb: framebuffer at 0x6a3f000
efifb: mode is 800x600x32, linelength=3200
fb0: EFI VGA frame buffer device
```

The disk-root `/bin/fbprobe` opens `/dev/fb0`, verifies fixed/variable screen
information, writes cyan, magenta, yellow and dark-gray quadrants through the
Linux framebuffer device, and holds them for two seconds. QMP independently
checks the same four coordinates against exact RGB values before the helper
prints `SYLE_FB_HANDOFF_VERIFIED`. The host screenshot is
`native/positive-once/linux-1.png`; UEFI screenshots are `gop-1.png` and
`gop-2.png`. The raw PPM captures and their hashes are retained.

The same native run then writes/fsyncs the persistence token, pauses the same
guest, checks 256 stopped STATUS calls and a rejected STEP, resumes the same
handle with unchanged digest and exit count (108,864), and verifies shell PID,
variable and persisted file. It remounts ext2 read-only and destroys the guest.
A fresh handle boots UEFI/Linux again, verifies the same file, and is destroyed.
Both stale handles are rejected. Ring3 exits successfully; QEMU returns 33
in 36.61 seconds. There is no host serial command injection.

| Check | Generation 1 | Generation 2 |
| --- | ---: | ---: |
| Valid ramfb configuration writes | 2 | 2 |
| Completed fw_cfg DMA requests | 150 | 150 |
| Presented frames | 126 | 88 |
| Presented visible pixels | 60,480,000 | 42,240,000 |
| PMM pages before CREATE / after DESTROY | 262 / 262 | 262 / 262 |
| Scanout revoked after DESTROY | 1 | 1 |
| Guest sector reads / writes | 7,441 / 114 | 7,431 / 40 |
| Ring3 RX sent / delivered | 283 / 283 | 122 / 122 |
| Guest TX produced / received | 22,823 / 22,823 | 22,525 / 22,525 |

Both releases also report VMX inactive, zero VMXON/VMCS pages, EPT released,
disk adapter revoked and no selected-trace failure/drop. Offline `debugfs`
verifies the same 33-byte file as the preceding lifecycle checkpoint; `e2fsck
-fn` returns zero. The complete 1,614-file source manifest and initial native
media manifests were checked. Tiny and no-Hv controls pass with no L1 WBINVD.
The paused-QMP hardware graph and prior host/module admission remain intact;
no display hardware or passthrough device was added.

The first graphics attempt already produced correct GOP pixels. Linux then
stopped at an unsupported PS/2-controller access (0x64) because enabling VT
had selected default keyboard/mouse drivers. Its evidence is preserved in
`efi/build/graphics-first-20261118`. Disabling those drivers and SERIO in a new
Linux build completed the accepted run; no PS/2 emulation was added. This
follow-up observed three L1 WBINVD instructions (one in that attempt, two in
the accepted run). The cumulative ledger is now 81 observed plus one historical
lost-capture uncertainty: **81..82**, next conservative prior upper bound 82.
These count architectural L1 instructions, not physical cache flushes.

## Reproduction and milestone scope

See `experiments/m2-snapshots/graphics-progress/README.md`. This profile is
prepared from the frozen Ring3 lifecycle source; it preserves the accepted
platform overlays rather than substituting the older checkout observer.
The native kernel ELF SHA-256 is
`9202c725fdb6294ad152ab309ac0758c564251a3548cb59b49385986e319cb89`.
The Linux EFI image SHA-256 is
`a5293cf31e544b7f476e04a3c7b35ebd46f212d7039f1ef8cfb91249296d1022`.

The requested milestone-2 core path now covers UEFI, disk-root Linux,
read/write persistence, Ring3 lifecycle and guest-to-host framebuffer output.
This closes the display gap recorded at the preceding checkpoint. General
multi-VM scheduling, arbitrary disk/image management and accelerated graphics
remain outside this fixed single-vCPU milestone profile.
