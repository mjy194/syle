#!/usr/bin/env python3
"""Execute the complete selected production checkpoint with controlled peers/VMX.
The checkpoint predicate is extracted, never duplicated in the test fixture.
This serial host test is not a VMX, memory-walk, or concurrent-round proof.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import resource
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--sanitizer', choices=['none', 'address,undefined', 'thread'], default='none')
p.add_argument('--source-root', type=Path)
p.add_argument('--expect-legacy-count', action='store_true')
a = p.parse_args()
root = (a.source_root or Path(__file__).resolve().parents[2]).resolve()
out = Path(tempfile.mkdtemp(prefix='syle-n47b-checkpoint-'))
print('N47B_CHECKPOINT_EVIDENCE', out, flush=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
r = subprocess.run(['make', '-s', '--eval=print-n47b-flags:;@echo $(N47BFLAGS)',
                    'print-n47b-flags'], cwd=root/'efi', capture_output=True, text=True)
assert r.returncode == 0, r.stderr
profile = r.stdout.split()
assert '-DVMX_NESTED_SOURCE_REPLACE_TEST' in profile
runtime = root/'krnl/nested_source_replace_runtime.inc'
source = runtime.read_text()
begin = source.index('static bool atomic_checkpoint(')
end = source.index('static bool atomic_final(', begin)
checkpoint = source[begin:end]
assert checkpoint.rstrip().endswith('}')
(out/'selected-checkpoint.inc').write_text(checkpoint)
fixture = Path(__file__).with_name('nested47b_checkpoint_test.c').read_text()
(out/'test.c').write_text(fixture)
flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-fno-pie', '-no-pie',
         *profile, '-I'+str(root/'krnl')]
if a.sanitizer != 'none':
    flags += ['-fsanitize='+a.sanitizer]
if a.expect_legacy_count:
    flags += ['-DEXPECT_LEGACY_COUNT']
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', TSAN_OPTIONS='halt_on_error=1')

def run(name, extra=()):
    cmd = [os.environ.get('CC', 'cc'), *flags, *extra, str(out/'test.c'), '-o', str(out/name)]
    (out/(name+'-command.json')).write_text(json.dumps(cmd, indent=2)+'\n')
    r = subprocess.run(cmd, capture_output=True, text=True)
    (out/(name+'-build.log')).write_text(r.stdout+r.stderr)
    assert r.returncode == 0, r.stderr
    r = subprocess.run([str(out/name)], capture_output=True, text=True, env=env, timeout=20)
    (out/(name+'-run.log')).write_text(r.stdout+r.stderr)
    return r

r = run('test')
assert r.returncode == 0, (r.returncode, r.stdout, r.stderr)
print(r.stdout, end='')
if not a.expect_legacy_count:
    # Restore the exact v3 production count clause, leaving the fixture intact.
    mutant, n = re.subn(r's->contexts\.count!=\([^;]+?\)\)goto fail;',
                        's->contexts.count!=(phase?2u:1u))goto fail;', checkpoint)
    assert n == 1 and mutant != checkpoint
    (out/'mutant-checkpoint.inc').write_text(mutant)
    r = run('legacy-mutant', ['-DCHECKPOINT_MUTANT'])
    assert r.returncode != 0 and 'Assertion' in r.stderr and 'atomic_checkpoint(&run,3)' in r.stderr, (r.returncode, r.stderr)
    print('KILLED exact v3 count predicate with legal phase3 count1/created3 positive fixture')
(out/'results.json').write_text(json.dumps({
    'status': 'PASS', 'sanitizer': a.sanitizer, 'legacy_reproducer': a.expect_legacy_count,
    'runtime_sha256': hashlib.sha256(runtime.read_bytes()).hexdigest(),
    'selected_checkpoint_sha256': hashlib.sha256(checkpoint.encode()).hexdigest(),
    'fixture_sha256': hashlib.sha256(fixture.encode()).hexdigest(),
    'production_types': True, 'hardware_and_peers_mocked': True, 'concurrency_claim': False,
    'hardware_claim': False,
    'checks': ['both owners all 8 phases', 'resident counts 0..3', 'created +/-1 and UINT64_MAX',
               'phase 8 and UINT_MAX', 'null runtime', 'phase3 prior guard negatives',
               'legacy production count predicate killed'] if not a.expect_legacy_count else ['exact v3 legal phase3 rejected'],
}, indent=2)+'\n')
