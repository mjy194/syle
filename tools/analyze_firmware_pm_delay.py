#!/usr/bin/env python3
"""Read-only analysis of the pinned OVMF Metronome PM-timer wait loop."""
import argparse
import hashlib
import json
import lzma
from pathlib import Path
import re
import struct
import uuid

CODE_SHA = '624e06de18b4fa535e90db7160d00d3d07d206422b89999bf1e27d920264e4e0'
MODULE_SHA = 'e84dbfdb21c113a1bf4420ff12179f1a1d312b2de3a833e828b92472c388aba9'
SHELL_SHA = '31b63d7ea99f12bfb833ee9ab4a8e32bf9cb0280b454cdd326280f54d1671cca'
HZ = 3579545
MASK = 0xffffff


def fields(text):
    return {k: int(v, 16) for k, v in re.findall(r'(\w+)=(0x[0-9a-f]+)', text)}


def verify_binary(path):
    code = Path(path).read_bytes()
    if hashlib.sha256(code).hexdigest() != CODE_SHA:
        raise ValueError('unrecognized firmware input')
    guid = uuid.UUID('ee4e5898-3914-4259-9d6e-dc7bd79403cf').bytes_le
    at = code.index(guid)
    start = at - 4
    size = int.from_bytes(code[start:start + 3], 'little')
    offset = struct.unpack_from('<H', code, at + 16)[0]
    raw = lzma.decompress(code[start + offset:start + size], format=lzma.FORMAT_ALONE)
    module = raw[0x1566bc:0x1566bc + 2560]
    if hashlib.sha256(module).hexdigest() != MODULE_SHA:
        raise ValueError('unrecognized Metronome module')
    assert module[0x31b] == 0xed and module[0x36e:0x370] == b'\xf3\x90'
    # MOV EAX,EBX; SUB EAX,EDX; BT EAX,23; JB done; PAUSE.
    assert module[0x364:0x370] == bytes.fromhex('89d829d00fbae0177204f390')
    assert 'Metronome'.encode('utf-16-le') in raw[0x1570bc:0x1570ec]
    shell = raw[0x486f14:0x486f14 + 0xda580]
    if hashlib.sha256(shell).hexdigest() != SHELL_SHA:
        raise ValueError('unrecognized Shell module')
    assert shell[0x1e973:0x1e97e] == bytes.fromhex('b940420f00ff90f8000000')
    assert int.from_bytes(shell[0x930f:0x9313], 'little') == 5
    return {'code_sha256': CODE_SHA, 'module_sha256': MODULE_SHA,
            'expanded_module_offset': 0x1566bc, 'inferred_runtime_base': 0x70c2000,
            'read_rva': 0x31b, 'pause_rva': 0x36e, 'shell_sha256': SHELL_SHA,
            'shell_inferred_base': 0x683d000, 'shell_stall_return_rip': 0x685b97e,
            'shell_default_delay_seconds': 5}


def analyze_serial(path):
    data = Path(path).read_bytes()
    text = data.decode('ascii', errors='strict')
    reads, pauses, targets = [], [], []
    last_read = None
    for block in text.split('OVMF.CLOCK_APIC exit reason=')[1:]:
        head = block.split('OVMF.', 1)[0]
        v = fields(head)
        reason = int(block.split()[0], 16)
        if reason == 30 and v.get('rip') == 0x70c231b and v.get('qual') == 0xb008000b:
            io = block.split('OVMF.CLOCK_APIC pci_cycle ', 1)
            if len(io) != 2:
                raise ValueError('missing PM read result (sampled logs need snapshot analysis)')
            result = fields(io[1].split('OVMF.', 1)[0])
            if result.get('accepted') != 1 or result.get('port') != 0xb008:
                raise ValueError('unaccepted PM read')
            last_read = result['after_rax']
            if last_read > MASK:
                raise ValueError('PM sample exceeds 24 bits')
            reads.append(last_read)
        elif reason == 40 and v.get('rip') == 0x70c236e:
            if last_read is None or v['rax'] > 0xffffffff or (v['rax'] & MASK) > 0x3fffff:
                raise ValueError('missing read or unexpected delay difference')
            commit = block.split('OVMF.PAUSE ', 1)
            if len(commit) != 2:
                raise ValueError('missing PAUSE completion')
            result = fields(commit[1].split('unchanged_clock_and_exit_budgets', 1)[0])
            if any(result.get(k) != val for k, val in
                   [('result', 0), ('terminal', 0), ('length', 2), ('byte0', 0xf3), ('byte1', 0x90)]):
                raise ValueError('PAUSE did not complete')
            pauses.append(v['rax'])
            targets.append((last_read + v['rax']) & MASK)
    if not pauses or len(set(targets)) != 1:
        raise ValueError('no single stable wait target')
    steps = [(b - a) & MASK for a, b in zip(reads, reads[1:])]
    if any(x >= 1 << 23 for x in steps):
        raise ValueError('ambiguous backwards/wrapped samples')
    target = targets[0]
    request = (target - reads[0]) & MASK
    progress = sum(steps)
    remaining = (target - reads[-1]) & MASK
    if request >= 1 << 22 or request != progress + remaining:
        raise ValueError('samples do not describe one bounded partial chunk')
    return {'serial_sha256': hashlib.sha256(data).hexdigest(),
            'reads': len(reads), 'pause_commits': len(pauses),
            'first_tick': reads[0], 'last_tick': reads[-1], 'target_tick': target,
            'requested_chunk_ticks': request, 'requested_chunk_seconds': request / HZ,
            'progress_ticks': progress, 'progress_seconds': progress / HZ,
            'remaining_ticks': remaining, 'remaining_seconds': remaining / HZ,
            'min_step_ticks': min(steps), 'max_step_ticks': max(steps),
            'mean_step_ticks': progress / len(steps),
            'total_request_note': 'R12 remaining chunk count is not in this historical log; capture it before claiming total duration.'}


def analyze_sampled_serial(path):
    """Analyze the isolated observer's explicit snapshots, never infer lost reads."""
    data = Path(path).read_bytes()
    text = data.decode('ascii', errors='strict')

    def records(marker):
        return [fields(b.split('OVMF.', 1)[0]) for b in text.split(marker)[1:]]

    snapshots = records('OVMF.PM_DELAY snapshot ')
    terminal = records('OVMF.PM_DELAY terminal ')
    clock = records('OVMF.RTC_OBSERVE phase=terminal_no_new_sample ')
    crossed = records('OVMF.PM_DELAY target_crossed ')
    if not snapshots or len(terminal) != 1 or len(clock) != 1:
        raise ValueError('missing or ambiguous diagnostic records')
    groups = []
    for snapshot in snapshots:
        if not groups or snapshot['target_ebx'] != groups[-1][0]['target_ebx']:
            groups.append([])
        groups[-1].append(snapshot)
    targets = [g[0]['target_ebx'] for g in groups]
    if len(set(targets)) != len(targets) or any(c['target'] not in targets for c in crossed):
        raise ValueError('ambiguous repeated or unknown target')
    waits = []
    for index, group in enumerate(groups):
        first = group[0]
        request = first['request_edi']
        # Longer requests require accounting for successive 2^22-tick chunks.
        if not 0 < request < 1 << 22 or any(
                s['chunks_r12'] != 0 or s['request_edi'] != request for s in group):
            raise ValueError('not an unsegmented request')
        crossing = [c for c in crossed if c['target'] == first['target_ebx']]
        wait = {'target': first['target_ebx'], 'requested_ticks': request,
                'requested_seconds': request / HZ, 'snapshots': group,
                'next_request_observed': index + 1 < len(groups)}
        if crossing:
            if len(crossing) != 1:
                raise ValueError('ambiguous target crossing')
            overshoot = (crossing[0]['returned'] - first['target_ebx']) & MASK
            if not 0 < overshoot < 1 << 23:
                raise ValueError('read did not satisfy the guest exit condition')
            wait.update(progress_ticks=request + overshoot,
                        progress_seconds=(request + overshoot) / HZ,
                        overshoot_ticks=overshoot, remaining_ticks=0,
                        crossing=crossing[0])
        else:
            if index + 1 < len(groups):
                raise ValueError('target changed without a crossing record')
            stopped = records('OVMF.CLOCK_APIC failed reason=')
            if len(stopped) != 1 or stopped[0].get('rip') != 0x70c231c:
                raise ValueError('terminal clock is not the last accepted loop IN')
            remaining = (first['target_ebx'] - clock[0]['pm_ticks']) & MASK
            if remaining > request:
                raise ValueError('terminal count is outside the request')
            progress = request - remaining
            wait.update(progress_ticks=progress, progress_seconds=progress / HZ,
                        remaining_ticks=remaining, remaining_seconds=remaining / HZ)
            pauses = terminal[0]['pauses'] - first['pauses'] + 1
            if progress and pauses:
                wait['estimated_extra_exits_at_observed_rate'] = 2 * remaining * pauses / progress
        waits.append(wait)
    return {'serial_sha256': hashlib.sha256(data).hexdigest(), 'waits': waits,
            'completed_timer_conditions': len(crossed), 'terminal': terminal[0],
            'shell_countdown': records('OVMF.PM_DELAY shell_countdown '),
            'terminal_pm_ticks': clock[0]['pm_ticks']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--code', required=True)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--sampled', action='store_true', help='Use explicit PM_DELAY snapshots')
    args = parser.parse_args()
    analyzer = analyze_sampled_serial if args.sampled else analyze_serial
    print(json.dumps({'binary': verify_binary(args.code), 'progress': analyzer(args.serial)}, indent=2))


if __name__ == '__main__':
    main()
