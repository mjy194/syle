#!/usr/bin/env python3
"""Real Makefile/kernel link acceptance; no VMX or runtime-maintenance claim."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TARGET = "build/krnl-smp-nested-atomic-catalog.elf"
OBJECT = "build/k-typed-root-atomic.o"
NEW = """ /* This large object must not lower to a libc memset in the freestanding
  * kernel. Match nested_parallel_init's explicit volatile byte stores. */
 struct tam_hooks hooks=*h;uint64_t type_epoch=a->bridge->lease->type_epoch;
 volatile unsigned char *bytes=(void *)m;
 for(size_t n=0;n<sizeof *m;n++)bytes[n]=0;
 m->owners[0]=a;m->owners[1]=b;m->hooks=hooks;m->context=context;
 m->state=TAM_OPEN;m->type_epoch=type_epoch;"""
OLD = " *m=(struct tr_atomic_maintenance){.owners={a,b},.hooks=*h,.context=context,.state=TAM_OPEN,.type_epoch=a->bridge->lease->type_epoch};"


def run(work, *args):
    result = subprocess.run(args, cwd=work / "efi", text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=180)
    print("COMMAND", " ".join(args), "EXIT", result.returncode, flush=True)
    print(result.stdout, end="", flush=True)
    return result


def symbols(work, path):
    result = run(work, "nm", "-u", path)
    assert result.returncode == 0
    return {line.split()[-1] for line in result.stdout.splitlines() if line.strip()}


with tempfile.TemporaryDirectory(prefix="syle-n42a-freestanding-") as tmp:
    work = Path(tmp)
    # No historic executable/object/cache can satisfy a fresh build dependency.
    for directory, names, files in os.walk(ROOT):
        names[:] = sorted(n for n in names if n not in {".git", "build", "__pycache__"})
        for name in sorted(files):
            source = Path(directory) / name
            assert not source.is_symlink()
            target = work / source.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    assert not (work / "efi/build").exists()
    result = run(work, "make", TARGET)
    assert result.returncode == 0, "fresh actual freestanding kernel link failed"
    commands = [line for line in result.stdout.splitlines()
                if " -c ../krnl/typed_root_atomic.c " in line]
    assert len(commands) == 1
    for flag in ("-O1", "-ffreestanding", "-mgeneral-regs-only", "-DSYLE_KERNEL"):
        assert flag in commands[0].split(), (flag, commands[0])
    assert "-nostdlib -static -T ../krnl/link.ld" in result.stdout
    assert not symbols(work, OBJECT) & {"memset", "memcpy", "memmove", "bzero"}
    assert not symbols(work, TARGET), "freestanding kernel has unresolved symbols"
    for path in (OBJECT, TARGET):
        print("FREESTANDING_SHA256", hashlib.sha256((work / "efi" / path).read_bytes()).hexdigest(), path)

    # Observed negative witness: only restore the exact rejected aggregate init.
    source = work / "krnl/typed_root_atomic_maintenance.inc"
    text = source.read_text()
    assert text.count(NEW) == 1
    source.write_text(text.replace(NEW, OLD))
    (work / "efi" / OBJECT).unlink()
    (work / "efi" / TARGET).unlink()
    result = run(work, "make", OBJECT)
    assert result.returncode == 0
    assert "memset" in symbols(work, OBJECT), "old aggregate no longer witnesses defect"
    result = run(work, "make", TARGET)
    assert result.returncode != 0, "old aggregate unexpectedly linked without libc"
    assert "undefined reference to `memset'" in result.stdout
    assert "typed_root_atomic_maintenance.inc:" in result.stdout
    assert not (work / "efi" / TARGET).exists()
    print("PASS N42A actual freestanding O1 compile/nm/nostdlib link; old aggregate rejected by undefined memset")
