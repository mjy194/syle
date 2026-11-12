#!/usr/bin/env python3
"""Host-only coverage and assertion checks for console, UART C and boot ABI.

No guest execution, privileged instructions, hardware ports, or production edits.
Coverage and sanitizer builds are separate. Mutations live only in evidence copies.
"""
from __future__ import annotations
import argparse
import datetime
import difflib
import gzip
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import shutil
import signal
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PRODUCTION = ["efi/src/console.c", "efi/src/console.h", "efi/src/exceptions.h",
              "krnl/runtime.c", "include/syle/boot.h", "include/syle/boot_validate.h"]
CASES = {
    "console": ("efi/tests/console_memory.c", ["-fshort-wchar"], "efi/src/console.c"),
    "uart": ("efi/tests/ms01_uart.c", ["-DSYLE_KERNEL"], "krnl/runtime.c"),
    "boot-info": ("krnl/tests/boot_info.c", [], "include/syle/boot_validate.h"),
}
# Hand-picked behavioral faults, not a random or exhaustive mutation score.
MUTATIONS = [
    ("console-default-color", "console", "efi/src/console.c", "color(225, 240, 250)", "color(224, 240, 250)"),
    ("console-scroll-source", "console", "efi/src/console.c", "(y + CELL_H) * screen.stride", "(y + CELL_H - 1) * screen.stride"),
    ("console-stale-snapshot", "console", "efi/src/console.c", "boot_fb.valid = 0;", "boot_fb.valid = boot_fb.valid;"),
    ("console-tab-three-spaces", "console", "efi/src/console.c", "i < 4; ++i", "i < 3; ++i"),
    ("console-gop-reversed-format", "console", "efi/src/console.c", "PixelBlueGreenRedReserved8BitPerColor ? 1 : 0", "PixelBlueGreenRedReserved8BitPerColor ? 0 : 1"),
    ("uart-corrupted-byte", "uart", "krnl/runtime.c", "out8(0x3f8, (uint8_t)c)", "out8(0x3f8, (uint8_t)(c ^ 1))"),
    ("uart-wrong-newline", "uart", "krnl/runtime.c", "serial_putc('\\r');", "serial_putc('\\n');"),
    ("uart-wrong-probe-divisor", "uart", "krnl/runtime.c", "out8(0x3f8, 1);", "out8(0x3f8, 2);"),
    ("uart-wrong-hex-digits", "uart", "krnl/runtime.c", '"0123456789abcdef"', '"1023456789abcdef"'),
    ("abi-reject-minimum-stride", "boot-info", "include/syle/boot_validate.h", "b->descriptor_size < 40", "b->descriptor_size <= 40"),
    ("abi-reject-exact-containment", "boot-info", "include/syle/boot_validate.h", "inner.base - outer.base <= outer.size - inner.size", "inner.base - outer.base < outer.size - inner.size"),
    ("abi-accept-exited-two", "boot-info", "include/syle/boot_validate.h", "b->boot_services_exited != 1", "b->boot_services_exited == 0"),
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True, help="Immutable recovered R3 source")
    parser.add_argument("--output", type=Path, default=ROOT / "ms01/evidence/r4-console-abi-host")
    parser.add_argument("--efi-prefix", type=Path, default=Path(os.environ.get("EFI_PREFIX", "/usr")))
    args = parser.parse_args()
    baseline, out = args.baseline.resolve(), args.output.resolve()
    # Output must not touch the baseline and must be an evidence directory.
    if not out.is_relative_to(ROOT / "ms01/evidence") or out.is_relative_to(baseline):
        raise SystemExit("Output must be under this checkout's ms01/evidence, outside the baseline")
    out.mkdir(parents=True, exist_ok=True)
    cc, gcov = os.environ.get("CC", "gcc"), os.environ.get("GCOV", "gcov")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    commands = []
    def run(argv, cwd, log, expected=None, env=None):
        argv = [str(x) for x in argv]
        cp = subprocess.run(argv, cwd=cwd, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30, env=env)
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(cp.stdout)
        commands.append({"argv": argv, "shell_display": shlex.join(argv), "cwd": str(cwd),
                         "returncode": cp.returncode, "log": str(log.relative_to(out)),
                         "test_environment": {key: env[key] for key in
                             ("ASAN_OPTIONS", "UBSAN_OPTIONS", "EFI_PREFIX")
                             if env is not None and key in env}})
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if expected is None and cp.returncode != 0:
            raise RuntimeError(f"Command failed ({cp.returncode}): {shlex.join(argv)}; see {log}")
        if expected is not None and cp.returncode not in expected:
            raise RuntimeError(f"Unexpected return code {cp.returncode}: see {log}")
        return cp

    source_paths = PRODUCTION + [c[0] for c in CASES.values()] + [
        "efi/tests/console_memory.sh", "efi/tests/ms01_uart.sh", "krnl/tests/boot_info.sh"]
    hashes = {label: {p: digest(root / p) for p in source_paths}
              for label, root in [("baseline", baseline), ("final", ROOT)]}
    for p in PRODUCTION:
        if hashes["baseline"][p] != hashes["final"][p]:
            raise SystemExit(f"Production source differs from recovered baseline: {p}")
    hashes["runner"] = {str(Path(__file__).relative_to(ROOT)): digest(Path(__file__))}
    (out / "source-hashes.json").write_text(json.dumps(hashes, indent=2) + "\n")
    (out / "test-changes.patch").write_text("".join(
        "".join(difflib.unified_diff((baseline/p).read_text().splitlines(True),
            (ROOT/p).read_text().splitlines(True), fromfile="baseline/"+p, tofile="final/"+p))
        for p in source_paths if hashes["baseline"][p] != hashes["final"][p]))
    compiler = run([cc, "--version"], ROOT, out/"compiler-version.txt").stdout.splitlines()[0]
    gcov_version = run([gcov, "--version"], ROOT, out/"gcov-version.txt").stdout.splitlines()[0]
    common = [cc, f"-I{args.efi_prefix}/include/efi", f"-I{args.efi_prefix}/include/efi/x86_64",
              "-std=c11", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
              "-Wall", "-Wextra", "-Werror", "-g"]
    coverage = {}
    for label, root in [("baseline", baseline), ("final", ROOT)]:
        coverage[label] = {}
        for case, (source, flags, target) in CASES.items():
            folder = out / label / case
            folder.mkdir(parents=True, exist_ok=True)
            # Remove only generated counters before a new measurement.
            for generated in folder.glob("*.gcda"):
                generated.unlink()
            run(common + flags + ["-O0", "--coverage", root/source, "-o", folder/"test"],
                ROOT, folder/"compile.log")
            run([folder/"test"], ROOT, folder/"test.log")
            gcno = sorted(folder.glob("*.gcno"))
            run([gcov, "-b", "-c", "-j"] + gcno, folder, folder/"gcov-json.log")
            run([gcov, "-b", "-c"] + gcno, folder, folder/"gcov-summary.txt")
            matching = []
            for archive in folder.glob("*.gcov.json.gz"):
                data = json.load(gzip.open(archive))
                matching += [f for f in data["files"] if Path(f["file"]) == root/target]
            assert len(matching) == 1
            data = matching[0]
            branches = [b for line in data["lines"] for b in line["branches"]]
            coverage[label][target] = {
                "lines_hit": sum(line["count"] > 0 for line in data["lines"]),
                "lines_total": len(data["lines"]),
                "branch_outcomes_hit": sum(b["count"] > 0 for b in branches),
                "branch_outcomes_total": len(branches),
                "unhit_lines": [line["line_number"] for line in data["lines"] if not line["count"]],
                "functions": [{k:f[k] for k in ("name", "execution_count", "blocks", "blocks_executed")}
                              for f in data["functions"]],
            }
            sanitize = ["-O1" if case == "boot-info" else "-O0",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            run(common + flags + sanitize + [root/source, "-o", folder/"sanitized"],
                ROOT, folder/"sanitizer-compile.log")
            env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0",
                       UBSAN_OPTIONS="halt_on_error=1")
            run([folder/"sanitized"], ROOT, folder/"sanitizer-run.log", env=env)

    mutations = []
    for name, case, target, old, new in MUTATIONS:
        row = {"name": name, "case": case, "target": target, "old": old, "new": new}
        source, flags, _ = CASES[case]
        for label, root in [("baseline", baseline), ("final", ROOT)]:
            folder = out/"mutations"/name/label
            copy_root = folder/"source"
            for p in PRODUCTION + [source]:
                dest = copy_root/p
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(root/p, dest)
            content = (copy_root/target).read_text()
            assert content.count(old) == 1, (name, old)
            (copy_root/target).write_text(content.replace(old, new))
            row[f"{label}_mutated_sha256"] = digest(copy_root/target)
            run(common + flags + ["-O0", copy_root/source, "-o", folder/"test"],
                ROOT, folder/"compile.log")
            result = run([folder/"test"], ROOT, folder/"test.log", expected={0, -signal.SIGABRT})
            # Only an assertion diagnostic counts as a killed mutation.
            killed = result.returncode == -signal.SIGABRT and "Assertion" in result.stdout
            if result.returncode != 0 and not killed:
                raise RuntimeError(f"Non-assertion failure does not count as a kill: {name}/{label}")
            row[label] = "killed_by_assertion" if killed else "survived"
        mutations.append(row)
    (out / "mutations.json").write_text(json.dumps(mutations, indent=2) + "\n")
    if any(row["final"] != "killed_by_assertion" for row in mutations):
        raise RuntimeError("A behavioral mutation survived the expanded assertions")
    # Existing public script entry points are independently rerun unchanged.
    env = dict(os.environ, EFI_PREFIX=str(args.efi_prefix),
               UBSAN_OPTIONS="halt_on_error=1")
    for script in ["efi/tests/console_memory.sh", "efi/tests/ms01_uart.sh", "krnl/tests/boot_info.sh"]:
        run(["bash", ROOT/script], ROOT, out/"existing-scripts"/(Path(script).stem+".log"), env=env)
    summary = {"generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
               "compiler": compiler, "gcov": gcov_version, "baseline_root": str(baseline),
               "source_root": str(ROOT), "coverage_optimization": "-O0", "coverage": coverage,
               "mutations": {"count": len(mutations),
                             "baseline_survived": sum(m["baseline"]=="survived" for m in mutations),
                             "final_killed": sum(m["final"]=="killed_by_assertion" for m in mutations)},
               "production_unchanged": True,
               "sanitizer_variants_passed": 6, "existing_scripts_passed": 3,
               "sanitizer_environment": {"ASAN_OPTIONS": "detect_leaks=0",
                                         "UBSAN_OPTIONS": "halt_on_error=1"},
               "existing_script_environment": {"UBSAN_OPTIONS": "halt_on_error=1"},
               "limits": ["Host coverage is compiler-specific branch execution, not exhaustive inputs or MC/DC.",
                          "GNU-EFI callback and UART registers are mocked; no firmware or hardware behavior is proved.",
                          "kernel_runtime is unexecuted, including boot validation and framebuffer normalization integration.",
                          "ABI tests validate readable record contents and integer ranges, not pointer accessibility.",
                          "Mutation cases are hand-picked behavioral faults, not a general mutation score."]}
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({"coverage": coverage, "mutations": summary["mutations"],
                      "evidence": str(out)}, indent=2))

if __name__ == "__main__":
    main()
