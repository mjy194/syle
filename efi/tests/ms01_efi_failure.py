#!/usr/bin/env python3
"""SC-M1-BOOT-05: actual EFI adapter fatal EBS path, under OVMF/TCG.
Only firmware ExitBootServices machine code is replaced in stopped guest RAM.
The production loader, adapters, retry controller and output execute unchanged.
This is deterministic fault injection, not physical/partially-torn-down firmware.
"""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
OUT = ROOT / 'build/ms01-efi-failure'
OUT.mkdir(parents=True, exist_ok=True)
# A failed rerun must never leave a previous aggregate PASS as its result.
(OUT / 'report.json').unlink(missing_ok=True)
PORT = '12441'

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def stop(p):
    if p.poll() is None:
        p.terminate()
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()

# Build and snapshot under the same lock used by other MS01 runners.
with (ROOT / 'build/.ms01-build.lock').open('w') as lock:
    fcntl.flock(lock, fcntl.LOCK_EX)
    subprocess.run(['make', '-j4', 'all'], check=True)
    esp = OUT / 'esp'
    shutil.rmtree(esp, ignore_errors=True)
    shutil.copytree(ROOT / 'build/esp', esp)
    for name in ('stage0.so', 'krnl.elf'):
        shutil.copyfile(ROOT / 'build' / name, OUT / name)
# Fast deterministic selection; all image/symbol bytes remain unchanged.
(esp / 'syle/boot.cfg').write_text('default=diagnostic\ntimeout=0\n')
inputs = [OUT / 'stage0.so', OUT / 'krnl.elf',
          *sorted(p for p in esp.rglob('*') if p.is_file())]
manifest = {str(p.relative_to(OUT)): sha(p) for p in inputs}
assert sha(OUT / 'krnl.elf') == sha(esp / 'syle/krnl.elf')
(OUT / 'image-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
env = dict(os.environ, ESP_DIR=str(esp), QEMU_GDB_PORT=PORT,
           OVMF_VARS_RUNTIME=str(OUT / 'vars.fd'))
# Discover relocation using the same private ESP/firmware, then verify it again.
serial = OUT / 'baseline.serial.log'
with serial.open('w') as stream:
    p = subprocess.Popen(['scripts/run.sh'], env=env, stdout=stream, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            text = serial.read_text(errors='replace')
            if 'KRNL.DIAGNOSTIC_IDLE' in text:
                break
            if p.poll() is not None:
                raise AssertionError(text)
            time.sleep(.1)
        else:
            raise TimeoutError(text)
        base = re.search(r'IMAGE.BASE=(0x[0-9a-f]+)', text)[1]
    finally:
        stop(p)

DRIVER = r'''
import gdb, json, time

def val(s): return int(gdb.parse_and_eval(s))
def check(ok, message):
    if not ok: raise gdb.GdbError(message)
def reach(s):
    address = val('&'+s)
    bp = gdb.Breakpoint('*'+s, type=gdb.BP_HARDWARE_BREAKPOINT, temporary=True)
    gdb.execute('continue')
    check(val('$pc') == address and not bp.is_valid(), 'unexpected stop before '+s)
reach('final_exit')
# The first final_map has already completed. Thereafter every attempt must be
# preceded by a newly executed firmware map call, with its returned key.
ctx = gdb.parse_and_eval('(struct efi_context*)$rdi')
bs = ctx['system']['BootServices'].dereference()
exit_addr = int(bs['ExitBootServices'])
map_addr = int(bs['GetMemoryMap'])
state = {'attempts': 0, 'maps': 0, 'keys': [], 'map_keys': [], 'forbidden': []}
first_key = val('$rsi')
# EFI_INVALID_PARAMETER (stale key) or EFI_DEVICE_ERROR. This stub executes
# through uefi_call_wrapper and the real final_exit status-normalizing adapter.
gdb.selected_inferior().write_memory(exit_addr, b'\x48\xb8' + STATUS.to_bytes(8,'little') + b'\xc3')
class Forbidden(gdb.Breakpoint):
    def __init__(self, address, label):
        super().__init__('*'+hex(address), type=gdb.BP_HARDWARE_BREAKPOINT)
        self.label = label
    def stop(self):
        state['forbidden'].append(self.label)
        return True
# Guard every other Boot Services entry, not merely the core's alloc callbacks.
seen = {exit_addr, map_addr}
for field in bs.type.fields():
    if field.type.code == gdb.TYPE_CODE_PTR:
        address = int(bs[field.name])
        if address and address not in seen:
            Forbidden(address, 'BootServices.'+field.name)
            seen.add(address)
for name in ('stage0_post_ebs', 'kernel_entry'):
    Forbidden(val('&'+name), name)
class MapReturn(gdb.FinishBreakpoint):
    def __init__(self, keyptr):
        super().__init__(gdb.newest_frame(), internal=True)
        self.keyptr = keyptr
    def stop(self):
        check(val('$rax') == 0, 'retry GetMemoryMap did not succeed')
        state['map_keys'].append(val('*(unsigned long long*)'+str(self.keyptr)))
        return False
class Map(gdb.Breakpoint):
    def stop(self):
        state['maps'] += 1
        MapReturn(val('$r8')) # EFI/MS x64 GetMemoryMap third argument
        return False
class Exit(gdb.Breakpoint):
    def stop(self):
        state['attempts'] += 1
        key = val('$rdx') # EFI/MS x64 ExitBootServices second argument
        state['keys'].append(key)
        check(state['maps'] == state['attempts']-1, 'missing fresh map before retry')
        check(key == (first_key if state['attempts']==1 else state['map_keys'][-1]), 'map key not passed to EBS')
        check(state['attempts'] <= EXPECTED, 'retry bound exceeded')
        return False
Map('*'+hex(map_addr), type=gdb.BP_HARDWARE_BREAKPOINT)
Exit('*'+hex(exit_addr), type=gdb.BP_HARDWARE_BREAKPOINT)
# Locate the compiler's terminal HLT in actual platform_launch instructions.
start = val('&platform_launch')
block = gdb.block_for_pc(start)
while block.function is None:
    block = block.superblock
insns = gdb.selected_frame().architecture().disassemble(start, block.end)
halts = [i['addr'] for i in insns if i['asm'].strip()=='hlt']
check(halts, 'no terminal HLT found')
halt = halts[0]
bp = gdb.Breakpoint('*'+hex(halt), type=gdb.BP_HARDWARE_BREAKPOINT)
gdb.execute('continue')
check(val('$pc') == halt, 'unexpected stop: '+repr(state))
check(not state['forbidden'], 'forbidden callback/transfer: '+repr(state))
check(state['attempts'] == EXPECTED, 'incorrect attempt count')
check(state['maps'] == EXPECTED-1, 'incorrect fresh-map count')
check(not (val('$eflags') & 512), 'CLI did not clear IF')
check(val('boot.boot_services_exited') == 0, 'false successful EBS flag')
# Decode terminal back-edge as well: HLT followed by a branch back to HLT.
terminal = gdb.selected_frame().architecture().disassemble(halt, halt+8)
check(terminal[1]['asm'].startswith('jmp') and hex(halt)[2:] in terminal[1]['asm'], 'terminal is not a bounded halt loop')
state['if_clear'] = True
state['terminal_hlt'] = hex(halt)
# Let HLT actually execute, then reconnect and check the guest stayed there.
# This is bounded by the outer debugger timeout, never an unbounded wait.
gdb.execute('delete breakpoints')
gdb.execute('detach')
time.sleep(.2)
gdb.execute('target remote 127.0.0.1:12441')
check(val('$pc') == terminal[1]['addr'], 'guest did not remain halted after HLT')
check(not (val('$eflags') & 512), 'IF changed at halted terminal')
state['halt_executed'] = True
with open(REPORT, 'w') as f: json.dump(state, f, indent=2)
gdb.write('EFI_FAILURE_PASS\n')
gdb.execute('detach')
'''
results = []
for name, status, expected in [('persistent-stale', 0x8000000000000002, 3),
                               ('nonretryable-error', 0x8000000000000007, 1)]:
    driver = OUT / (name + '.gdb.py')
    report = OUT / (name + '.json')
    driver.write_text('STATUS=%d\nEXPECTED=%d\nREPORT=%r\n' % (status, expected, str(report)) + DRIVER)
    serial = OUT / (name + '.serial.log')
    with serial.open('w') as stream:
        p = subprocess.Popen(['scripts/run.sh', 'debug'], env=env, stdout=stream, stderr=subprocess.STDOUT)
        try:
            time.sleep(.5)
            run = subprocess.run([os.environ.get('GDB', 'gdb'), '-q', '-batch',
                '-ex', 'set pagination off', '-ex', 'set confirm off',
                '-ex', 'set tcp auto-retry on', '-ex', 'set remotetimeout 10',
                '-ex', 'target remote 127.0.0.1:'+PORT,
                '-ex', f'add-symbol-file {OUT / "stage0.so"} -o {base}',
                '-ex', f'add-symbol-file {OUT / "krnl.elf"}',
                '-ex', f'source {driver}'], text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, timeout=120)
            (OUT / (name + '.gdb.log')).write_text(run.stdout)
            assert run.returncode == 0 and 'EFI_FAILURE_PASS' in run.stdout, run.stdout
            time.sleep(.2)
            text = serial.read_text(errors='replace')
            assert f'IMAGE.BASE={base}' in text, text
            assert text.count('ERROR.EXIT_BOOT_SERVICES') == 1, text
            assert 'ERROR.BOOT_LAUNCH' not in text and 'KRNL.' not in text, text
            normalized = 2 if expected == 3 else 3
            assert f'ERROR.EXIT_BOOT_SERVICES\n0x{normalized:016x}' in text, text
            results.append({'scenario': 'SC-M1-BOOT-05', 'case': name,
                            'status': 'PASS', **json.loads(report.read_text())})
            print('PASS SC-M1-BOOT-05', name, flush=True)
        finally:
            stop(p)
assert manifest == {str(p.relative_to(OUT)): sha(p) for p in inputs}, 'input image changed'
(OUT / 'report.json').write_text(json.dumps(results, indent=2) + '\n')
