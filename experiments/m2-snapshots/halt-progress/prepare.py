#!/usr/bin/env python3
"""Create an isolated HLT candidate from the verified PM-countdown runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

OBSERVER_SHA = '0bc0cfde67fca1c8f6655a68fe230eeb21497ad40168900f1550a3719ec2de2b'
FILES = ['krnl/firmware_halt.c', 'krnl/firmware_halt.h', 'krnl/firmware_halt_observe.inc',
         'krnl/tests/firmware_halt.c', 'krnl/tests/firmware_halt.sh', 'efi/firmware-halt.mk']


def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('source anchor mismatch: ' + old[:100])
    return text.replace(old, new)


def patch_observer(text):
    text = once(text, 'static struct fpa_state pause_state;', '''static struct fpa_state pause_state;
#ifdef SYLE_FIRMWARE_HALT
#include "firmware_halt.h"
static struct fhl_state halt_state;
static bool halt_supported;
static uint64_t halt_eois,halt_last_eoi_wake;
#endif''')
    text = once(text, 'static bool dispatch(uint64_t reason){', '''#ifdef SYLE_FIRMWARE_HALT
#include "firmware_halt_observe.inc"
#endif
static bool dispatch(uint64_t reason){''')
    text = once(text, '#ifdef SYLE_FIRMWARE_PAUSE\n if(reason==40)', '''#ifdef SYLE_FIRMWARE_HALT
 if(reason==12)return halt_dispatch();
#endif
#ifdef SYLE_FIRMWARE_PAUSE
 if(reason==40)''')
    text = once(text, ' uint64_t pin_controls;if(!rd(0x4000,&pin_controls)', '''#ifdef SYLE_FIRMWARE_HALT
 uint64_t halt_misc;
 if(!ops.read_msr(ops.context,0x485,&halt_misc))goto cleanup;
 halt_supported=(halt_misc&0x40)!=0;
 log_text("OVMF.HLT capability misc=");log_hex(halt_misc);log_text(" supported=");log_hex(halt_supported);log_text("\\n");
 if(!halt_supported)goto cleanup;
#endif
 uint64_t pin_controls;if(!rd(0x4000,&pin_controls)''')
    text = once(text, '  uint64_t irq_attempt=0,window_attempt=0;bool staged=false,windowed=false;', '''  uint64_t irq_attempt=0,window_attempt=0;bool staged=false,windowed=false;
#ifdef SYLE_FIRMWARE_HALT
  bool halt_receipt=false;
#endif''')
    text = once(text, '  native.entry_status=UINT32_MAX;', '''#ifdef SYLE_FIRMWARE_HALT
  if(!halt_entry_check()){log_text("OVMF.HLT entry_state_rejected\\n");mode_commit_failed();break;}
  bool entered_halted=halt_state.halted;
#endif
  native.entry_status=UINT32_MAX;''')
    text = once(text, '   if(receipt!=FTIS_ACCOUNTED)break;', '''   if(receipt!=FTIS_ACCOUNTED)break;
#ifdef SYLE_FIRMWARE_HALT
   halt_receipt=true;
#endif''')
    text = once(text, '  reason=raw_reason;', '''#ifdef SYLE_FIRMWARE_HALT
  if(entered_halted&&!halt_after_entry(native.entry_status==1&&!state.host_dirty&&safe()&&irq_identity(),staged,halt_receipt))break;
#endif
  reason=raw_reason;''')
    text = once(text, '   enum fat_result ir=fiap_mmio', '''#ifdef SYLE_FIRMWARE_HALT
   bool halt_was_in_service=irq_session.owner.in_service;
#endif
   enum fat_result ir=fiap_mmio''')
    text = once(text, '   log_text("OVMF.IRQ mmio result=");', '''#ifdef SYLE_FIRMWARE_HALT
   if(ir==FAT_COMMITTED&&e.physical==UINT64_C(0xfee000b0)&&halt_was_in_service&&
      !irq_session.owner.in_service&&halt_state.wakeups>halt_last_eoi_wake){
    halt_last_eoi_wake=halt_state.wakeups;halt_eois++;
    log_text("OVMF.HLT wake_EOI wakeups=");log_hex(halt_state.wakeups);
    log_text(" eois=");log_hex(halt_eois);log_text(" rip=");log_hex(e.rip);log_text(" committed_guest_EOI\\n");
   }
#endif
   log_text("OVMF.IRQ mmio result=");''')
    text = once(text, '  native.resume=1;\n }\n', '''#ifdef SYLE_FIRMWARE_HALT_PROBE
  if(halt_state.completed>=4&&halt_state.halted&&halt_state.idle_exits&&halt_state.wakeups>=3&&halt_eois>=3){
   observed++;
   log_text("OVMF.HLT probe_complete completed=");log_hex(halt_state.completed);
   log_text(" idle_exits=");log_hex(halt_state.idle_exits);log_text(" wakeups=");log_hex(halt_state.wakeups);
   log_text(" eois=");log_hex(halt_eois);log_text(" reference=");log_hex(active_apic.reference);
   log_text(" bounded_three_wake_cycles_not_boot_acceptance\\n");break;
  }
#endif
  native.resume=1;
 }
#ifdef SYLE_FIRMWARE_HALT
 log_text("OVMF.HLT terminal completed=");log_hex(halt_state.completed);
 log_text(" idle_exits=");log_hex(halt_state.idle_exits);log_text(" wakeups=");log_hex(halt_state.wakeups);
 log_text(" eois=");log_hex(halt_eois);log_text(" halted=");log_hex(halt_state.halted);
 log_text(" failed=");log_hex(halt_state.failed);log_text("\\n");
#endif
''')
    return text


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    base, out = args.base.resolve(), args.output.resolve()
    repo = Path(__file__).resolve().parents[3]
    observer = base / 'source/krnl/firmware_ovmf_quiet_observe.c'
    if hashlib.sha256(observer.read_bytes()).hexdigest() != OBSERVER_SHA:
        raise ValueError('not the frozen final countdown observer')
    paths = []
    for line in (base / 'source.sha256').read_text().splitlines():
        digest, name = line.split(None, 1)
        rel = Path(name.strip())
        if rel.is_absolute() or '..' in rel.parts:
            raise ValueError('unsafe manifest path')
        src = base / 'source' / rel
        if src.is_symlink() or hashlib.sha256(src.read_bytes()).hexdigest() != digest:
            raise ValueError('base source changed: ' + name)
        paths.append(rel)
    out.mkdir(parents=True, exist_ok=False)
    dest = out / 'source'
    for rel in paths:
        target = dest / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(base / 'source' / rel, target)
    for name in FILES:
        shutil.copy2(repo / name, dest / name)
    (dest / 'krnl/firmware_ovmf_quiet_observe.c').write_text(patch_observer(observer.read_text()))
    (dest / 'efi/firmware-halt-runtime.mk').write_text('''# Native PM-countdown baseline; include after firmware-halt.mk.
build/fhalt-observer.o: KFLAGS += -DFPM_DELAY_EXIT_CAP=131072 -DSYLE_FIRMWARE_HALT_PROBE
''')
    (out / 'preparation.json').write_text(json.dumps({
        'base': str(base), 'verified_source_files': len(paths), 'added': FILES + ['efi/firmware-halt-runtime.mk'],
        'changed': ['krnl/firmware_ovmf_quiet_observe.c'],
        'limits': {'dispatches': 131072, 'reference_seconds': 30, 'trace_bytes': 2097152, 'outer_seconds': 180},
        'probe_stop': 'four HLT completions, at least one no-injection idle slice, three verified wakes and guest EOIs',
        'prior_architectural_wbinvd_count': 44}, indent=2))
    files = sorted(p for p in dest.rglob('*') if p.is_file())
    (out / 'source.sha256').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' +
                                              str(p.relative_to(dest)) + '\n' for p in files))
    print(out)


if __name__ == '__main__':
    main()
