#!/usr/bin/env python3
"""Instrument the four boot host suites, without booting an image.

Use a new output directory per run so baseline evidence is never overwritten.
Coverage describes host-executed production C, not EFI/guest integration.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
SUITES = {
    "policy": ["boot/boot.c", "boot/config.c", "boot/tests/policy.c"],
    "exit_services": ["boot/tests/exit_services.c"],
    "read_bounded": ["boot/tests/read_bounded.c"],
    "kernel_elf": ["boot/kernel_elf.c", "boot/tests/kernel_elf.c"],
}
PRODUCTION = {
    "boot/boot.c", "boot/config.c", "boot/exit_services.h",
    "boot/read_bounded.h", "boot/kernel_elf.c", "boot/kernel_elf.h",
    "boot/kernel_prepare.h", "include/syle/boot_platform.h",
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, cwd, log, env=None):
    result = subprocess.run(command, cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    with log.open("a") as stream:
        stream.write("$ " + shlex.join(map(str, command)) + "\n")
        stream.write(result.stdout)
        stream.write(f"exit_status={result.returncode}\n")
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}); see {log}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--kernel-elf", type=Path)
    args = parser.parse_args()
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=False)
    artifact = args.kernel_elf.resolve() if args.kernel_elf else None
    report = {
        "scope": "host production C only; not EFI, QEMU, hardware, or guest coverage",
        "compiler": subprocess.check_output(["gcc", "--version"], text=True).splitlines()[0],
        "gcov": subprocess.check_output(["gcov", "--version"], text=True).splitlines()[0],
        "flags": ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O0", "-g",
                  "--coverage", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                  "-fno-pie", "-no-pie"],
        "notes": ["No production source instrumentation edits.",
                  "ASan/UBSan enabled; leak detection disabled as in the existing host scripts.",
                  "Non-PIE instrumented binaries avoid address-space layout collisions with ASan.",
                  "Taken branch edges are gcov arcs with count > 0, not MC/DC or requirements coverage.",
                  "Raw gcov text and JSON retain per-line/per-edge results."],
        "source_sha256": {name: digest(ROOT / name) for name in sorted(
            PRODUCTION | {p for suite in SUITES.values() for p in suite} | {"boot/tests/host_coverage.py"})},
        "kernel_elf": {"path": str(artifact), "sha256": digest(artifact),
                       "bytes": artifact.stat().st_size} if artifact else None,
        "suites": {},
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
    for name, sources in SUITES.items():
        work = output / name
        work.mkdir()
        log = work / "commands.log"
        objects = []
        for index, source in enumerate(sources):
            obj = work / f"{index}-{Path(source).stem}.o"
            run(["gcc", *report["flags"], "-c", str(ROOT / source), "-o", str(obj)], work, log)
            objects.append(obj)
        executable = work / "test"
        run(["gcc", *report["flags"], *map(str, objects), "-o", str(executable)], work, log)
        command = [str(executable)]
        if name == "kernel_elf" and artifact:
            command.append(str(artifact))
        run(command, work, log, environment)
        for obj in objects:
            run(["gcov", "-b", "-c", "-p", str(obj)], work, log)
            run(["gcov", "-b", "-c", "--json-format", str(obj)], work, log)
        files = {}
        for path in sorted(work.glob("*.gcov.json.gz")):
            with gzip.open(path, "rt") as stream:
                data = json.load(stream)
            for source in data["files"]:
                source_path = Path(source["file"])
                if not source_path.is_absolute():
                    source_path = Path(data["current_working_directory"]) / source_path
                relative = source_path.resolve().relative_to(ROOT).as_posix()
                if relative not in PRODUCTION:
                    continue
                lines = source["lines"]
                branches = [b for line in lines for b in line.get("branches", [])]
                files[relative] = {
                    "lines_total": len(lines),
                    "lines_hit": sum(line["count"] > 0 for line in lines),
                    "branch_edges_total": len(branches),
                    "branch_edges_taken": sum(branch["count"] > 0 for branch in branches),
                    "unhit_lines": [line["line_number"] for line in lines if not line["count"]],
                    "lines_with_untaken_edges": [line["line_number"] for line in lines
                        if any(not branch["count"] for branch in line.get("branches", []))],
                }
        report["suites"][name] = {"status": "PASS", "production_files": files}
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(name, json.dumps(files, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
