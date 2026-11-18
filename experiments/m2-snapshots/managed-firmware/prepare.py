#!/usr/bin/env python3
"""Generate a resumable session from the exact accepted persistent runtime.
Each entry/exit transaction is retained; only its outer lifecycle is split.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,sys,re
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'disk-progress'))
from prepare import once
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();repo=Path(__file__).resolve().parents[3];base=repo/'efi/build/persistent-accepted-20261118';dest=a.output/'source';a.output.mkdir(parents=True,exist_ok=False)
for line in (base/'source.sha256').read_text().splitlines():
 h,name=line.split(None,1);f=base/'source'/name.strip();assert hashlib.sha256(f.read_bytes()).hexdigest()==h
 t=dest/name.strip();t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,t)
files=['krnl/main.c','krnl/ring3.c','krnl/firmware_managed.h','krnl/firmware_managed.inc','krnl/managed_host_context.h','krnl/virtio_blk/ata_pio.c','krnl/virtio_blk/ata_pio.h','efi/firmware-managed.mk','user/mgmt/firmware_lifecycle.c','user/mgmt/firmware_lifecycle.h','user/mgmt/firmware_lifecycle_entry.S']
for name in files:
 t=dest/name;t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/name,t)
f=dest/'krnl/firmware_ovmf_quiet_observe.c';s=f.read_text()
s=once(s,'static bool observation_deferred,observation_finished;', 'static bool observation_deferred,observation_finished;\nstatic void fmg_emit(uint8_t);\nstatic bool fmg_input(void);')
s=once(s,'static void console_live_byte(uint8_t b){char text[2]={(char)b,0};log_text(text);}', 'static void console_live_byte(uint8_t b){fmg_emit(b);}')
s=s.replace('!disk_input()', '!fmg_input()')
start=s.index('bool firmware_probe_run(void){');loop=s.index(' for(;observed<FPM_DELAY_EXIT_CAP;observed++){',start);end=s.index('\n }\n console_sample_quiet=false;',loop)
init=s[start:loop];body=s[loop+len(' for(;observed<FPM_DELAY_EXIT_CAP;observed++){'):end]
init=init.replace('bool firmware_probe_run(void){\n bool ok=false;uint64_t reason=0;', 'static bool fmg_prepare_runtime(void){')
init=once(init,' unsigned observed=0;',' fmg_observed=0;')
init=init.replace('goto cleanup;', 'return false;')
init+=' return true;\n}\n'
# All body breaks target the old outer observation loop (no nested loops).
assert body.count('continue;')==2
body=body.replace('continue;', 'goto fmg_accounted;').replace('break;', 'return fmg_failed_step(__LINE__);')
body=body.replace('observed','fmg_observed').replace('.fmg_observed=','.observed=').replace('reason=raw_reason;', 'fmg_reason=raw_reason;').replace('if(reason==7)', 'if(fmg_reason==7)').replace('if(reason==40', 'if(fmg_reason==40').replace('(reason==30','(fmg_reason==30').replace('log_hex(reason)','log_hex(fmg_reason)').replace('if(reason==48)','if(fmg_reason==48)').replace('if(reason>UINT16_MAX||!dispatch(reason))','if(fmg_reason>UINT16_MAX||!dispatch(fmg_reason))')
# Replace only C identifiers, keeping diagnostic strings and .reason members.
body=re.sub(r'"(?:\\.|[^"\\])*"|(?<![.\w])reason\b',lambda m:'fmg_reason' if m[0]=='reason' else m[0],body)
# Diagnostic probe branches are not management completion policies.
a0=body.index('#ifdef SYLE_FIRMWARE_HALT_PROBE');body=body[:a0]+'\n  native.resume=1;\n'
step='''static int fmg_run_entries(void){
 for(unsigned slice=0;slice<64;slice++){
  if(fmg_observed>=FPM_DELAY_EXIT_CAP)return fmg_failed_step(__LINE__);
'''+body+'''fmg_accounted:
  fmg_observed++;
  if(fmg_io_failed||persistent_reboot)return fmg_failed_step(__LINE__);
 }
 return 0;
}
'''
# The adapter owns reset/prepare/stop/release and wraps each entry in the
# previously tested supervisor-root + selector transition for int80 callers.
s=s[:start]+'''static uint64_t fmg_observed,fmg_reason;
static bool fmg_io_failed;
static int fmg_failed_step(unsigned);
'''+init+step+'\n#include "firmware_managed.inc"\n'
f.write_text(s)
f=dest/'efi/firmware-managed-runtime.mk';f.write_text('build/fmg-observer.o: KFLAGS += -DFPM_DELAY_EXIT_CAP=1048576\n')
(a.output/'source.sha256').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+str(f.relative_to(dest))+'\n' for f in sorted(dest.rglob('*')) if f.is_file()))
(a.output/'preparation.json').write_text(json.dumps({'base':str(base),'profile':'Ring3-managed UEFI + writable virtio disk','entries_per_call':64,'preserved':'same entry/exit/IRQ/device transactions, guest state retained across STOP/START','clock_policy':'admitted host wall clock; samples advance only in execution slices; no synthetic pause-time clock'},indent=2))
