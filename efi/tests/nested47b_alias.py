#!/usr/bin/env python3
"""Focused WB-only full-CR3 audit and existing cache-page contracts, serial only."""
import argparse
import hashlib
import json
import os
import pathlib
import resource
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--sanitizer', choices=['none', 'address,undefined', 'thread'], default='none')
args = parser.parse_args()
repo = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(tempfile.mkdtemp(prefix='syle-n47b-alias-'))
print('N47B_ALIAS_EVIDENCE', out, flush=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
files = ['krnl/cache_page.c', 'krnl/cache_page.h', 'krnl/tests/cache_page.c',
         'efi/tests/nested47b_alias.c', 'efi/tests/nested47b_alias.py']
for name in files:
    target = out / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(repo / name, target)
(out / 'source-sha256.json').write_text(json.dumps({name: hashlib.sha256((out / name).read_bytes()).hexdigest()
                                                for name in files}, sort_keys=True, indent=2) + '\n')
flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-fno-pie', '-no-pie']
if args.sanitizer != 'none':
    flags += ['-fsanitize=' + args.sanitizer, '-fno-omit-frame-pointer']
results = {'sanitizer': args.sanitizer, 'serial_fixture': True, 'hardware_claim': False, 'tests': []}
for name, source in [('alias', 'efi/tests/nested47b_alias.c'), ('legacy', 'krnl/tests/cache_page.c')]:
    cmd = [os.environ.get('CC', 'cc'), *flags, str(out / source), str(out / 'krnl/cache_page.c'), '-o', str(out / name)]
    (out / (name + '-command.json')).write_text(json.dumps(cmd, indent=2) + '\n')
    build = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / (name + '-build.log')).write_text(build.stdout)
    if build.returncode:
        print(build.stdout, end='')
        raise SystemExit(build.returncode)
    run = subprocess.run([str(out / name)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', TSAN_OPTIONS='halt_on_error=1'), timeout=120)
    (out / (name + '-run.log')).write_text(run.stdout)
    results['tests'].append({'name': name, 'returncode': run.returncode})
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(run.stdout, end='')
    if run.returncode:
        raise SystemExit(run.returncode)
    if name == 'alias':
        assert sum(line.startswith('PASS N47B') for line in run.stdout.splitlines()) == 5
    else:
        assert 'cache_page audit and sequence: PASS' in run.stdout
print('PASS N47B WB-only full-CR3 audit and legacy contracts; sanitizer=' + args.sanitizer + '; serial fixture; no hardware claim')
