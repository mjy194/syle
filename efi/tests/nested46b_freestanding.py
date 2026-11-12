#!/usr/bin/env python3
"""Fresh N46B UC-profile Makefile compile/link. Does not execute any guest."""
from pathlib import Path
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
TARGET = 'smp-nested-atomic-uc'
PROFILE_SOURCES = {
    'vcpu': 'nested_vcpu_policy.c',
    'runtime': 'nested_parallel.c',
    'guest': 'vmx_guest.c',
    'adapter': 'nested_guest.c',
    'ept': 'ept.c',
    'owner': 'nested_owner_atomic.c',
    'sparse': 'nested_owner_sparse.c',
    'memory': 'nested_atomic_memory.c',
    'domain-memory': 'nested_domain_memory.c',
    'smp': 'smp.c',
    'maintenance': 'nested_owner_atomic_maintenance.c',
    'retrack': 'nested_atomic_retrack.c',
    'multiround': 'nested_atomic_multiround.c',
    'provision': 'nested_atomic_uc_provision.c',
    'payload': 'ept_payload.S',
}
PROFILE_FLAGS = {
    '-DVMX_NESTED_TEST',
    '-DVMX_NESTED_ATOMIC_CATALOG_TEST',
    '-DVMX_NESTED_ATOMIC_RETRACK_TEST',
    '-DVMX_NESTED_ATOMIC_MULTIROUND_TEST',
    '-DVMX_NESTED_ATOMIC_UC_TEST',
}
FREESTANDING_FLAGS = {'-O1', '-ffreestanding', '-mgeneral-regs-only', '-DSYLE_KERNEL'}
OTHER_PROFILES = {
    '-DVMX_NESTED_ATOMIC_LARGEPAGE_TEST',
    '-DVMX_NESTED_ATOMIC_GIGAPAGE_TEST',
    '-DVMX_NESTED_OWNER_TYPED_TEST',
}

work = Path(tempfile.mkdtemp(prefix='syle-n46b-freestanding-'))
print('N46B_FREESTANDING_EVIDENCE', work, flush=True)
manifest = {}
for directory, names, files in os.walk(ROOT):
    names[:] = sorted(n for n in names if n not in {'.git', 'build', '__pycache__'})
    for name in names:
        assert not (Path(directory) / name).is_symlink()
    for name in sorted(files):
        source = Path(directory) / name
        assert not source.is_symlink()
        relative = source.relative_to(ROOT)
        # A frozen input receipt is not source and would collide with our own
        # generated manifest. Every copied source is still hashed and checked.
        if relative == Path("source-sha256.json"):
            continue
        target = work / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        manifest[str(relative)] = hashlib.sha256(target.read_bytes()).hexdigest()
(work / 'source-sha256.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
for name in ('efi/Makefile', 'krnl/nested_atomic_uc_runtime.inc',
             'krnl/nested_atomic_uc_bootstrap.inc', 'krnl/nested_atomic_uc_provision.c'):
    assert name in manifest, name
assert not (work / 'efi/build').exists()

result = subprocess.run(
    ['make', '--no-print-directory', TARGET], cwd=work / 'efi', text=True,
    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180,
)
(work / 'freestanding-build.log').write_text(result.stdout)
assert result.returncode == 0, result.stdout
commands = [shlex.split(line) for line in result.stdout.splitlines()]


def compile_command(source, object_name):
    matches = [cmd for cmd in commands if '-c' in cmd and
               cmd[cmd.index('-c') + 1] == '../krnl/' + source]
    assert len(matches) == 1, (source, matches)
    command = matches[0]
    assert command[command.index('-o') + 1] == 'build/' + object_name, command
    return command


for suffix, source in PROFILE_SOURCES.items():
    command = compile_command(source, 'k-nested46b-' + suffix + '.o')
    assert PROFILE_FLAGS <= set(command), (source, command)
    assert not OTHER_PROFILES & set(command), (source, command)
    if suffix != 'payload':
        assert FREESTANDING_FLAGS <= set(command), (source, command)
        assert [flag for flag in command if flag.startswith('-O')] == ['-O1'], command
for source, object_name in (
    ('typed_root_atomic.c', 'k-typed-root-atomic.o'),
    ('cache_maintenance.c', 'k-cache-maintenance.o'),
    ('cache_page.c', 'k-cache-page.o'),
):
    command = compile_command(source, object_name)
    assert FREESTANDING_FLAGS <= set(command), (source, command)

elf_path = work / 'efi/build' / ('krnl-' + TARGET + '.elf')
links = [cmd for cmd in commands if '-o' in cmd and
         cmd[cmd.index('-o') + 1] == 'build/' + elf_path.name]
assert len(links) == 1, links
link = links[0]
assert {'-nostdlib', '-static', '-T', '../krnl/link.ld'} <= set(link), link
for suffix in PROFILE_SOURCES:
    assert 'build/k-nested46b-' + suffix + '.o' in link, suffix
for object_name in ('k-cache-maintenance.o', 'k-cache-page.o'):
    assert 'build/' + object_name in link, object_name
assert not any(name.startswith(('build/k-nested43-', 'build/k-nested44-',
                                'build/k-nested45-')) for name in link), link

for path in sorted((work / 'efi/build').glob('k-*.o')) + [elf_path]:
    undefined = subprocess.check_output(['nm', '-u', str(path)], text=True)
    assert not {'memset', 'memcpy', 'memmove', 'bzero'} & {
        line.split()[-1] for line in undefined.splitlines()
    }, (path.name, undefined)
    if path == elf_path:
        assert not undefined.strip(), undefined
    print('FREESTANDING_SHA256', hashlib.sha256(path.read_bytes()).hexdigest(), path.name)

elf = elf_path.read_bytes()
assert elf.startswith(b'\x7fELF')
assert elf == (work / 'efi/build' / (TARGET + '-esp/syle/krnl.elf')).read_bytes()
symbols = subprocess.check_output(['nm', str(elf_path)], text=True).splitlines()
for symbol in (
    'nauc_run', 'nrc_init_atomic_uc', 'nested_ept_atomic_walk_uc_ipat',
    'trl_init_atomic_uc', 'typed_root_registry_init_atomic_uc',
    'nested_ept_sparse_set_typed_admission_atomic_uc',
    'cache_maintenance_precheck', 'cache_maintenance_tlb', 'cache_maintenance_cache',
    'cache_maintenance_lease_current', 'cache_page_audit_current',
):
    assert any(line.endswith(' T ' + symbol) for line in symbols), symbol

# These dry runs check include dependencies without executing any extra build.
for dependency, object_name in (
    ('nested_atomic_uc_runtime.inc', 'k-nested46b-smp.o'),
    ('nested_atomic_uc_bootstrap.inc', 'k-nested46b-smp.o'),
    ('nested_atomic_uc_provision.h', 'k-nested46b-provision.o'),
    ('nested_atomic_multiround_l1.inc', 'k-nested46b-payload.o'),
    ('nested_atomic_multiround_l2.inc', 'k-nested46b-payload.o'),
):
    planned = subprocess.check_output(
        ['make', '--no-print-directory', '-n', '-W', '../krnl/' + dependency,
         'build/' + object_name], cwd=work / 'efi', text=True,
    )
    assert ' -c ' in planned and ' -o build/' + object_name in planned, (dependency, planned)

for name, digest in manifest.items():
    assert hashlib.sha256((work / name).read_bytes()).hexdigest() == digest, name
print('PASS N46B fresh isolated UC-profile kernel/ESP link; required UC, provision and cache symbols; '
      'no undefined libc or kernel symbols; compile/link only, no guest execution')
