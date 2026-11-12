#!/usr/bin/env python3
"""Small read-only provenance record. Not a hermetic/reproducible-build claim.
Only explicitly selected tool/version and build-input paths are examined; the
process environment, credentials and unrelated host files are never dumped.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def file_record(path):
    p = Path(path)
    result = {'path': str(p)}
    try:
        if not p.is_file():
            result['status'] = 'missing-or-not-regular-file'
        else:
            result.update(status='recorded', bytes=p.stat().st_size,
                          sha256=hashlib.sha256(p.read_bytes()).hexdigest())
    except OSError as error:
        result.update(status='unavailable', error=str(error))
    return result


def collect():
    result = {'scope': 'Selected executable versions and firmware/build inputs only; dynamic libraries, host kernel and every build dependency are not captured.',
              'tools': {}, 'inputs': {}}
    for name, env_key, default in [('gcc', None, 'gcc'), ('ld', None, 'ld'),
                                   ('make', None, 'make'),
                                   ('qemu', 'QEMU', 'qemu-system-x86_64'),
                                   ('gdb', 'GDB', 'gdb')]:
        # The aggregate invokes literal make; efi/Makefile fixes CC=gcc/LD=ld.
        # Do not mislabel an unused CC/LD/MAKE environment override as that tool.
        executable = os.environ.get(env_key, default) if env_key else default
        entry = {'command': [executable, '--version']}
        resolved = shutil.which(executable)
        if resolved:
            entry['executable'] = file_record(resolved)
        try:
            run = subprocess.run(entry['command'], text=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 timeout=10, check=False)
            entry.update(status='recorded' if run.returncode == 0 else 'version-command-failed',
                         exitCode=run.returncode, versionLines=run.stdout.splitlines()[:4])
        except (OSError, subprocess.TimeoutExpired) as error:
            entry.update(status='unavailable', error=str(error))
        result['tools'][name] = entry
    for name in ('OVMF_CODE', 'OVMF_VARS', 'VGA_ROM'):
        value = os.environ.get(name)
        result['inputs'][name] = file_record(value) if value else {'status': 'not-explicitly-set'}
    prefix = os.environ.get('EFI_PREFIX')
    if prefix:
        for relative in ('include/efi/efi.h', 'lib/crt0-efi-x86_64.o',
                         'lib/libefi.a', 'lib/libgnuefi.a'):
            result['inputs']['EFI_PREFIX/' + relative] = file_record(Path(prefix) / relative)
    return result


if __name__ == '__main__':
    print(json.dumps(collect(), indent=2))
