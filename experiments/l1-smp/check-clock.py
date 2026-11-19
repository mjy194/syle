"""Verify the unmodified SMP disk over the serial socket of an m2/run.py guest."""
import argparse
import json
from pathlib import Path
import re
import secrets
import socket
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'm2'))
from check_manual import Console


def check_l2_busybox(console, clock_observe=0):
    prompt = b'L2 busybox # '
    console.expect(b'L2 | SYLE_LINUX_USERSPACE_READY', timeout=180)
    console.expect(prompt)
    console.send(b'uname -s; cat /sys/devices/system/cpu/online\r')
    console.expect(b'\nLinux\r')
    console.expect(b'\n0-1\r')
    console.expect(prompt)
    token = secrets.token_hex(6).encode()
    console.send(b"printf 'L2_BUSYBOX_%s\\n' " + token + b'\r')
    console.expect(b'\nL2_BUSYBOX_' + token + b'\r')
    console.expect(prompt)
    if clock_observe:
        console.send(f'l2-clock 1 {clock_observe}\r'.encode())
        for phase in range(1, 4):
            console.expect(f'L2_CLOCK_SOURCE phase={phase} source=tsc\r'.encode(),
                           timeout=90 + 2 * clock_observe if phase == 1 else 30)
        console.expect(prompt)
    console.send(b'halt -f\r')
    console.expect(b'L2_SMP_SHUTDOWN bsp_halted=1 ap_mp_state=3', timeout=180)


def check_l2_clock(data, path, tick_hz=None):
    if not re.search(rb'^L2 \| [^\r\n]*clocksource: hpet: mask:', data, re.MULTILINE):
        raise SystemExit(f'L2 did not register its HPET watchdog reference in {path}')
    # The host reference counter has fixed 100 ns units. The tag associates
    # the host read with the actual RDTSC reported by the pinned L2 process.
    host_rows = re.findall(rb'L2\.CLOCK index=(0x[0-9a-f]+)\s+owner=(0x[0-9a-f]+)'
                           rb'\s+host_ref=(0x[0-9a-f]+)\s+host_exit_tsc=(0x[0-9a-f]+)'
                           rb'\s+offset=(0x[0-9a-f]+)\s+guest_tsc=(0x[0-9a-f]+)', data)
    guest_rows = re.findall(rb'L2_CLOCK_TSC index=(\d+) cpu=(\d+) tsc=(\d+)', data)
    irq_rows = re.findall(rb'L2_CLOCK_IRQ phase=(\d+) loc0=(\d+) loc1=(\d+)'
                          rb' irq0cpu0=(\d+) irq0cpu1=(\d+)', data)
    host = [tuple(int(field, 16) for field in row) for row in host_rows]
    guest = [tuple(map(int, row)) for row in guest_rows]
    irqs = [tuple(map(int, row)) for row in irq_rows]
    timers = {}
    for match in re.finditer(rb'^L2\.TIMER index=(0x[0-9a-f]+)((?:\s+\w+=0x[0-9a-f]+)+)',
                             data, re.MULTILINE):
        timers[int(match[1], 16)] = {
            name: int(value, 16)
            for name, value in re.findall(rb'(\w+)=(0x[0-9a-f]+)', match[2])
        }
    expected = list(range(8, 32))
    if ([row[0] for row in host] != expected or [row[0] for row in guest] != expected
            or [row[0] for row in irqs] != [1, 2, 3]):
        raise SystemExit(f'Missing or repeated L2 clock probe samples in {path}')
    if any((index, cpu, tsc) != (i, owner, value) or cpu != index % 2
           for (index, cpu, tsc), (i, owner, _, _, _, value) in zip(guest, host)):
        raise SystemExit(f'L2 and host samples do not match in {path}')
    if any(after[2] <= before[2] for before, after in zip(host, host[1:])):
        raise SystemExit(f'Host reference counter did not advance in {path}')
    biases = [row[5] - row[2] for row in host]
    spread = max(biases) - min(biases)
    if spread > 10_000:
        raise SystemExit(f'L2 TSC differs across samples or CPUs by {spread / 10:.1f} us in {path}')
    print(f'{path.parent.name}: L2 TSC max host-adjusted spread {spread / 10:.1f} us')
    tick_short = []
    for phase in range(2):
        for cpu in range(2):
            start, end = host[phase * 8 + cpu], host[(phase + 1) * 8 + cpu]
            elapsed = end[2] - start[2]
            ticks = end[5] - start[5]
            loc = irqs[phase + 1][1 + cpu] - irqs[phase][1 + cpu]
            pit = irqs[phase + 1][3 + cpu] - irqs[phase][3 + cpu]
            if elapsed < 10_000_000 or end[3] <= start[3] or loc <= 0 or pit < 0:
                raise SystemExit(f'Invalid L2 clock or timer sample duration in {path}')
            rate = loc * 1e7 / elapsed
            if tick_hz and abs(rate - tick_hz) > tick_hz * 0.05:
                tick_short.append(f'window {phase + 1} CPU{cpu} {rate:.1f}/s')
            print(f'  CPU{cpu}: {elapsed / 1e7:.3f}s host, TSC delta {ticks}, '
                  f'LOC +{loc} ({rate:.1f}/s), IRQ0 +{pit}')
            if timers:
                if start[0] not in timers or end[0] not in timers:
                    raise SystemExit(f'Incomplete layered timer trace in {path}')
                before, after = timers[start[0]], timers[end[0]]
                def delta(name):
                    return after[name] - before[name]
                expired = delta(b'l1_expired')
                batched = delta(b'l1_batched')
                late_us = delta(b'l1_late_ticks') / max(1, expired - batched) / 10
                vmx = after.get(b'l1_vmx_expired', 0) - before.get(b'l1_vmx_expired', 0)
                print(f"    L1 expiry +{expired}, processed +{delta(b'l1_accept') + delta(b'l1_reflect')}, "
                      f"batched +{batched}, observed late {late_us:.1f}us avg; "
                      f"L1 VMX expiry +{vmx}; L2 submit +{delta(b'l2_submit')}, deliver +{delta(b'l2_deliver')}")
    if tick_short:
        raise SystemExit(f'L2 LAPIC counts outside 5% of {tick_hz} Hz: {", ".join(tick_short)}')
    if re.search(rb'timekeeping watchdog[^\r\n]*unstable|Marking TSC unstable', data):
        raise SystemExit(f'L2 clocksource watchdog rejected TSC in {path}')


def check_interleaved(data, path):
    first_exit = data.find(b'SMP_L2_EXIT ')
    if first_exit < 0 or any(
        f'SMP_L2_READY cpu={cpu} api=12 vmx=1'.encode() not in data[:first_exit]
        for cpu in range(2)
    ):
        raise SystemExit(f'Both KVM workers must be ready before the first L2 exit in {path}')

    expected = []
    r8 = [0x1234567800000040, 0x9876543200000060]
    mask = (1 << 64) - 1
    memory = [~(value ^ 0x2a98dde414f031da) & mask for value in r8]
    for step in range(9):
        for cpu in range(2):
            if step < 8:
                memory[cpu] = (memory[cpu] + r8[cpu]) & mask
                r8[cpu] += 1
            reason = 'IO' if step < 8 else 'HLT'
            expected.append(
                f'SMP_L2_EXIT cpu={cpu} round={step} exit={reason} '
                f'r8={r8[cpu]:016x} mem={memory[cpu]:016x}'.encode()
            )
    actual = re.findall(rb'SMP_L2_EXIT [^\r\n]+', data)
    if sorted(actual[:2]) != expected[:2] or actual[2:] != expected[2:]:
        raise SystemExit(f'L2 exit order or register/memory state mismatch in {path}')
    if b'SMP_L2_INTERLEAVE_PASS' not in data:
        raise SystemExit(f'Missing L2 interleave completion in {path}')


def check_migration(data, path):
    entries = re.findall(rb'SMP_L2_MIGRATE_(?:ENTER|EXIT) [^\r\n]+', data)
    fd = re.search(rb'fd=([0-9]+)', entries[0]) if entries else None
    if fd is None:
        raise SystemExit(f'Missing migration entries in {path}')

    expected = []
    r8 = 0x1234567800000040
    sentinel = r8 ^ 0x2a98dde414f031da
    mask = (1 << 64) - 1
    memory = ~sentinel & mask
    for step, cpu in enumerate((0, 1, 0)):
        if step < 2:
            memory = (memory + r8) & mask
            r8 += 1
        suffix = f'step={step} cpu={cpu} fd={fd[1].decode()}'
        expected.append(f'SMP_L2_MIGRATE_ENTER {suffix}'.encode())
        reason, rip = ('IO', 0x19) if step < 2 else ('HLT', 0x1e)
        expected.append((
            f'SMP_L2_MIGRATE_EXIT {suffix} exit={reason} rip={rip:x} rcx={2 - step:x} '
            f'r8={r8:016x} r9={memory:016x} r10={sentinel:016x} '
            f'mem={memory:016x} sentinel={sentinel:016x}'
        ).encode())
    if entries != expected or b'SMP_L2_MIGRATE_PASS' not in data:
        raise SystemExit(f'L2 migration order, file descriptor or state mismatch in {path}')


def check_host_parallel(data, directory):
    affinity = json.loads((directory / 'affinity.json').read_text())
    if ([cpu['syle_cpu'] for cpu in affinity] != [0, 1] or
            len({(cpu['package'], cpu['core']) for cpu in affinity}) != 2):
        raise SystemExit(f'The QEMU vCPU threads did not use separate physical cores in {directory}')
    owner = re.search(rb'FMG.HOST_AP_OWNER vmcs=0x([0-9a-f]+)', data)
    create = re.search(rb'FMG.CREATE generation=0x[0-9a-f]+\s+vmcs=0x([0-9a-f]+)', data)
    if not owner or not create or owner[1] == create[1]:
        raise SystemExit(f'Missing independent VMCS01 owners in {directory}')
    _, marker, summary = data.rpartition(b'FMG.HOST_PARALLEL ')
    summary = summary.split(b'FMG.RELEASE', 1)[0]
    values = {key.decode(): int(value, 16) for key, value in re.findall(rb'(\w+)=0x([0-9a-f]+)', summary)}
    counts = ('ap_entries', 'l1_overlap_pairs', 'l2_overlap_pairs', 'l2_overlap_tsc',
              'cross_ipis', 'shared_ept_ack', 'bsp_l2_invept', 'ap_l2_invept')
    if (not marker or any(values.get(key, 0) <= 0 for key in counts)
            or values.get('host_ipi_ack', 0) < values['shared_ept_ack']
            or any(values.get(key, -1) != 0 for key in ('ap_vmx_active', 'ap_vmxon', 'ap_vmcs'))
            or b'L2_SMP_WORK_PASS workers=2' not in data
            or b'FMG.RESUME same_guest=1' not in data):
        raise SystemExit(f'Incomplete host parallelism or AP cleanup evidence in {directory}: {values}')
    print(f"{directory.name}: VM-entry overlaps L1={values['l1_overlap_pairs']} "
          f"L2={values['l2_overlap_pairs']}, shared EPT acknowledgements={values['shared_ept_ack']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--observe', type=int, default=15,
                        help='seconds to observe after the affinity probe passes')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--nested', action='store_true', help='check the sequential L2 probes')
    mode.add_argument('--concurrent', action='store_true', help='check the interleaved L2 probes')
    mode.add_argument('--migrate', action='store_true', help='check one L2 vCPU migrating 0→1→0')
    mode.add_argument('--l2-smp', action='store_true', help='check Linux in a single L2 with two vCPUs')
    parser.add_argument('--host-parallel', action='store_true',
                        help='require distinct physical cores, hardware overlap and AP teardown')
    parser.add_argument('--l2-clock', type=int, default=0, metavar='SECONDS',
                        help='check three L2 TSC snapshots spaced by SECONDS; requires --trace-l2-clock build')
    parser.add_argument('--l2-tick-hz', type=int, metavar='HZ',
                        help='require both L2 LAPIC interrupt rates to be within 5%% of HZ')
    args = parser.parse_args()
    if args.host_parallel and not args.l2_smp:
        parser.error('--host-parallel requires --l2-smp')
    if args.l2_clock and (not args.l2_smp or not 1 <= args.l2_clock <= 300):
        parser.error('--l2-clock requires --l2-smp and an interval of 1–300 seconds')
    if args.l2_tick_hz is not None and (not args.l2_clock or args.l2_tick_hz < 1):
        parser.error('--l2-tick-hz requires --l2-clock and a positive rate')
    if args.observe < 1:
        parser.error('--observe must be positive')
    directory = args.run_directory.resolve()
    with socket.socket(socket.AF_UNIX) as stream:
        deadline = time.monotonic() + 30
        while True:
            try:
                stream.connect(str(directory / 'serial.sock'))
                break
            except (FileNotFoundError, ConnectionRefusedError):
                if time.monotonic() >= deadline:
                    raise
                time.sleep(0.1)
        stream.settimeout(0.5)
        path = directory / 'serial.log'
        with path.open('xb', buffering=0) as log:
            console = Console(stream, log)
            console.expect(b'[q]uit')
            nested = args.nested or args.concurrent or args.migrate or args.l2_smp
            for key, marker in [(b'n' if nested else b'c', b'M2 ready'), (b's', b'M2 running')]:
                console.send(key)
                console.expect(marker, 120)
            console.send(b'a')
            if args.l2_smp:
                check_l2_busybox(console, args.l2_clock)
                console.expect(b'L2_SMP_RUNNER_DONE rc=0')
            else:
                console.expect(b'SMP_DUAL_PASS', timeout=120)
            deadline = time.monotonic() + args.observe
            while time.monotonic() < deadline:
                console.receive()
            console.detach()
            console.send(b'p')
            console.expect(b'M2 paused')
            if args.host_parallel:
                console.send(b's')
                console.expect(b'M2 running')
                console.send(b'p')
                console.expect(b'M2 paused')
            console.send(b'q')
            console.expect(b'M2 closed')
    data = path.read_bytes()
    if args.l2_clock:
        check_l2_clock(data, path, args.l2_tick_hz)
    if args.host_parallel:
        check_host_parallel(data, directory)
    l1 = b'\n'.join(line for line in data.splitlines() if not line.startswith(b'L2 | '))
    sources = re.findall(rb'\bSwitched to clocksource ([\w-]+)', l1)
    if b'clocksource: hpet: mask:' not in data:
        raise SystemExit(f'Guest did not register the HPET clocksource in {path}')
    markers = (b'SMP_ONLINE=0-1', b'SMP_PIN requested=0 actual=0', b'SMP_PIN requested=1 actual=1')
    if args.concurrent:
        check_interleaved(data, path)
    if args.migrate:
        check_migration(data, path)
    if args.l2_smp:
        markers = (b'L2_BSP_WORKER online l1_cpu=0', b'L2_AP_WORKER online l1_cpu=1',
                   b'L2_SMP_PIN actual=0', b'L2_SMP_PIN actual=1',
                   b'L2_SMP_USERSPACE_PASS', b'L2_SMP_SHUTDOWN bsp_halted=1 ap_mp_state=3',
                   b'L2_SMP_RUNNER_DONE rc=0')
    if args.concurrent or args.migrate or args.l2_smp:
        release = data.rpartition(b'FMG.RELEASE generation=')[2].split(b'M2 closed', 1)[0]
        fields = dict(re.findall(rb'(\w+)=0x([0-9a-f]+)', release))
        if (not fields.get(b'pages_baseline') or fields[b'pages_baseline'] != fields.get(b'pages_after')
                or any(int(fields.get(key, b'1'), 16) for key in (b'vmx_active', b'vmxon', b'vmcs'))
                or any(int(fields.get(key, b'0'), 16) != 1 for key in (b'ept_released', b'adapter_revoked'))):
            raise SystemExit(f'Guest resources did not return to baseline in {path}')
        print(f'{directory.name}: L2 exit state and resource release passed', flush=True)
    if args.nested:
        for cpu in range(2):
            begin = f'SMP_PIN requested={cpu} actual={cpu}'.encode()
            end = f'SMP_KVM_L2_RESUME_PASS cpu={cpu}'.encode()
            _, start, tail = data.partition(begin)
            probe, finish, _ = tail.partition(end)
            expected = [b'vmx=1', b'KVM_GET_API_VERSION=12', b'KVM_RUN 1 exit=2', b'L2 OUT A',
                        b'KVM_RUN 2 exit=2', b'L2 OUT B', b'KVM_RUN 3 exit=5',
                        b'L2_64 RIP=18 R8=1234567800000042 RAX=1234567800000042', b'KVM_L2_64_RESUME_PASS']
            if not start or not finish:
                raise SystemExit(f'Missing L2 result on CPU {cpu} in {path}')
            for marker in expected:
                _, found, probe = probe.partition(marker)
                if not found:
                    raise SystemExit(f'Missing {marker!r} on CPU {cpu} in {path}')
    if not all(marker in data for marker in markers):
        raise SystemExit(f'Missing CPU affinity evidence in {path}')
    if (b'tsc' not in sources or any(name != b'tsc' for name in sources[sources.index(b'tsc'):])
            or b'Marking TSC unstable' in l1):
        raise SystemExit(f'Clocksource check failed in {path}: {sources!r}')
    result = 'dual-vCPU L2 BusyBox round trip and halt' if args.l2_smp else (
        'L2 migration' if args.migrate else 'interleaved L2 state' if args.concurrent else 'both affinities')
    print(f'{directory.name}: tsc, CPUs 0-1, {result} passed, {args.observe}s observed')


if __name__ == '__main__':
    main()
