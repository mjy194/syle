#!/usr/bin/env python3
"""Verify the ordered HLT/idle/IRQ/EOI evidence of the bounded native probe."""
import argparse
import hashlib
import json
from pathlib import Path
import re

from analyze_firmware_pm_delay import fields


def require(condition, message):
    if not condition:
        raise ValueError(message)


def analyze(path):
    data = Path(path).read_bytes()
    text = data.decode('ascii')
    waiting = False
    resume = None
    completed = idle = wakes = eois = 0
    receipt = last_exit = trace = probe = terminal = None
    receipts = []
    hlt_sites = []
    wake_sites = []
    hardware_active_on_timer = 0
    capability = None
    for block in re.split(r'(?=OVMF\.)', text):
        v = fields(block)
        if block.startswith('OVMF.HLT capability '):
            require(v.get('supported') == 1 and v['misc'] & 0x40, 'HLT capability missing')
            capability = v
        elif block.startswith('OVMF.CLOCK_APIC exit '):
            last_exit = v
        elif block.startswith('OVMF.IRQ receipt=') and waiting:
            receipt = v
        elif block.startswith('OVMF.HLT completion '):
            require(not waiting, 'completed HLT while still logically halted')
            require(v.get('result') == 0 and v.get('failed') == 0 and v.get('halted') == 1,
                    'HLT completion failed')
            require(v['completed'] == completed + 1 and v['opcode'] == 0xf4 and
                    v['resume_rip'] == v['rip'] + 1 and v['flags'] & 0x200 and
                    v['blocking'] <= 2 and v['activity_before'] == 0, 'invalid HLT retirement')
            require(eois == wakes, 'next HLT reached before wake EOI evidence')
            require(last_exit and last_exit.get('reason') == 12 and last_exit['rip'] == v['rip'],
                    'HLT record has no matching hardware exit')
            completed += 1
            hlt_sites.append(v['rip'])
            resume, waiting, receipt = v['resume_rip'], True, None
        elif block.startswith('OVMF.HLT after_entry '):
            require(waiting and v.get('result') == 0 and not v.get('failed'), 'invalid halted entry outcome')
            if v['injected']:
                require(v['receipt_verified'] == 1 and v['activity_exit'] == 0 and not v['halted'],
                        'wake without active state and verified receipt')
                require(receipt and receipt.get('receipt') == 3 and receipt.get('native_status') == 1 and
                        receipt['reason'] == v['reason'] and not receipt['vectoring'] & 0x80000000,
                        'wake missing independent IRQ receipt')
                require(receipt['attempt'] not in receipts, 'reused IRQ receipt')
                receipts.append(receipt['attempt'])
                wakes += 1
                waiting = False
                wake_sites.append({'rip': v['rip'], 'reason': v['reason']})
            else:
                require(not v['receipt_verified'] and v['reason'] == 52 and
                        v['rip'] == resume and v['halted'] == 1 and v['activity_exit'] in (0, 1),
                        'timer exit executed guest code or spuriously woke guest')
                hardware_active_on_timer += v['activity_exit'] == 0
                idle += 1
            require(v['idle_exits'] == idle and v['wakeups'] == wakes, 'inconsistent wait counters')
            receipt = None
        elif block.startswith('OVMF.HLT wake_EOI '):
            require(not waiting and v['wakeups'] == wakes and wakes == eois + 1 and
                    v['eois'] == eois + 1, 'EOI not matched to a unique wake')
            require(last_exit and last_exit.get('reason') == 48 and
                    last_exit.get('gpa') == 0xfee000b0 and last_exit['rip'] == v['rip'],
                    'EOI has no matching APIC MMIO exit')
            eois += 1
        elif block.startswith('OVMF.HLT probe_complete '):
            require(probe is None and waiting and completed >= 4 and idle > 0 and wakes >= 3 and eois >= 3,
                    'probe stopped before repeated wait/wake cycles')
            require(all(v[k] == n for k, n in [('completed', completed), ('idle_exits', idle),
                                               ('wakeups', wakes), ('eois', eois)]), 'probe counters disagree')
            probe = v
        elif block.startswith('OVMF.HLT terminal '):
            require(not v['failed'] and v['halted'] == int(waiting), 'terminal HLT state failed')
            require(all(v[k] == n for k, n in [('completed', completed), ('idle_exits', idle),
                                               ('wakeups', wakes), ('eois', eois)]), 'terminal counters disagree')
            terminal = v
        elif block.startswith('OVMF.TRACE dump_end '):
            trace = v
    require(capability and probe and terminal and trace, 'missing completed probe or trace footer')
    require(all(trace[k] == 0 for k in ['failed', 'incomplete', 'dropped_bytes']), 'trace incomplete')
    require(trace['encoded_bytes'] < 2097152 and trace['accepted_logical_bytes'] == trace['decoded_bytes'],
            'trace capacity or decoding mismatch')
    require('STOPPED cleanup, boot not completed' in text and 'exit_budget_reached' not in text and
            'observation_time_budget=30s reached' not in text and 'OVMF.PM_DELAY dispatch_rejected' not in text,
            'unexpected budget/rejection/cleanup result')
    return {'serial_sha256': hashlib.sha256(data).hexdigest(), 'completed': completed,
            'idle_exits': idle, 'wakeups': wakes, 'guest_eois': eois,
            'hardware_active_on_timer': hardware_active_on_timer, 'hlt_sites': hlt_sites,
            'wake_sites': wake_sites, 'receipt_attempts': receipts,
            'capability': capability, 'probe': probe, 'terminal': terminal, 'trace': trace,
            'acceptance': 'Repeated idle/wake cycles only; not interactive Shell or Linux boot'}


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('serial', type=Path)
    args = ap.parse_args()
    print(json.dumps(analyze(args.serial), indent=2))
