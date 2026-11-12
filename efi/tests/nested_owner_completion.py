#!/usr/bin/env python3
"""Compile and execute the actual completion slice under real Makefile profiles.

This intentionally tests both counter checks in sequence, so reintroducing the
N34C2 guard into N38C-B fails even when all isolated policy tests pass. No VMX or
PMM operation runs. Source boundaries must be unique; any drift fails closed.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

EFI = Path(__file__).resolve().parents[1]
ROOT = EFI.parent
source = (ROOT / "krnl/smp_nested.inc").read_text()
start = "    uint64_t *shared=paging_phys_to_virt(nested_shared);\n"
end = "#ifdef VMX_NESTED_LOCAL_INVEPT_TEST\n    for(unsigned i=0;i<2;i++){"
assert source.count(start) == source.count(end) == 1, "completion slice moved; update boundaries"
completion = source.split(start, 1)[1].split(end, 1)[0]
profiles = ("N40BFLAGS", "N39B2FLAGS", "N39B1FLAGS", "N38CBFLAGS", "N34C2FLAGS")
recipe = ".PHONY: completion-profile-flags\ncompletion-profile-flags:\n"
recipe += "\t@printf '%s\\n' " + " ".join("'$(" + p + ")'" for p in profiles) + "\n"
flags = subprocess.check_output(
    ["make", "--no-print-directory", "-s", "-f", "Makefile", "-f", "-", "completion-profile-flags"],
    input=recipe, text=True, cwd=EFI,
).splitlines()
assert len(flags) == len(profiles)
with tempfile.TemporaryDirectory(prefix="syle-owner-completion-") as directory:
    out = Path(directory)
    (out / "nested-owner-completion.inc").write_text(start + completion)
    for profile, defines in zip(profiles, flags):
        binary = out / profile
        subprocess.run([
            *shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-O2", "-g",
            "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
            *shlex.split(defines), "-I", str(out),
            str(ROOT / "krnl/tests/nested_owner_completion.c"),
            str(ROOT / "krnl/nested_owner_pool.c"), str(ROOT / "krnl/typed_root_overlap.c"), "-o", str(binary),
        ], check=True, cwd=EFI)
        subprocess.run([str(binary)], check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
