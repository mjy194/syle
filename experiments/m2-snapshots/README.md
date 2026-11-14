# M2 experimental source snapshots

These are archival source-only snapshots, not integrated production features. They preserve work outside the main checkout without overwriting it. No guest execution, new build, or validation is performed by this archival step.

- `run32-source-delta`: source differences from the cloud BootMenu candidate tree relative to this checkout at archival time; not a claim of byte-identical complete archbook run32 source. Verify actual archived run32 source and baseline manifests before applying subsequent overlays.
- `firmware-bootmenu`: BootMenu scalar overlay on its recorded actual-run31 base.
- `pm1-control`: opt-in PM1 control overlay on actual run32. Its host tests/static review and later observed read/write are recorded in the conversation; do not infer all ACPI semantics.
- `code-rom-diagnostic`: stop-only CODE diagnostic on the actual PM1 source. Built and host-tested, not guest-tested; does not grant CODE write permission.
- `nodma-board6144`: independent noDMA diagnostic profile. Its native callbacks refuse DMA. The board-limit change was host-tested; its subsequent guest run was blocked, not completed. It is not a functional block device or boot proof.
- `ramfb-host-contract`: numeric parser and host tests only, no guest ownership/display integration.
- `graphics-config-drafts`: Kconfig-resolved drafts only, no new kernel build or display acceptance.

Original relative source layout is retained. Generated binaries, ELF files, full source archives, logs and large raw evidence are intentionally omitted. Original package CONTROL/SHA256SUMS lists were omitted where they referred to excluded artifacts. `BASE-SHA256SUMS` and source-only frozen manifests remain historical prerequisites, not evidence that this checkout is that base. Some retained runbooks mention external artifacts which are not included. Execution restrictions and limitations in those runbooks still apply; this archive does not authorize retrying a blocked operation.

`SOURCE-SHA256SUMS` checks the saved files only.

- `diskroot`: Linux 6.12 virtio PCI legacy/ext4 built-in configuration draft; Kconfig resolved only, not compiled or boot-tested.
