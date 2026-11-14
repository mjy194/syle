# Independent Linux baseline fixture

The program is a separate static x86-64 ELF, not a shell builtin. It uses Linux
syscalls directly and tests argument delivery, exclusive file creation, write,
close, reopen, read, unlink and a nonzero exit code. It uses guest `/tmp` only.
A previous `/tmp/syle-program-test` causes explicit failure instead of overwrite.

```
guest/linux-baseline/build.sh /tmp/baseline-probe
/bin/baseline-probe check
# Expect SYLE_PROGRAM_OK and exit status 0.
/bin/baseline-probe exit37
printf 'EXIT=%s\n' "$?"
# Expect SYLE_PROGRAM_EXIT37 and EXIT=37.
/bin/sleep 1
printf 'SYLE_WAKE_OK\n'
```

`make-initramfs.py` builds a deterministic uncompressed newc archive from an
explicit static x86-64 BusyBox and the probe. No root, mknod or cpio is needed.
BusyBox must include sh, mount, mkdir, setsid, cttyhack, sleep, cat and uname.
The caller must preserve its version/config/license and verify these applets.
The ELF check excludes PT_INTERP, but does not prove all applets are enabled.

```
guest/linux-baseline/make-initramfs.py --busybox /path/to/static/busybox \
 --probe /tmp/baseline-probe --output /tmp/baseline-initramfs.cpio \
 > /tmp/baseline-initramfs.manifest.json
```

The output uses sorted names, fixed permissions, zero uid/gid/mtime and fixed
inode numbers. Identical input binary bytes produce identical archive bytes.
The probe compiler itself must be pinned for reproducible probe binaries.

**This is a new fixture**, not a recreation of the historical accepted archive.
The existing managed guest build pins its original input hashes; it will rightly
reject this archive. Do not replace the historical fixture or alter its expected
hash silently. Wire any new test input through a separately named reviewed target
and record the new hashes. A fixed Linux kernel/config must also be recorded;
this directory does not claim to reconstruct the historical kernel.

Host tests only demonstrate the ELF/syscall program and archive encoding. Guest
acceptance requires independent execution through Syle, correct markers and exit
codes, and captured logs. `sleep`/uptime timing must be checked on the actual guest,
not inferred from the existence of the BusyBox applet.

## Explicit input gate and guest target

Keep the original `bzImage` and new archive in a separate directory, named
`bzImage` and `initramfs.cpio`. Pass the generated manifest explicitly:

```
make -C efi -f Makefile -f managed-baseline.mk managed-baseline-probe \
 MB_INPUTS=/absolute/baseline-inputs MB_MANIFEST=/absolute/manifest.json
```

Pair `krnl-managed-baseline.elf` with `mgmt-guest-baseline-probe.elf` in an
isolated ESP. The command runs the independent ELF, validates its intentional
exit status 37, waits with `/bin/sleep 1`, and requires the independent line
`SYLE_BASELINE_OK`, then 256 further successful slices and checked cleanup.
The manifest is an explicit input selection, not a signature or trust claim.
The gate always checks the frozen original kernel SHA and the exact new archive
SHA from the manifest. Startup and embedding depend on that gate, and all
new-fixture kernel objects use the `mb-` prefix. No historical hashes are changed.

`managed-baseline-terminal` pairs `krnl-managed-baseline-terminal.elf` with the
existing `mgmt-guest-terminal.elf` for externally supplied commands. Input stays
opt-in in that kernel. Do not pair the baseline manager with the old initramfs,
which lacks `/bin/baseline-probe`.
