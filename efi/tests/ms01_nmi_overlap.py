#!/usr/bin/env python3
"""Real QEMU NMI delivery during a CPU #UD handler, not INT 2 or a preset guard.
Representative checkpoints, not exhaustive instruction-window coverage. A second
NMI while NMI-blocked is NOT claimed. No bare-metal/AP coverage is implied.
"""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/ms01-nmi-overlap'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    os.chdir(ROOT)
    OUT.mkdir(parents=True, exist_ok=True)
    # A failed rerun must not expose an older successful report.
    for name in ('report.json', 'coverage.json'):
        (OUT / name).unlink(missing_ok=True)
    with (ROOT / 'build/.ms01-build.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        subprocess.run(['make', 'all'], check=True, stdout=subprocess.DEVNULL)
        esp = OUT / 'esp'
        shutil.rmtree(esp, ignore_errors=True)
        shutil.copytree(ROOT / 'build/esp', esp)
        elf = OUT / 'krnl.elf'
        shutil.copyfile(ROOT / 'build/krnl.elf', elf)
    manifest = {str(p.relative_to(OUT)): sha(p) for p in [elf, *sorted(esp.rglob('*'))] if p.is_file()}
    assert sha(elf) == sha(esp / 'syle/krnl.elf')
    (OUT / 'image-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    # Static emergency-path contract: finite literal, bounded poll, immediate
    # exit on exhausted budget/absent COM1; no stack or console dependencies.
    assembly = (ROOT / 'src/exceptions.S').read_text()
    emergency = assembly.split('nmi_emergency_stop:', 1)[1].split('# Fatal path:', 1)[0]
    assert all(word not in emergency for word in ['push', 'pop', 'call', '%rsp', 'iret'])
    assert 'movl $1024, %ecx' in emergency
    assert 'decl %ecx\n    jnz .Lnmi_poll\n    jmp .Lnmi_done' in emergency
    assert 'cmpb $0xff, %al\n    je .Lnmi_done' in emergency
    assert 'nmi_marker: .asciz "\\nPANIC.SECOND_FAULT\\n"' in assembly
    for reg in ['rax', 'rcx', 'rdx', 'rsi']:
        assert f'movq %{reg}, nmi_marker_{reg}(%rip)' in emergency
        assert f'movq nmi_marker_{reg}(%rip), %{reg}' in emergency
    results = []
    for name in ['early-saved-frame', 'during-uart', 'after-uart-before-screen', 'early-no-uart']:
        script = OUT / (name + '.gdb')
        serial = OUT / (name + '.serial.log')
        evidence = OUT / (name + '.frame.json')
        checkpoint = ''
        if name == 'during-uart':
            checkpoint = '''
class FieldBreakpoint(gdb.Breakpoint):
    def stop(self):
        return gdb.parse_and_eval('(char*)$rdi').string() == 'rip='
bp = FieldBreakpoint('log_text', type=gdb.BP_HARDWARE_BREAKPOINT)
gdb.execute('continue')
bp.delete()
'''
        elif name == 'after-uart-before-screen':
            checkpoint = "gdb.execute('hbreak console_panic_style')\ngdb.execute('continue')\ngdb.execute('delete breakpoints')\n"
        script.write_text(f'''set pagination off
set confirm off
file {elf}
target remote :12443
hbreak vmx_probe
continue
delete breakpoints
set $probe = $rip
set {{unsigned char}}$probe = 0x0f
set {{unsigned char}}($probe+1) = 0x0b
set $rbx = 0x123456789abcdef0
''' + f'''hbreak exception_panic
continue
delete breakpoints
python
import gdb, json
inferior = gdb.selected_inferior()
value = lambda expr: int(gdb.parse_and_eval(expr))
frame = value('$rdi')
size = value('sizeof(struct frame)')
first = bytes(inferior.read_memory(frame, size))
f = gdb.parse_and_eval('(struct frame*)$rdi').dereference()
fields = {{field.name: int(f[field.name]) for field in f.type.fields()}}
assert fields['vector'] == 6 and fields['error'] == 0
assert fields['rip'] == value('$probe')
assert fields['rbx'] == 0x123456789abcdef0
lo = value('&exception_stack')
assert lo <= frame and frame + size <= lo + value('sizeof(exception_stack)')
assert value('*(unsigned char*)&exception_entered') == 1
assert value('*(unsigned char*)&nmi_entered') == 0
{checkpoint}
interrupted_rip, interrupted_rsp = value('$rip'), value('$rsp')
assert bytes(inferior.read_memory(frame, size)) == first
gdb.execute('hbreak exception_2')
gdb.execute('monitor nmi')
gdb.execute('continue')
gdb.execute('delete breakpoints')
nmi_rsp = value('$rsp')
scratch = {{r: value('$'+r) for r in ['rax', 'rcx', 'rdx', 'rsi']}}
assert value('&nmi_stack') <= nmi_rsp < value('&nmi_stack') + value('sizeof(nmi_stack)')
assert not lo <= nmi_rsp < lo + value('sizeof(exception_stack)')
assert value('*(unsigned long long*)$rsp') == interrupted_rip
assert value('*(unsigned long long*)($rsp+24)') == interrupted_rsp
nmi_hardware = bytes(inferior.read_memory(nmi_rsp, 40))
gdb.execute('hbreak exception_stop')
gdb.execute('continue')
assert value('*(unsigned char*)&nmi_entered') == 1
assert value('tss.ist[2]') == value('&nmi_spare_top')
ranges = [(value('&exception_stack'), value('sizeof(exception_stack)')),
          (value('&nmi_stack'), value('sizeof(nmi_stack)')),
          (value('&nmi_spare'), value('&nmi_spare_top') - value('&nmi_spare'))]
for index, (base, length) in enumerate(ranges):
    for other, other_length in ranges[index+1:]:
        assert base+length <= other or other+other_length <= base
assert value('$rsp') == nmi_rsp
assert all(value('$'+r) == v for r, v in scratch.items())
assert bytes(inferior.read_memory(nmi_rsp, 40)) == nmi_hardware
assert bytes(inferior.read_memory(frame, size)) == first
gdb.execute('stepi')
assert not value('$eflags') & 512
assert 'hlt' in gdb.execute('x/i $pc', to_string=True)
with open({str(evidence)!r}, 'w') as stream:
    json.dump(dict(first_frame_address=frame, first_frame_fields=fields,
                   before_hex=first.hex(), after_hex=bytes(inferior.read_memory(frame,size)).hex(),
                   interrupted_rip=interrupted_rip, interrupted_rsp=interrupted_rsp,
                   nmi_hardware_frame_address=nmi_rsp, nmi_hardware_frame_hex=nmi_hardware.hex(),
                   next_nmi_ist=value('tss.ist[2]'), terminal_if=0), stream, indent=2)
print('NMI_OVERLAP_PASS')
end
detach
quit
''')
        env = dict(os.environ, ESP_DIR=str(esp), QEMU_GDB_PORT='12443',
                   NO_UART='1' if name == 'early-no-uart' else '0',
                   OVMF_VARS_RUNTIME=str(OUT / (name + '.vars.fd')))
        with serial.open('w') as log:
            process = subprocess.Popen(['scripts/run.sh', 'debug'], env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                time.sleep(.5)
                run = subprocess.run([os.environ.get('GDB', 'gdb'), '-q', '-batch', '-x', str(script)],
                                     text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=100)
                (OUT / (name + '.gdb.log')).write_text(run.stdout)
                assert run.returncode == 0 and 'NMI_OVERLAP_PASS' in run.stdout, run.stdout
                time.sleep(.2)
                text = serial.read_text(errors='replace')
                if name == 'early-no-uart':
                    assert 'PANIC.SECOND_FAULT' not in text
                else:
                    assert text.count('PANIC.SECOND_FAULT') == 1
                if name == 'early-saved-frame':
                    assert 'REAL CPU EXCEPTION' not in text
                    assert 'vector=' not in text

                if name in ('during-uart', 'after-uart-before-screen'):
                    assert 'vector=0x0000000000000006' in text
                    assert 'error=0x0000000000000000' in text
                if name == 'after-uart-before-screen':
                    assert 'PANIC.COMPLETE' in text
                else:
                    assert 'PANIC.COMPLETE' not in text
                results.append(dict(case=name, status='PASS', frame_unchanged=True,
                                    method='CPU UD2 then QEMU monitor nmi during ordinary exception',
                                    second_fault_marker=name != 'early-no-uart',
                                    emergency_scratch_preserved=True,
                                    first_uart='complete' if name == 'after-uart-before-screen' else
                                    'prefix retained' if name == 'during-uart' else 'none yet; saved frame only',
                                    terminal='IF=0 next instruction HLT, no IRET'))
                print('PASS', name, flush=True)
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    assert all(sha(OUT / path) == digest for path, digest in manifest.items())
    (OUT / 'coverage.json').write_text(json.dumps(dict(
        scenarios=['SC-M1-047', 'SC-M1-048', 'SC-M1-056'],
        actual_delivery='QEMU hardware NMI during an active CPU UD2 exception',
        marker_scope='NMI entry finding an already active fatal exception; best effort COM1 only',
        static_poll_limit=1024,
        static_marker_max_bytes=len('\nPANIC.SECOND_FAULT\n'),
        limits=['Representative checkpoints, not all instruction windows or vectors',
                'No claim of natural nested NMI delivery while NMI blocking remains active',
                'First diagnostic is not serialized at the early checkpoint; only debugger frame survives',
                'UART absence or a stalled transmitter can suppress/truncate the emergency marker',
                'Busy-transmitter finite budget is checked statically, not an electrical UART stall',
                'No bare-metal, AP or timer qualification']), indent=2) + '\n')
    (OUT / 'report.json').write_text(json.dumps(results, indent=2) + '\n')

if __name__ == '__main__':
    main()
