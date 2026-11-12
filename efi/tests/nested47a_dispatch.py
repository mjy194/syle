#!/usr/bin/env python3
"""Frozen-copy production SOURCE_REPLACE dispatch tests; no hardware claim."""
import argparse, hashlib, json, os, pathlib, resource, shutil, subprocess, tempfile
p = argparse.ArgumentParser()
p.add_argument('--sanitizer', choices=['none', 'address,undefined', 'thread'], default='none')
a = p.parse_args()
repo = pathlib.Path(__file__).resolve().parents[2]
out = pathlib.Path(tempfile.mkdtemp(prefix='syle-n47a-dispatch-'))
print('N47A_DISPATCH_EVIDENCE', out, flush=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
files = ['krnl/nested_domain.c', 'krnl/nested_domain.h', 'krnl/nested_domain_dispatch.c',
         'krnl/nested_domain_dispatch.h', 'krnl/nested_vmx.c', 'krnl/nested_vmx.h',
         'krnl/tests/nested_domain_dispatch.c', 'krnl/tests/nested_domain_maintenance.c',
         'efi/tests/nested47a_dispatch.c', 'efi/tests/nested47a_dispatch.py']
for name in files:
    target = out / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(repo / name, target)
base = (out/'krnl/tests/nested_domain_dispatch.c').read_text()
assert base.count('int main(void)') == 1
name = 'krnl/tests/nested47a_dispatch_base.inc'
(out/name).write_text(base.replace('int main(void)', 'static void original_dispatch_main(void)'))
files.append(name)
manifest = {name: hashlib.sha256((out/name).read_bytes()).hexdigest() for name in files}
(out/'source-sha256.json').write_text(json.dumps(manifest, indent=2)+'\n')
flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-fno-pie', '-no-pie']
if a.sanitizer != 'none': flags += ['-fsanitize='+a.sanitizer]
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', TSAN_OPTIONS='halt_on_error=1')
results = []
for label, fixture in [('source', 'efi/tests/nested47a_dispatch.c'), ('maintenance', 'krnl/tests/nested_domain_maintenance.c')]:
    binary = out/label
    cmd = [os.environ.get('CC', 'cc'), *flags, str(out/fixture), *[str(out/'krnl'/n) for n in ['nested_domain_dispatch.c', 'nested_domain.c', 'nested_vmx.c']], '-o', str(binary)]
    r = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out/(label+'-build.log')).write_text(r.stdout)
    results.append({'test':label, 'build_returncode':r.returncode})
    if r.returncode == 0:
        r = subprocess.run([str(binary)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, timeout=90)
        (out/(label+'-run.log')).write_text(r.stdout)
        results[-1]['run_returncode'] = r.returncode
        print(r.stdout, end='')
(out/'results.json').write_text(json.dumps({'sanitizer':a.sanitizer, 'hardware_claim':False, 'results':results}, indent=2)+'\n')
assert all(hashlib.sha256((out/name).read_bytes()).hexdigest()==h for name,h in manifest.items())
assert all(r.get('run_returncode')==0 for r in results), results
print('PASS N47A dispatch and existing maintenance; sanitizer='+a.sanitizer+'; no hardware claim')
