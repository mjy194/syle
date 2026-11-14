#!/usr/bin/env python3
"""Prepare (and optionally run) an isolated, bounded direct-Linux test.

No downloads, host configuration changes, or implicit KVM fallback. A source
snapshot is rebuilt; runtime files use a new run directory. Firmware here boots
Syle, not its guest.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

FIXED = {
    "bzImage": "3d1c081c18e64ad97a44e2b038b8b3d0c733d38494b6293ba2402898c3a629e6",
    "initramfs.cpio": "7f71f66ed40ca0fa5e26e9dba8138672c148b0f720ed856a118b8955c36fa0a5",
}


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for data in iter(lambda: f.read(1024 * 1024), b""):
            h.update(data)
    return h.hexdigest()


def fixed_inputs(directory):
    result = {name: sha(directory / name) for name in FIXED}
    if result != FIXED:
        raise ValueError("fixed input hashes differ; do not silently replace the accepted baseline")
    return result


def command(run, code, qemu):
    return [qemu, "-machine", "q35,accel=kvm", "-cpu", "host", "-smp", "1", "-m", "512M",
            "-drive", f"if=pflash,format=raw,unit=0,readonly=on,file={code}",
            "-drive", f"if=pflash,format=raw,unit=1,file={run / 'vars.fd'}",
            "-drive", f"format=raw,readonly=on,file=fat:ro:{run / 'esp'}",
            "-net", "none", "-display", "none", "-monitor", "none", "-no-reboot",
            "-chardev", f"socket,id=terminal,path={run / 'console.sock'},server=on,wait=on,logfile={run / 'serial.log'},logappend=off",
            "-serial", "chardev:terminal"]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inputs", type=Path, required=True)
    p.add_argument("--run-dir", type=Path, required=True, help="must not already exist")
    p.add_argument("--ovmf-code", type=Path, required=True)
    p.add_argument("--ovmf-vars", type=Path, required=True)
    p.add_argument("--qemu", default="qemu-system-x86_64")
    p.add_argument("--seconds", type=int, default=180)
    p.add_argument("--run", action="store_true", help="actually start KVM after preparing")
    a = p.parse_args(argv)
    repo = Path(__file__).resolve().parents[1]
    run, inputs = a.run_dir.resolve(), a.inputs.resolve()
    code, variables = a.ovmf_code.resolve(), a.ovmf_vars.resolve()
    if not 1 <= a.seconds <= 3600:
        p.error("seconds must be 1..3600")
    if run.exists():
        p.error("run directory already exists; use a new directory")
    if run == repo or repo in run.parents:
        p.error("run directory must be outside the source checkout")
    # QEMU's comma-separated drive syntax cannot quote commas in filenames.
    if any("," in str(x) or "\n" in str(x) for x in (run, code, variables)):
        p.error("run and firmware paths cannot contain commas/newlines")
    if len(os.fsencode(run / "console.sock")) >= 104:
        p.error("run directory is too long for a Unix socket")
    hashes = fixed_inputs(inputs)
    for path in (code, variables):
        if not path.is_file():
            p.error(f"firmware not readable: {path}")
    qemu = shutil.which(a.qemu)
    if not qemu:
        p.error("QEMU executable not found")
    run.mkdir(parents=True, mode=0o700)
    git = lambda *args: subprocess.check_output(["git", "-C", str(repo), *args])
    source_commit = git("rev-parse", "HEAD").decode().strip()
    (run / "source.diff").write_bytes(git("diff", "HEAD", "--binary"))
    (run / "source.status").write_bytes(git("status", "--short"))
    files = git("ls-files", "-z", "--cached", "--others", "--exclude-standard").split(b"\0")
    snapshot = run / "source"
    snapshot.mkdir()
    source_hashes = {}
    for raw in files:
        if not raw:
            continue
        name = os.fsdecode(raw)
        source = repo / name
        if not source.exists() and not source.is_symlink():
            continue  # tracked deletion, also recorded in source.diff
        if source.is_symlink():
            raise ValueError(f"source symlink needs explicit review before frozen build: {name}")
        if not source.is_file():
            raise ValueError(f"source is not a regular file: {name}")
        target = snapshot / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        source_hashes[name] = sha(target)
    # Force a rebuild: prior opt-in flags must not leak through cached objects.
    build = ["make", "-B", "-C", str(snapshot / "efi"), "-f", "Makefile", "-f",
             "managed-terminal.mk", "all", "managed-terminal", f"MG_INPUTS={inputs}"]
    with open(run / "build.log", "wb") as log:
        subprocess.run(build, stdout=log, stderr=subprocess.STDOUT, check=True)
    esp = run / "esp"
    (esp / "EFI/BOOT").mkdir(parents=True)
    (esp / "syle").mkdir()
    copies = {
        "efi/build/esp/EFI/BOOT/BOOTX64.EFI": "EFI/BOOT/BOOTX64.EFI",
        "efi/build/krnl-managed-terminal.elf": "syle/krnl.elf",
        "efi/build/mgmt-guest-terminal.elf": "syle/mgmt.elf",
    }
    for src, dst in copies.items():
        shutil.copyfile(snapshot / src, esp / dst)
    (esp / "syle/boot.cfg").write_text("default=normal\ntimeout=0\ntitle=Syle managed baseline\n")
    shutil.copyfile(variables, run / "vars.fd")
    if any(sha(snapshot / name) != digest for name, digest in source_hashes.items()):
        raise RuntimeError("source snapshot changed during build; refusing runtime")
    cmd = command(run, code, qemu)
    manifest = {"schema": 1, "scope": "direct Linux / single-vCPU PIC04 terminal",
                "commit": source_commit, "source_sha256": source_hashes,
                "inputs_sha256": hashes, "firmware_sha256": {str(code): sha(code), str(variables): sha(variables)},
                "artifacts_sha256": {dst: sha(esp / dst) for dst in copies.values()},
                "build": build, "qemu": cmd, "timeout_seconds": a.seconds,
                "acceptance": "not run"}
    (run / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Prepared {run}; connect using tools/serial_terminal.py {run / 'console.sock'}", flush=True)
    if not a.run:
        return 0
    if not os.access("/dev/kvm", os.R_OK | os.W_OK):
        raise RuntimeError("KVM inaccessible in this execution context; no TCG fallback")
    manifest["acceptance"] = "not inferred: inspect captured serial and command evidence"
    with open(run / "qemu.log", "wb") as log:
        try:
            completed = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, timeout=a.seconds)
            manifest["qemu_returncode"] = completed.returncode
        except subprocess.TimeoutExpired:
            # subprocess.run kills and waits for the direct QEMU process.
            manifest["qemu_returncode"] = 124
    (run / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest["qemu_returncode"]


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"managed run failed: {exc}", file=sys.stderr)
        sys.exit(1)
