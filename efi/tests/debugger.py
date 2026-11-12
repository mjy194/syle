#!/usr/bin/env python3
"""Prove reset-stop, relocatable symbols and source-level EFI/post-EBS breakpoints."""
import os
from pathlib import Path
import re
import subprocess
import time

root = Path(__file__).resolve().parents[1]
os.chdir(root)

def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()

preflight = root / 'build/debug-preflight.log'
with preflight.open('w') as log:
    process = subprocess.Popen(['scripts/run.sh', 'run'], stdout=log, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            data = preflight.read_text(errors='replace')
            if 'STAGE0.POST_EBS' in data:
                break
            if process.poll() is not None:
                raise RuntimeError(data)
            time.sleep(0.1)
        else:
            raise TimeoutError('Normal boot did not reach post-EBS')
        base = re.search(r'IMAGE.BASE=(0x[0-9a-f]+)', data).group(1)
    finally:
        stop(process)

serial = root / 'build/debug-serial.log'
with serial.open('w') as log:
    process = subprocess.Popen(['scripts/run.sh', 'debug'], stdout=log, stderr=subprocess.STDOUT)
    try:
        # QEMU startup before GDB connects; remote retry handles slower starts.
        time.sleep(0.5)
        # One-shot exact-address breakpoints avoid retaining old stops. Check
        # each actual PC before announcing a stage, rather than assuming every
        # `continue` reaches the next breakpoint (the old harness did so).
        driver = Path('build/gdb-stage-check.py')
        driver.write_text("""import gdb

def fail(message):
    gdb.write('GDB.CHECK_FAILED ' + message + '\\n')
    gdb.execute('quit 1')

def reach(symbol, marker):
    address = int(gdb.parse_and_eval('&' + symbol))
    bp = gdb.Breakpoint('*' + symbol, type=gdb.BP_HARDWARE_BREAKPOINT,
                        temporary=True)
    gdb.execute('continue')
    pc = int(gdb.parse_and_eval('$pc'))
    if pc != address or bp.is_valid():
        fail('unexpected stop at %#x, expected %s=%#x' % (pc, symbol, address))
    gdb.write(marker + '\\n')

reach('efi_main', 'GDB.ENTRY_HIT')
reach('stage0_post_ebs', 'GDB.POST_EBS_HIT')
gdb.execute('info registers rip rsp')
gdb.execute('add-symbol-file build/krnl.elf')
reach('kernel_entry', 'GDB.KERNEL_ENTRY_HIT')
reach('kernel_runtime', 'GDB.KERNEL_STACK_HIT')
rsp = int(gdb.parse_and_eval('$rsp'))
low = int(gdb.parse_and_eval('&kernel_startup_stack'))
high = int(gdb.parse_and_eval('&kernel_startup_stack_top'))
if not low <= rsp < high:
    fail('kernel stack out of bounds')
gdb.write('GDB.OWN_STACK=1\\n')
gdb.execute('info registers rip rsp')
gdb.execute('detach')
""")
        result = subprocess.run([
            os.environ.get('GDB', 'gdb'), '-q', '-batch',
            '-ex', 'set pagination off',
            '-ex', 'set confirm off',
            '-ex', 'set tcp auto-retry on',
            '-ex', 'set remotetimeout 10',
            '-ex', 'target remote 127.0.0.1:1234',
            '-ex', f'add-symbol-file build/stage0.so -o {base}',
            '-ex', f'source {driver}',
        ], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        Path('build/gdb.log').write_text(result.stdout)
        print(result.stdout)
        assert result.returncode == 0
        assert 'GDB.CHECK_FAILED' not in result.stdout
        assert result.stdout.count('GDB.ENTRY_HIT') == 1
        assert result.stdout.count('GDB.POST_EBS_HIT') == 1
        assert 'GDB.KERNEL_ENTRY_HIT' in result.stdout
        assert 'GDB.KERNEL_STACK_HIT' in result.stdout
        assert 'GDB.OWN_STACK=1' in result.stdout
        assert f'IMAGE.BASE={base}' in serial.read_text(errors='replace')
        print('PASS: GDB EFI -> post-EBS -> independent kernel entry -> own startup stack')
    finally:
        stop(process)
