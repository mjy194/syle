# New, explicitly rebuildable Linux fixture (not the historical config)

The accepted old bzImage has no recovered .config/IKCONFIG. Its hash gate stays
unchanged. This directory defines a **new** Linux6.12 configuration and output
identity, not a recreation of that image or its unknown toolchain. Until the
real source resolves the fragment and a new image is built/booted, it is only a
build recipe with host gate tests. No new kernel output hash is invented.

Source: https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.tar.xz
SHA256: b1a2562be56e42afb3f8489d4c2a7ac472ac23098f1ef1c1e40da601f54625eb
The pin was checked against kernel.org's `sha256sums.asc`; that lookup is not
claimed as an independent GPG-signature verification. Acquire only from the
pinned official source and verify before extraction/executing its build rules.

`config.fragment` is an explicit allnoconfig request: single-vCPU x86-64,
relocatable kernel required by the existing loader, UART console, external
initramfs, ELF/scripts, proc/sys/devtmpfs, POSIX clocks and IKCONFIG. No network,
PCI or ACPI is requested. This deliberately does NOT support future virtio
boot/disks. Such support requires a separate named config and validation. Do
not add features silently to make an unrelated milestone pass.

## Build and inspect

Required executables include gcc/binutils, make, bc, bison, flex, perl and tar;
Linux may report further development-library dependencies once configured.
The script never installs packages, downloads sources or overwrites a build.
Archbook's existing tools were observed present; cloud lacks bc/bison/flex.
Run serially with other heavy work rather than burden the active VM test.

```
python3 guest/linux-rebuild/build.py --source-tar /verified/linux-6.12.tar.xz \
 --output /new/linux-build-1 --jobs 2
```

It verifies the source hash before extraction, records actual tools (including
GCC cc1), starts allnoconfig under a clean environment, checks every requested
setting against resolved.config, builds bzImage, rechecks config/tools and
records hashes plus provenance. Failed Kconfig requests are blockers to inspect,
not silently ignored. Missing dependencies or failure leave logs but no success
build manifest. Build time/user/host/version are fixed and IKCONFIG is embedded
so this new config remains recoverable. All paths are independent.

Repeat in a second fresh directory with the same source and exact toolchain,
then compare both resolved.config and bzImage hashes. Only matching repeated
artifacts justify a same-toolchain reproducibility claim. Recording tool
binaries/versions is not a hermetic container lock: shared libraries, compiler
packaging and dependency versions can still matter. Cross-toolchain identical
output is not promised. Preserve logs and resolved config even on mismatch.

## Separate input assembly and target

Use the existing deterministic BusyBox/probe initramfs builder, but keep its
archive/manifest explicit. Combine without modifying historical inputs:

```
python3 guest/linux-rebuild/assemble-inputs.py --kernel-build /new/linux-build-1 \
 --archive /verified/initramfs.cpio --archive-manifest /verified/manifest.json \
 --output /new/rebuild-inputs
make -C efi -f Makefile -f managed-rebuild.mk managed-rebuild \
 MB_INPUTS=/new/rebuild-inputs MB_MANIFEST=/new/rebuild-inputs/manifest.json
```

The new gate checks the official-source identity, selected fragment, complete
resolved config, kernel, initramfs and both provenance manifests. The manifest
is an explicit local build selection, not a cryptographic build attestation.
`rk20-tsc-*` objects and `krnl-managed-rebuild-20-tsc.elf` isolate this kernel from
all old `hc*` binaries. The terminal manager stays separate. Do not bypass the
old baseline gate or relabel old fixed hashes. Any native test must use fresh
ESP/VARS and record all new hashes, requiring Linux shell, independent program,
clock modes, cleanup and later STOP/START before extending previous acceptance.

Host tests: `PYTHONDONTWRITEBYTECODE=1 python3 guest/linux-rebuild/test.py`.
They use synthetic artifacts to test rejected tampering and unsupported recipe
state. They do not compile Linux or validate a guest boot.

## GNU C dialect compatibility, recipe v2

The first actual GCC 16.2.1 build resolved all requested Kconfig settings but
failed in compressed/string.o: GCC's C23 default treats false and bool as
keywords. Linux v6.12's top-level Makefile selects gnu11, while
arch/x86/boot/compressed/Makefile resets KBUILD_CFLAGS and loses that selection.
This recipe applies only the stored compressed-gnu11.patch, adding -std=gnu11
to that directory's C flags. CC/HOSTCC and tool probes remain unchanged; no
assembler flags or kernel configuration are added. The directory derives
KBUILD_AFLAGS from C flags already; -std=gnu11 does not change assembler syntax.
The exact original Makefile hash is required, and before/after/patch hashes
are saved with the patch in the new build and assembled inputs.

This is official Linux6.12 source plus a local compatibility patch, NOT pristine
source or an assertion that GCC16 is officially validated for this kernel.
No successful kernel or runtime claim follows from this fix alone. Preserve
the failed first build. Use a fresh build-2 directory, jobs2, then inspect logs
and manifest. Repeat in another new directory before claiming same-toolchain
reproducibility. Any further compiler failure must be inspected, not suppressed.

## First native-host build result and next gates

The v2 recipe completed one fresh jobs2 build using archbook's GCC16.2.1 and
Binutils2.47. Resolved config SHA256:
03b49a3ed3ce103d337df4fca29e6003ea15fc3c6245204e5eccce9340916a58
bzImage SHA256:
fc7e0e06e1b92768859b38fdf4b0e3bde80b9ec0511aaa40f2c952c3105952e1
Its status remains built-not-runtime-validated. This does not establish repeated
build identity or Linux guest boot. A second fresh build-3 is scheduled with the
same recipe/source/tools and jobs2, keeping objects separate. Compare actual
config/image/patch/tool hashes and all environment fields. Record the changed
absolute source/output paths rather than reusing a directory to hide them.
Preserve a mismatch for diagnosis; do not silently add prefix mapping or alter
the recipe and claim it was the same test.

After comparison, assemble new inputs using the explicit verified clock/pause
probe initramfs and build managed-rebuild. Pair only the resulting
krnl-managed-rebuild-20-tsc.elf with mgmt-guest-terminal.elf. Schedule an isolated
fresh ESP/VARS KVM run with the established single-vCPU hv-time CPU contract.
Require Linux shell, actual clocksource selection, all five clock modes and
checked cleanup. Guest program exit-status and same-process STOP/START are
additional new-kernel tests, not inherited from the previous bzImage results.

The second fresh jobs2 build has now completed. Both builds' bzImage,
resolved.config, config.fragment, compatibility patch and full build.json are
byte-identical. The source and object directories were separately build-2 and
build-3; no object reuse. Both actual output directories passed load_build in
an independent inspection of the archived artifacts. This establishes the
observed two-build identity on this exact toolchain and recipe, not hermetic or
cross-machine reproducibility. New-kernel runtime acceptance remains a separate
gate; the independent managed-rebuild five-mode boot test is next.
