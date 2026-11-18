#!/usr/bin/env python3
"""Reject false wakeups and missing receipts by perturbing a verified trace."""
import argparse
import json
from pathlib import Path
import tempfile

from analyze_firmware_halt import analyze
from analyze_firmware_pm_delay import fields


def check(path):
    analyze(path)
    text = path.read_text()
    start = text.index('OVMF.HLT after_entry ')
    end = text.index('OVMF.', start + 5)
    first = text[start:end]
    rip = fields(first)['rip']
    variants = {
        'false_timer_wakeup': text[:start] + first.replace(
            'halted=0x0000000000000001', 'halted=0x0000000000000000') + text[end:],
        'idle_rip_advanced': text[:start] + first.replace(f'rip={rip:#018x}', f'rip={rip + 1:#018x}') + text[end:],
        'missing_eoi': text.replace('OVMF.HLT wake_EOI ', 'OVMF.HLT removed_EOI ', 1),
        'missing_irq_receipt': text[:start] + text[start:].replace(
            'OVMF.IRQ receipt=', 'OVMF.IRQ removed_receipt=', 1),
        'missing_trace_footer': text.split('OVMF.TRACE dump_end ', 1)[0],
        'missing_probe_completion': text.replace('OVMF.HLT probe_complete ', 'OVMF.HLT removed_complete ', 1),
    }
    with tempfile.TemporaryDirectory() as directory:
        damaged = Path(directory) / 'serial.log'
        for name, variant in variants.items():
            if variant == text:
                raise ValueError('fixture was not changed: ' + name)
            damaged.write_text(variant)
            try:
                analyze(damaged)
            except (ValueError, KeyError):
                continue
            raise ValueError('accepted damaged evidence: ' + name)
    return {'valid_log': 'PASS', 'rejected_mutations': list(variants)}


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('serial', type=Path)
    args = ap.parse_args()
    print(json.dumps(check(args.serial), indent=2))
