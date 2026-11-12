#!/usr/bin/env python3
"""Execute INT 32..255 on one QEMU/OVMF BSP and inspect actual CPU delivery.

Run after configuring the normal EFI/QEMU/GDB dependency environment:
    python3 efi/tests/ms01_fallback_vectors.py

The default covers all 224 fallback vectors. --vectors 32,127,128,254,255 is
a focused harness check, never an all-vector pass. The debugger patches two
bytes at the stopped vmx_probe entry and observes the CPU at the IDT target,
exception_panic entry, and exception_stop. After each case it explicitly
resets GPRs/RSP/RIP/RFLAGS/SS and the two panic guards. This is test intervention,
NOT kernel recovery or an IRET path. No production source is changed.

Scope: CPL0 software INT delivery on one TCG BSP before Ring 3 replaces gate
128. Not hardware IRQ routing, first-32 exception semantics, NMI/#DF, APs,
bare-metal qualification, interrupt-enabled execution, or production recovery.
"""

import argparse
import contextlib
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
PORT = 12445
ALL_VECTORS = list(range(32, 256))
LIMITS = [
    "CPL0 software INT on one QEMU TCG BSP; no hardware IRQ routing evidence",
    "Fallback gate 128 is checked before the later Ring 3 syscall gate change",
    "No first-32 exception semantics, NMI/#DF, AP, or bare-metal qualification",
    "Input IF is zero; does not test maskable interrupt delivery or IF clearing",
    "Debugger resets registers and two panic guards between cases; not recovery",
    "Stops immediately before HLT; no sustained halt or IRET execution claim",
]
ASSERTIONS = [
    "Each gate is a present DPL0 64-bit interrupt gate (0x8e), selector 8, IST1, reserved 0",
    "Each gate address equals its own exception_N symbol and is unique",
    "CPU executes bytes CD NN at the checkpoint and stops at that exact exception_N entry",
    "Raw CPU frame is at IST1 top minus 40: RIP=probe+2, CS, RFLAGS, old RSP, SS",
    "Raw entry preserves all 15 injected GPRs and has IF=0",
    "C frame is wholly on exception_stack at IST1 top minus sizeof(frame)=176",
    "C frame contains the exact unsigned vector, synthetic error=0, and matching hardware frame",
    "All 15 saved GPRs equal their vector-specific injected values",
    "C entry stack is SysV aligned (RSP mod 16=8); IF=0 and DF=0",
    "Entry guard changes from 0 to 1; panic guard changes from 0 to 1",
    "CPU reaches exception_stop then continues to a hardware breakpoint at its next HLT; IF=0",
    "Serial blocks occur once per vector in order with matching vector/error/RIP and PANIC.COMPLETE",
    "ELF and every ESP file plus firmware templates retain their pre-run SHA-256 hashes",
]

# Executed inside GDB's Python runtime, not as a separate guest program.
GDB_DRIVER = r'''
import gdb, json, os, struct, time, traceback

config = json.load(open(os.environ['MS01_FALLBACK_CONFIG']))
results = open(config['cases_path'], 'w', buffering=1)
active_vector = None

def value(expr):
    return int(gdb.parse_and_eval(expr))

def require(condition, message):
    if not condition:
        raise AssertionError(message)

def expect(actual, expected, label):
    require(actual == expected, '%s: got %#x, expected %#x' % (label, actual, expected))

def stop_at(address):
    bp = gdb.Breakpoint('*%#x' % address, type=gdb.BP_HARDWARE_BREAKPOINT, internal=True)
    bp.silent = True
    try:
        gdb.execute('continue', to_string=True)
        expect(value('$rip'), address, 'CPU breakpoint RIP')
    finally:
        bp.delete()

def put_register(register, number):
    gdb.execute('set $%s = %#x' % (register, number), to_string=True)

try:
    gdb.execute('set architecture i386:x86-64', to_string=True)
    gdb.execute('file ' + json.dumps(config['elf']), to_string=True)
    gdb.execute('target remote 127.0.0.1:%d' % config['port'], to_string=True)
    probe = value('&vmx_probe')
    stop_at(probe)
    require(len(gdb.selected_inferior().threads()) == 1, 'Expected one BSP only')
    expect(value('$cs'), 8, 'checkpoint CS')
    expect(value('$ss'), 16, 'checkpoint SS')
    require(not value('$eflags') & 0x200, 'Checkpoint must have IF=0')
    require(not value('$eflags') & 0x400, 'Checkpoint must have DF=0')
    expect(value('*(unsigned char*)&exception_entered'), 0, 'initial entry guard')
    expect(value('panicking'), 0, 'initial panic guard')
    expect(value('sizeof(struct frame)'), 176, 'frame size')
    expect(value('sizeof(idt)'), 4096, 'IDT size')
    stack_base = value('&exception_stack')
    stack_size = value('sizeof(exception_stack)')
    stack_top = value('tss.ist[0]')
    expect(stack_top, stack_base + stack_size, 'IST1 top')
    expect(stack_top % 16, 0, 'IST1 alignment')
    require(stack_top != value('tss.ist[1]'), 'IST1 aliases IST2')
    require(stack_top != value('tss.ist[2]'), 'IST1 aliases IST3')
    original_rsp, original_flags = value('$rsp'), value('$eflags')
    original_cs, original_ss = value('$cs'), value('$ss')
    panic_entry = value('&exception_panic')
    halt_entry = value('&exception_stop')
    inferior = gdb.selected_inferior()
    original_probe = bytes(inferior.read_memory(probe, 2))
    registers = ['rax', 'rbx', 'rcx', 'rdx', 'rbp', 'rsi', 'rdi',
                 'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15']
    seen_targets = set()

    for index, vector in enumerate(config['vectors']):
        active_vector = vector
        started = time.monotonic()
        gate = gdb.parse_and_eval('idt[%d]' % vector)
        target = (int(gate['offset_low']) | (int(gate['offset_mid']) << 16)
                  | (int(gate['offset_high']) << 32))
        expect(target, value('&exception_%d' % vector), 'gate target')
        require(target not in seen_targets, 'Duplicate fallback gate target')
        seen_targets.add(target)
        for field, expected in [('selector', 8), ('ist', 1), ('flags', 0x8e), ('reserved', 0)]:
            expect(int(gate[field]), expected, 'gate ' + field)
        expect(value('tss.ist[0]'), stack_top, 'unchanged IST1')

        # Explicit test-only reset. No guest return from the fatal path occurs.
        # Long-mode IST delivery clears live SS; restore the checkpoint selector.
        gdb.execute('set {unsigned char}&exception_entered = 0', to_string=True)
        gdb.execute('set panicking = 0', to_string=True)
        expect(value('*(unsigned char*)&exception_entered'), 0, 'reset entry guard')
        expect(value('panicking'), 0, 'reset panic guard')
        injected = {name: 0x1100000000000000 + (vector << 24) + number * 0x010101
                    for number, name in enumerate(registers, 1)}
        for name, number in injected.items():
            put_register(name, number)
        put_register('rsp', original_rsp)
        put_register('rip', probe)
        put_register('eflags', original_flags)
        put_register('ss', original_ss)
        expect(value('$ss'), original_ss, 'reset SS')
        instruction = bytes([0xcd, vector])
        inferior.write_memory(probe, instruction)
        require(bytes(inferior.read_memory(probe, 2)) == instruction, 'INT bytes mismatch')
        stop_at(target)

        # This is the raw hardware-created IST frame, BEFORE the stub runs.
        entry_rsp = value('$rsp')
        expect(entry_rsp, stack_top - 40, 'raw CPU frame RSP')
        raw = list(struct.unpack('<5Q', bytes(inferior.read_memory(entry_rsp, 40))))
        expected_raw = [probe + 2, original_cs, original_flags, original_rsp, original_ss]
        require(raw == expected_raw, 'Raw CPU frame mismatch: %r != %r' % (raw, expected_raw))
        expect(value('*(unsigned char*)&exception_entered'), 0, 'pre-stub entry guard')
        expect(value('$ss'), 0, 'long-mode IST entry SS')
        require(not value('$eflags') & 0x200, 'Raw entry IF must be zero')
        for name, number in injected.items():
            expect(value('$' + name), number, 'raw entry ' + name)

        stop_at(panic_entry)
        frame_address = value('$rdi')
        expect(frame_address, stack_top - 176, 'normalized frame address')
        require(stack_base <= frame_address and frame_address + 176 <= stack_top,
                'Normalized frame not wholly inside exception stack')
        expect(value('$rsp'), (frame_address & ~15) - 8, 'C call stack pointer')
        expect(value('$rsp') % 16, 8, 'C entry stack alignment')
        require(not value('$eflags') & 0x600, 'C entry must have IF=0 and DF=0')
        frame = gdb.parse_and_eval('(struct frame*)$rdi').dereference()
        fields = {'vector': vector, 'error': 0, 'rip': probe + 2,
                  'cs': original_cs, 'rflags': original_flags,
                  'rsp': original_rsp, 'ss': original_ss, **injected}
        for name, number in fields.items():
            expect(int(frame[name]), number, 'normalized frame ' + name)
        expect(value('*(unsigned char*)&exception_entered'), 1, 'post-stub entry guard')
        expect(value('panicking'), 0, 'pre-panic C guard')

        stop_at(halt_entry)
        expect(value('panicking'), 1, 'post-panic C guard')
        require(stack_base <= value('$rsp') < stack_top, 'Halt stack not inside IST1')
        require(bytes(inferior.read_memory(halt_entry, 2)) == b'\xfa\xf4',
                'Expected CLI/HLT at exception_stop')
        # Continue from the observed CLI to a hardware breakpoint on HLT.
        # Some GDB/QEMU combinations report a single-step stop without advancing
        # RIP after a removed hardware breakpoint. Do not retry or assign RIP:
        # the CPU must execute the verified one-byte CLI to reach this stop.
        expect(value('$rip'), halt_entry, 'pre-CLI RIP')
        stop_at(halt_entry + 1)
        expect(value('$rip'), halt_entry + 1, 'post-CLI RIP')
        require(not value('$eflags') & 0x200, 'Post-CLI IF must be zero')
        require(bytes(inferior.read_memory(value('$rip'), 1)) == b'\xf4', 'Next instruction is not HLT')

        record = {'vector': vector, 'status': 'PASS', 'entry_rip': target,
                  'raw_frame_address': entry_rsp, 'raw_cpu_frame': raw,
                  'c_frame_address': frame_address, 'saved_rip': probe + 2,
                  'synthetic_error': int(frame['error']), 'saved_gpr_count': len(injected),
                  'gate': {'selector': 8, 'ist': 1, 'flags': 0x8e, 'reserved': 0},
                  'halt_entry_rip': halt_entry, 'post_cli_rip': value('$rip'),
                  'safe_halt': 'CPU continued from CLI to a hardware breakpoint on HLT; IF=0',
                  'test_reset_before_case': index != 0,
                  'elapsed_seconds': round(time.monotonic() - started, 4)}
        results.write(json.dumps(record, sort_keys=True) + '\n')
        print('FALLBACK_VECTOR_PASS %d' % vector, flush=True)

    # Restore the temporary code bytes. Never resume the kernel afterward.
    inferior.write_memory(probe, original_probe)
    require(bytes(inferior.read_memory(probe, 2)) == original_probe, 'Probe restoration failed')
    print('FALLBACK_ALL_REQUESTED_PASS %d' % len(config['vectors']), flush=True)
except BaseException as error:
    results.write(json.dumps({'vector': active_vector, 'status': 'FAIL',
                              'error': str(error), 'traceback': traceback.format_exc()}) + '\n')
    traceback.print_exc()
    raise
finally:
    results.close()
    # Unlike detach, disconnect does not issue a guest resume packet.
    try:
        gdb.execute('disconnect', to_string=True)
    except gdb.error:
        pass
'''


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def hashes(directory):
    return {str(path.relative_to(directory)): sha256(path)
            for path in sorted(directory.rglob('*')) if path.is_file()}


@contextlib.contextmanager
def lock(path, timeout=120):
    with path.open('a') as stream:
        deadline = time.monotonic() + timeout
        while True:
            try:
                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    raise TimeoutError('Timed out acquiring ' + str(path))
                time.sleep(0.1)
        try:
            yield
        finally:
            fcntl.flock(stream, fcntl.LOCK_UN)


def terminate(process):
    if process is None or process.poll() is not None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        process.wait(timeout=5)
        return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait(timeout=5)


def parse_vectors(text):
    try:
        values = [int(value) for value in text.split(',')]
    except ValueError as error:
        raise argparse.ArgumentTypeError('Use comma-separated decimal vectors') from error
    if not values or len(set(values)) != len(values) or any(value not in ALL_VECTORS for value in values):
        raise argparse.ArgumentTypeError('Vectors must be unique integers from 32 through 255')
    return values


def run(args):
    out = ROOT / 'build/ms01-fallback-vectors'
    out.mkdir(parents=True, exist_ok=True)
    with lock(out / '.runner.lock'):
        # A failed/aborted run must never leave an older successful report current.
        report_path = out / 'report.json'
        report_path.unlink(missing_ok=True)
        run_dir = Path(tempfile.mkdtemp(prefix='run-', dir=out))
        images = run_dir / 'images'
        images.mkdir()
        report = {
            'schema': 'ms01-fallback-vectors-v1', 'status': 'RUNNING',
            'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'requested_vectors': args.vectors, 'requested_count': len(args.vectors),
            'full_32_255_run': args.vectors == ALL_VECTORS,
            'method': 'Real CPU software INT with debugger-directed code/state injection',
            'machine': {'accelerator': 'tcg', 'cpus': 1, 'cpu': 'max', 'gdb_port': PORT},
            'assertions': ASSERTIONS, 'limits': LIMITS,
            'run_directory': str(run_dir), 'source_sha256': sha256(Path(__file__)),
            'timeouts_seconds': {'build_lock': 120, 'make': 180, 'gdb_total': args.timeout},
            'cases': [], 'failures': [],
        }
        builder = qemu = debugger = None
        started = time.monotonic()
        try:
            with lock(ROOT / 'build/.ms01-build.lock'):
                with (run_dir / 'build.log').open('w') as log:
                    builder = subprocess.Popen(['make', 'all'], cwd=ROOT, stdout=log,
                                               stderr=subprocess.STDOUT, start_new_session=True)
                    try:
                        build_returncode = builder.wait(timeout=180)
                        if build_returncode:
                            raise RuntimeError('make all failed with exit %d; see build.log' % build_returncode)
                    finally:
                        terminate(builder)
                shutil.copytree(ROOT / 'build/esp', images / 'esp')
                shutil.copyfile(ROOT / 'build/krnl.elf', images / 'krnl.elf')
            if sha256(images / 'krnl.elf') != sha256(images / 'esp/syle/krnl.elf'):
                raise AssertionError('Debug ELF does not match the snapshotted ESP kernel')
            for name, default in [('OVMF_CODE', '/usr/share/OVMF/OVMF_CODE_4M.fd'),
                                  ('OVMF_VARS', '/usr/share/OVMF/OVMF_VARS_4M.fd')]:
                shutil.copyfile(Path(os.environ.get(name, default)), images / (name + '.fd'))
            before = hashes(images)
            report['image_sha256_before'] = before
            (run_dir / 'image-manifest.json').write_text(json.dumps(before, indent=2) + '\n')
            # Prevent connecting to an unrelated existing debugger on the fixed port.
            with socket.socket() as listener:
                listener.bind(('127.0.0.1', PORT))
            home, temp = run_dir / 'home', run_dir / 'tmp'
            home.mkdir()
            temp.mkdir()
            env = dict(os.environ, ESP_DIR=str(images / 'esp'),
                       OVMF_CODE=str(images / 'OVMF_CODE.fd'),
                       OVMF_VARS=str(images / 'OVMF_VARS.fd'),
                       OVMF_VARS_RUNTIME=str(run_dir / 'vars-runtime.fd'),
                       QEMU_GDB_PORT=str(PORT), QEMU_CPUS='1', QEMU_CPU='max',
                       QEMU_MEMORY='256M', DISPLAY_BACKEND='none', NO_UART='0', NO_GOP='0',
                       HOME=str(home), TMPDIR=str(temp))
            env.pop('QMP_PORT', None)
            cases_path = run_dir / 'cases.jsonl'
            config = {'elf': str(images / 'krnl.elf'), 'port': PORT,
                      'vectors': args.vectors, 'cases_path': str(cases_path)}
            config_path = run_dir / 'config.json'
            config_path.write_text(json.dumps(config, indent=2) + '\n')
            env['MS01_FALLBACK_CONFIG'] = str(config_path)
            driver = run_dir / 'driver.py'
            driver.write_text(GDB_DRIVER)
            commands = run_dir / 'run.gdb'
            commands.write_text('set pagination off\nset confirm off\nset remotetimeout 15\n'
                                'set print thread-events off\nsource ' + str(driver) + '\nquit\n')
            serial_path, gdb_path = run_dir / 'serial.log', run_dir / 'gdb.log'
            with serial_path.open('w') as serial, gdb_path.open('w') as gdb_log:
                qemu = subprocess.Popen(['scripts/run.sh', 'debug'], cwd=ROOT, env=env,
                                        stdout=serial, stderr=subprocess.STDOUT, start_new_session=True)
                time.sleep(0.5)
                if qemu.poll() is not None:
                    raise RuntimeError('QEMU exited before GDB launch; see serial.log')
                debugger = subprocess.Popen([os.environ.get('GDB', 'gdb'), '-nx', '-nh', '-q', '-batch',
                                             '-x', str(commands)], cwd=ROOT, env=env,
                                            stdout=gdb_log, stderr=subprocess.STDOUT, start_new_session=True)
                returncode = debugger.wait(timeout=args.timeout)
                report['gdb_returncode'] = returncode
                if returncode != 0:
                    raise RuntimeError('GDB failed with exit %d; see gdb.log and cases.jsonl' % returncode)
            if 'FALLBACK_ALL_REQUESTED_PASS %d' % len(args.vectors) not in gdb_path.read_text():
                raise AssertionError('GDB completion marker missing')
            # QEMU is stopped at the last halt path; finish it before reading final logs/hashes.
            terminate(qemu)
            records = [json.loads(line) for line in cases_path.read_text().splitlines()]
            if [case['vector'] for case in records] != args.vectors:
                raise AssertionError('Case sequence/count mismatch')
            if any(case['status'] != 'PASS' for case in records):
                raise AssertionError('A debugger assertion failed')
            serial_text = serial_path.read_text(errors='replace')
            blocks = re.findall(r'UNHANDLED VECTOR \(SAFE FALLBACK\)\s*'
                                r'(.*?)PANIC\.COMPLETE', serial_text, re.DOTALL)
            if len(blocks) != len(args.vectors):
                raise AssertionError('Expected %d serial panic blocks, got %d' % (len(args.vectors), len(blocks)))
            if serial_text.count('PANIC.COMPLETE') != len(args.vectors):
                raise AssertionError('Unexpected extra/missing serial PANIC.COMPLETE marker')
            for case, block in zip(records, blocks):
                for field, number in [('vector', case['vector']), ('error', 0), ('rip', case['saved_rip'])]:
                    match = re.search(r'\b' + field + r'=0x([0-9a-fA-F]{16})\b', block)
                    if match is None or int(match.group(1), 16) != number:
                        raise AssertionError('Serial %s mismatch for vector %d' % (field, case['vector']))
                case['serial_verified'] = True
            report['cases'] = records
            report['status'] = 'PASS'
        except BaseException as error:
            report['status'] = 'FAIL'
            report['failures'].append(type(error).__name__ + ': ' + str(error))
        finally:
            terminate(builder)
            terminate(debugger)
            terminate(qemu)
            report['owned_processes_reaped'] = all(proc is None or proc.poll() is not None
                                                  for proc in (builder, debugger, qemu))
            if 'image_sha256_before' in report:
                report['image_sha256_after'] = hashes(images)
                if report['image_sha256_after'] != report['image_sha256_before']:
                    report['status'] = 'FAIL'
                    report['failures'].append('Image hashes changed during execution')
            partial = run_dir / 'cases.jsonl'
            if partial.exists() and not report['cases']:
                for line in partial.read_text().splitlines():
                    try:
                        report['cases'].append(json.loads(line))
                    except json.JSONDecodeError:
                        report['failures'].append('Incomplete case record')
            report['passed_count'] = sum(case['status'] == 'PASS' for case in report['cases'])
            report['failed_case_count'] = sum(case['status'] != 'PASS' for case in report['cases'])
            report['unexecuted_count'] = len(args.vectors) - len({case['vector'] for case in report['cases']
                                                               if case.get('vector') in args.vectors})
            report['elapsed_seconds'] = round(time.monotonic() - started, 3)
            report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            encoded = json.dumps(report, indent=2) + '\n'
            (run_dir / 'report.json').write_text(encoded)
            temporary = out / 'report.json.tmp'
            temporary.write_text(encoded)
            temporary.replace(report_path)
        print('%s %d/%d fallback software-INT cases; %s' %
              (report['status'], report['passed_count'], len(args.vectors), report_path), flush=True)
        for failure in report['failures']:
            print('FAIL:', failure, flush=True)
        return 0 if report['status'] == 'PASS' else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--vectors', type=parse_vectors, default=ALL_VECTORS,
                        help='Focused subset of decimal vectors; default is every vector 32..255')
    parser.add_argument('--timeout', type=int, default=600,
                        help='Total GDB/guest execution timeout in seconds, from 10 to 1800 (default 600)')
    args = parser.parse_args()
    if not 10 <= args.timeout <= 1800:
        parser.error('--timeout must be between 10 and 1800 seconds')
    def interrupted(signum, _frame):
        raise InterruptedError('Runner interrupted by signal %d' % signum)
    signal.signal(signal.SIGTERM, interrupted)
    return run(args)


if __name__ == '__main__':
    raise SystemExit(main())
