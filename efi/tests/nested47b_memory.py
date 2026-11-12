#!/usr/bin/env python3
"""Focused N47B actual memory fixture checks; no hardware or VM execution."""
import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--sanitizer', choices=['none', 'address,undefined', 'thread'], default='none')
p.add_argument('--cas-injection', action='store_true')
a = p.parse_args()
repo = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(tempfile.mkdtemp(prefix='syle-n47b-memory-'))
print('N47B_MEMORY_EVIDENCE', out, flush=True)
for name in ('krnl', 'include', 'efi/tests', 'efi/src'):
    shutil.copytree(repo / name, out / name)
inc = pathlib.Path(os.environ.get('EFI_PREFIX', '/usr')) / 'include/efi'
flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-pthread', '-fno-pie', '-no-pie',
         '-I' + str(inc), '-I' + str(inc / 'x86_64'), '-I' + str(out / 'krnl'),
         '-DVMX_NESTED_DOMAIN_TEST']
if a.sanitizer != 'none':
    flags += ['-fsanitize=' + a.sanitizer, '-fno-omit-frame-pointer']
if a.cas_injection:
    flags += ['-DNSRF_CAS_INJECT', '-fno-inline-atomics']
sources = ['nested_atomic_memory.c', 'nested_domain_memory.c', 'nested_root_catalog.c',
           'nested_ept.c', 'nested_source_replace_memory.c', 'nested_source_replace_fixture.c']
cmd = [os.environ.get('CC', 'cc'), *flags, str(out / 'efi/tests/nested47b_memory.c'),
       *[str(out / 'krnl' / s) for s in sources], '-o', str(out / 'test')]
if a.cas_injection:
    cmd += ['-Wl,--wrap=__atomic_compare_exchange_8', '-latomic']
(out / 'source-sha256.json').write_text(json.dumps({str(f.relative_to(out)): hashlib.sha256(f.read_bytes()).hexdigest()
    for name in ('krnl', 'efi/tests') for f in (out / name).glob('*') if f.is_file()}, sort_keys=True, indent=2) + '\n')
(out / 'command.json').write_text(json.dumps(cmd, indent=2) + '\n')
build = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(out / 'build.log').write_text(build.stdout)
if build.returncode:
    raise SystemExit(build.stdout)
run = subprocess.run([str(out / 'test')], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                     env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', TSAN_OPTIONS='halt_on_error=1'), timeout=120)
(out / 'run.log').write_text(run.stdout)
print(run.stdout, end='')
if run.returncode:
    raise SystemExit(run.returncode)
expected = 5 if a.cas_injection else 4
assert sum(line.startswith('PASS N47B') for line in run.stdout.splitlines()) == expected
print('PASS N47B focused actual memory fixture; sanitizer=' + a.sanitizer +
      '; CAS injection=' + str(a.cas_injection) + '; no hardware claim')
