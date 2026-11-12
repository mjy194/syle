#!/usr/bin/env python3
"""Real OVMF/TCG runs against isolated ESP copies, no edits to default image."""
from pathlib import Path
import os
import json
import socket
import shutil
import subprocess
import time

root = Path(__file__).resolve().parents[1]
os.chdir(root)
subprocess.run(['make', 'all'], check=True)


def run(name, config, marker, *, image='valid', no_gop=False, key=None, kernel='valid'):
    esp = root / 'build' / ('chain-' + name + '-esp')
    if esp.exists():
        shutil.rmtree(esp)
    shutil.copytree(root / 'build/esp', esp)
    cfg = esp / 'syle/boot.cfg'
    if config is None:
        cfg.unlink()
    else:
        cfg.write_text(config)
    if image == 'missing':
        (esp / 'syle/mgmt.elf').unlink()
    elif image == 'invalid':
        (esp / 'syle/mgmt.elf').write_bytes(b'not an ELF')
    if kernel == 'missing':
        (esp / 'syle/krnl.elf').unlink()
    elif kernel == 'invalid':
        (esp / 'syle/krnl.elf').write_bytes(b'not a kernel ELF')
    env = dict(os.environ, ESP_DIR=str(esp))
    if no_gop:
        env['NO_GOP'] = '1'
    if key:
        env['QMP_PORT'] = '14446'
    path = root / 'build' / ('chain-' + name + '.log')
    with path.open('w') as log:
        proc = subprocess.Popen(['scripts/run.sh', 'run'], env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            key_sent = False
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                text = path.read_text(errors='replace')
                if key and not key_sent and 'Enter: configured default' in text:
                    with socket.create_connection(('127.0.0.1', 14446), timeout=5) as sock:
                        f = sock.makefile('rwb', buffering=0)
                        json.loads(f.readline())
                        for command in ({'execute': 'qmp_capabilities'},
                                        {'execute': 'human-monitor-command',
                                         'arguments': {'command-line': 'sendkey ' + key}}):
                            f.write((json.dumps(command) + '\n').encode())
                            while True:
                                response = json.loads(f.readline())
                                if 'error' in response:
                                    raise AssertionError(response)
                                if 'return' in response:
                                    break
                    key_sent = True
                if marker in text:
                    break
                if proc.poll() is not None:
                    raise AssertionError(text)
                time.sleep(.1)
            else:
                raise AssertionError('Timeout: ' + text)
            if key:
                assert key_sent, text
            if marker == 'KRNL.IDLE':
                required = ['SYLE.BOOT_ENTRY', 'BOOT.USER_IMAGE_READY',
                            'BOOT.KERNEL_LOADED', 'STAGE0.EXIT_BOOT_SERVICES_BEGIN',
                            'KRNL.STANDALONE_ELF', 'KRNL.OWN_STARTUP_STACK', 'SYLE.KRNL_ENTRY',
                            'KRNL.PMM_READY', 'KRNL.EARLY_OWN_CR3=',
                            'MGMT.ELF_LOADED', 'RING3.OWN_CR3_AND_STACK',
                            'SYLE.MGMT:', 'MGMT.EXIT', 'KRNL.IDLE']
                positions = [text.find(m) for m in required]
                assert all(p >= 0 for p in positions) and positions == sorted(positions), text
                assert 'PANIC' not in text and 'ERROR.' not in text, text
            if name in ('diagnostic', 'bad-config'):
                assert 'BOOT.USER_IMAGE_READY' not in text and 'SYLE.MGMT:' not in text, text
            if image == 'missing' or kernel != 'valid':
                assert 'STAGE0.EXIT_BOOT_SERVICES_BEGIN' not in text, text
            if no_gop:
                assert 'SCREEN.UNAVAILABLE' in text, text
            print('PASS boot chain', name)
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

run('normal', 'default=normal\ntimeout=0\ntitle=Syle test\n', 'KRNL.IDLE')
run('missing-config', None, 'KRNL.IDLE')
run('diagnostic', 'default=diagnostic\ntimeout=0\n', 'KRNL.DIAGNOSTIC_IDLE')
run('bad-config', 'default=surprise\n', 'KRNL.DIAGNOSTIC_IDLE')
run('missing-user', 'timeout=0\n', 'ERROR.USER_IMAGE_READ', image='missing')
run('invalid-user', 'timeout=0\n', 'INVALID MANAGEMENT ELF', image='invalid')
run('no-gop', 'timeout=0\n', 'KRNL.IDLE', no_gop=True)

run('keyboard-2', 'default=normal\ntimeout=30\n', 'KRNL.DIAGNOSTIC_IDLE', key='2')
run('keyboard-1', 'default=diagnostic\ntimeout=30\n', 'KRNL.IDLE', key='1')

run('missing-kernel', 'timeout=0\n', 'ERROR.KERNEL_FILE_READ', kernel='missing')
run('invalid-kernel', 'timeout=0\n', 'ERROR.KERNEL_ELF', kernel='invalid')
