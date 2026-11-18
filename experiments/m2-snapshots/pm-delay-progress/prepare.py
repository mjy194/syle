#!/usr/bin/env python3
"""Create an isolated PM-delay observer from the verified PAUSE runtime source."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

OBSERVER_SHA = '1350056443caa3e10e3e72068ad4b29dbc23f0b345dbc32cb5b014970be4d0ae'


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('source anchor mismatch: ' + old[:100])
    return text.replace(old, new)


def wrap(text, start, end, condition):
    a = text.index(start)
    b = text.index(end, a) + len(end)
    return text[:a] + 'if(' + condition + '){\n' + text[a:b] + '\n }' + text[b:]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base', type=Path, required=True, help='Prior run directory with source.sha256 and source/')
    ap.add_argument('--output', type=Path, required=True, help='Must not exist')
    ap.add_argument('--stack-frames', type=int, choices=[4, 8], default=4,
                    help='Bounded read-only caller diagnostics; default preserves baseline')
    ap.add_argument('--shell-delay-probe', action='store_true',
                    help='Read pinned Shell countdown only with an eight-frame caller match')
    args = ap.parse_args()
    if args.shell_delay_probe and args.stack_frames != 8:
        ap.error('--shell-delay-probe requires --stack-frames 8')
    base, out = args.base.resolve(), args.output.resolve()
    observer = base / 'source/krnl/firmware_ovmf_quiet_observe.c'
    if hashlib.sha256(observer.read_bytes()).hexdigest() != OBSERVER_SHA:
        raise ValueError('not the frozen PAUSE observer')
    entries = []
    for line in (base / 'source.sha256').read_text().splitlines():
        digest, name = line.split(None, 1)
        rel = Path(name.strip())
        if rel.is_absolute() or '..' in rel.parts:
            raise ValueError('unsafe manifest path')
        src = base / 'source' / rel
        if src.is_symlink() or hashlib.sha256(src.read_bytes()).hexdigest() != digest:
            raise ValueError('base source changed: ' + name)
        entries.append(rel)
    out.mkdir(parents=True, exist_ok=False)
    dest = out / 'source'
    for rel in entries:
        target = dest / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(base / 'source' / rel, target)
    text = observer.read_text()
    text = replace_once(text, 'static struct fpa_state pause_state;', '''static struct fpa_state pause_state;
#ifndef FPM_DELAY_EXIT_CAP
#error "Use the isolated firmware-pm-delay target"
#endif
_Static_assert(FPM_DELAY_EXIT_CAP==6144 || FPM_DELAY_EXIT_CAP==32768 || FPM_DELAY_EXIT_CAP==131072,"bounded diagnostic profiles only");
static bool pm_delay_seen,pm_delay_quiet;
static uint64_t pm_delay_target,pm_delay_reads,pm_delay_pauses,pm_delay_quiet_exits;
static bool pm_delay_report;''')
    # Only log blocks are sampled. All state checks, clock samples, device reads,
    # VMCS writes, IRQ receipts and entry/exit accounting execute as before.
    text = wrap(text, ' log_text("OVMF.IRQ fresh_gate flags=");',
                'log_hex(vector);log_text("\\n");',
                '!pm_delay_quiet || irq_session.owner.pending || irq_session.owner.in_service')
    text = wrap(text, '   log_text("OVMF.IRQ window_gate=");',
                'log_hex(primary);log_text("\\n");',
                '!pm_delay_quiet || gate!=FIW_WAIT || irq_session.owner.pending || irq_session.owner.in_service')
    text = wrap(text, '  log_text("OVMF.CLOCK_APIC pci_cycle port=");',
                'log_hex(accepted);log_text("\\n");', '!pm_delay_quiet || !accepted')
    text = wrap(text, '  log_text("OVMF.CLOCK_APIC pm_state base=");',
                'log_hex(pci.pm.timer_status);log_text("\\n");', '!pm_delay_quiet')
    text = replace_once(text, '  return true; /* fpp committed RIP exactly once. */', '''  if(port==0xb008 && rip==0x70c231b && q==0xb008000b){
   pm_delay_reads++;
   /* At this post-IN boundary EBX is the guest target and EAX the actual
    * returned count. Only report crossing after the known PAUSE loop bound it.
    * It proves timer progress, not execution of the following guest branch. */
   if(pm_delay_seen && !pm_delay_report &&
      (uint32_t)native.gpr[1]==pm_delay_target &&
      (((uint32_t)native.gpr[1]-(uint32_t)native.gpr[0])&0x800000)){
    pm_delay_report=true;
    log_text("OVMF.PM_DELAY target_crossed target=");log_hex(native.gpr[1]);
    log_text(" returned=");log_hex(native.gpr[0]);log_text(" pm_ticks=");log_hex(pci.pm.ticks);
    log_text(" reference=");log_hex(active_apic.reference);log_text(" reads=");log_hex(pm_delay_reads);
    log_text(" pauses=");log_hex(pm_delay_pauses);log_text(" branch_not_yet_observed\\n");
   }
  }
  return true; /* fpp committed RIP exactly once. */''')
    text = replace_once(text, ' log_text("OVMF.PAUSE result=");', ''' if(result==FAT_COMMITTED && trace.rip==0x70c236e){
  pm_delay_pauses++;
  bool fresh=!pm_delay_seen || pm_delay_target!=(uint32_t)native.gpr[1];
  if(fresh)pm_delay_report=false;
  pm_delay_seen=true;pm_delay_target=(uint32_t)native.gpr[1];
  if(fresh || (pm_delay_pauses&255)==0){
   log_text("OVMF.PM_DELAY snapshot pauses=");log_hex(pm_delay_pauses);
   log_text(" target_ebx=");log_hex(native.gpr[1]);log_text(" remaining_eax=");log_hex(native.gpr[0]);
   log_text(" request_edi=");log_hex(native.gpr[5]);log_text(" chunks_r12=");log_hex(native.gpr[11]);
   log_text(" pm_ticks=");log_hex(pci.pm.ticks);log_text(" reference=");log_hex(active_apic.reference);
   log_text(" no_extra_clock_sample\\n");
  }
 }
 if(!pm_delay_quiet || result!=FAT_COMMITTED){
 log_text("OVMF.PAUSE result=");''')
    text = replace_once(text, ' log_text(" unchanged_clock_and_exit_budgets\\n");',
                        ' log_text(" same_instruction_and_clock_semantics\\n");\n }')
    text = replace_once(text, ' for(;observed<6144;observed++){', ''' log_text("OVMF.PM_DELAY policy exit_cap=");log_hex(FPM_DELAY_EXIT_CAP);
 log_text(" sample_stride=256 reference_seconds=30 trace_capacity=2097152\\n");
 for(;observed<FPM_DELAY_EXIT_CAP;observed++){''')
    text = replace_once(text, '  log_text("OVMF.CLOCK_APIC exit reason=");', '''  pm_delay_quiet=pm_delay_seen && (observed&255)!=0 &&
   ((reason==40 && rip==0x70c236e) || (reason==30 && rip==0x70c231b && q==0xb008000b));
  if(pm_delay_quiet)pm_delay_quiet_exits++;
  if(!pm_delay_quiet){
  log_text("OVMF.CLOCK_APIC exit reason=");''')
    text = replace_once(text, '  if(reason>UINT16_MAX||!dispatch(reason))break;', '''  }
  if(reason>UINT16_MAX||!dispatch(reason)){
   log_text("OVMF.PM_DELAY dispatch_rejected reason=");log_hex(reason);log_text(" rip=");log_hex(rip);log_text(" qual=");log_hex(q);log_text("\\n");break;
  }''')
    text = replace_once(text, ' if(safe())(void)rtc_observation("terminal_no_new_sample");', ''' pm_delay_quiet=false;
 log_text("OVMF.PM_DELAY terminal reads=");log_hex(pm_delay_reads);log_text(" pauses=");log_hex(pm_delay_pauses);
 log_text(" quiet_exits=");log_hex(pm_delay_quiet_exits);log_text(" last_target=");log_hex(pm_delay_target);
 log_text(" target_crossed=");log_hex(pm_delay_report);log_text(" sampled_trace_not_every_exit\\n");
 if(safe())(void)rtc_observation("terminal_no_new_sample");''')
    text = replace_once(text, ' if(observed==6144)log_text("OVMF.CLOCK_APIC observation_exit_budget=6144 reached\\n");',
                        ' if(observed==FPM_DELAY_EXIT_CAP){log_text("OVMF.PM_DELAY exit_budget_reached cap=");log_hex(FPM_DELAY_EXIT_CAP);log_text("\\n");}')
    text = text.replace('lossless_compact_v1_same30s_timer_VMbudget_exitcap6144_no_new_clock_sample',
                        'compact_v1_selected_records_only_same30s_sameVMslice_no_new_clock_sample')
    if args.shell_delay_probe:
        text = replace_once(text, ' return true; /* Missing/non-frame-pointer chain is diagnostic, not a guest fault. */', '''
 /* Pinned Shell call site: MOV ECX,1000000; CALL [RAX+0xf8]. These reads
  * observe owned guest RAM only; a mismatch is diagnostic, never approval. */
 for(unsigned i=1;ok && i<result.count && i<FOS_MAX_FRAMES;i++){
  if(result.frames[i].return_rip!=0x685b97e || result.frames[i].frame<32 ||
     result.frames[i-1].return_rip!=0x7f1735e)continue;
  static const uint8_t expected[]={0xb9,0x40,0x42,0x0f,0,0xff,0x90,0xf8,0,0,0};
  uint8_t bytes[15];size_t n=fmm_fetch(&m,0x685b973,bytes);bool match=n>=sizeof expected;
  for(unsigned j=0;j<sizeof expected && match;j++)if(bytes[j]!=expected[j])match=false;
  uint64_t configured=0,remaining=0;
  bool readable=match && fos_read_word(&m,0x68ad080,&configured) &&
   fos_read_word(&m,result.frames[i].frame-32,&remaining);
  log_text("OVMF.PM_DELAY shell_countdown readable=");log_hex(readable);
  log_text(" configured_seconds=");log_hex(configured);log_text(" remaining_including_current=");log_hex(remaining);
  log_text(" pinned_callsite_readonly_no_clock_sample\\n");
  break;
 }
 return true; /* Missing/non-frame-pointer chain is diagnostic, not a guest fault. */''')
        text = replace_once(text, '  if(fresh)pm_delay_report=false;',
                            '  if(fresh){pm_delay_report=false;(void)rtc_observation("delay_new_target");}')
    (dest / 'krnl/firmware_ovmf_quiet_observe.c').write_text(text)
    if args.stack_frames == 8:
        header = dest / 'krnl/firmware_observer_stack.h'
        header.write_text(replace_once(header.read_text(), '#define FOS_MAX_FRAMES 4u',
                                       '#define FOS_MAX_FRAMES 8u'))
        test = dest / 'krnl/tests/firmware_observer_stack.c'
        fixture = replace_once(test.read_text(), 'i<5;i++', 'i<FOS_MAX_FRAMES+1;i++')
        fixture = replace_once(fixture, 'i==4?0:', 'i==FOS_MAX_FRAMES?0:')
        fixture = replace_once(fixture, 'r.count==4&&r.stop==FOS_LIMIT',
                                'r.count==FOS_MAX_FRAMES&&r.stop==FOS_LIMIT')
        test.write_text(fixture)
    (dest / 'efi/firmware-pm-delay.mk').write_text('''# Include after firmware-pause.mk and before firmware-ovmf-quiet.mk.
PM_DELAY_CAP ?= 6144
FQ_KERNEL := build/krnl-pm-delay-$(PM_DELAY_CAP).elf
FQ_OBSERVER := build/fpm-delay-$(PM_DELAY_CAP)-observer.o
$(FQ_OBSERVER): KFLAGS += -DSYLE_FIRMWARE_CODE_ROM -DSYLE_FIRMWARE_PAUSE -DFPM_DELAY_EXIT_CAP=$(PM_DELAY_CAP)
$(FQ_KERNEL): build/fpause-model.o build/fq-firmware_code_rom.o
.PHONY: firmware-pm-delay
firmware-pm-delay: $(FQ_KERNEL)
''')
    (out / 'preparation.json').write_text(json.dumps({
        'base': str(base), 'verified_source_files': len(entries),
        'changed': ['krnl/firmware_ovmf_quiet_observe.c'] +
                   (['krnl/firmware_observer_stack.h', 'krnl/tests/firmware_observer_stack.c']
                    if args.stack_frames == 8 else []),
        'added': ['efi/firmware-pm-delay.mk'], 'stack_frames': args.stack_frames,
        'shell_delay_probe': args.shell_delay_probe,
        'profiles': [6144, 32768, 131072], 'reference_seconds': 30, 'trace_capacity': 2097152,
        'sampling': 'Only repetitive known delay-loop log blocks, every 256 exits; failures and IRQ receipts retained',
        'prior_architectural_wbinvd_count': 39}, indent=2))
    lines = [hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + str(p.relative_to(dest))
             for p in sorted(dest.rglob('*')) if p.is_file()]
    (out / 'source.sha256').write_text('\n'.join(lines) + '\n')
    print(out)


if __name__ == '__main__':
    main()
