#!/usr/bin/env python3
"""Create new console runtime; preserve the verified HLT source and evidence."""
import argparse,hashlib,json,shutil
from pathlib import Path
FILES=['krnl/firmware_console.c','krnl/firmware_console.h','krnl/firmware_console_observe.inc','efi/firmware-console.mk','krnl/runtime.c']
def once(s,a,b):
    assert s.count(a)==1,a
    return s.replace(a,b)
def patch(s):
    s=once(s,'#define log_text observation_text','''#ifdef SYLE_FIRMWARE_CONSOLE
#include "firmware_console.h"
static struct fcon_state console_state;
static uint8_t console_output[16384];static unsigned console_count;
static void console_live_byte(uint8_t b){char text[2]={(char)b,0};log_text(text);}
#endif
#define log_text observation_text''')
    s=once(s,'static bool dispatch(uint64_t reason){','''#ifdef SYLE_FIRMWARE_CONSOLE
#include "firmware_console_observe.inc"
#endif
static bool dispatch(uint64_t reason){''')
    s=once(s,'  unsigned port=(unsigned)((q>>16)&0xffff);','''  unsigned port=(unsigned)((q>>16)&0xffff);
#ifdef SYLE_FIRMWARE_CONSOLE
  if(fcon_port(&console_state,(uint16_t)port))return console_dispatch(rip,q,(unsigned)len);
  if(port>=0xcfc&&port<=0xcff&&(pci.pci.address&0x80ffff00)==0x80001000)
   return console_config(rip,q,(unsigned)len);
#endif''')
    s=once(s,'  native.entry_status=UINT32_MAX;','''#ifdef SYLE_FIRMWARE_CONSOLE
  if(!console_poll()){log_text("OVMF.CONSOLE input_error\\n");break;}
#endif
  native.entry_status=UINT32_MAX;''')
    s=once(s,' if(!ok){uint64_t rip=0,err=0,q=0;', '''#ifdef SYLE_FIRMWARE_CONSOLE
 log_text("OVMF.CONSOLE terminal bytes=");log_hex(console_count);log_text(" received=");log_hex(console_state.received);
 log_text(" reads=");log_hex(console_state.reads);log_text(" writes=");log_hex(console_state.writes);
 log_text(" bar=");log_hex(console_state.bar);log_text(" command=");log_hex(console_state.command);log_text(" failed=");log_hex(console_state.failed);log_text("\\n");
 for(unsigned i=0;i<console_count;i++){log_text("OVMF.CONSOLE TX byte=");log_hex(console_output[i]);log_text("\\n");}
#endif
 if(!ok){uint64_t rip=0,err=0,q=0;''')
    s=once(s,'static void observation_text(const char*s){','static bool console_sample_quiet;\nstatic void observation_text(const char*s){if(console_sample_quiet)return;')
    s=once(s,'static void observation_hex(uint64_t value){','static void observation_hex(uint64_t value){if(console_sample_quiet)return;')
    s=once(s,'  /* Bounded diagnostic expansion only; same per-entry execution slice. */','  console_sample_quiet=console_state.command && (observed&255)!=0;\n  /* Bounded diagnostic expansion only; same per-entry execution slice. */')
    s=once(s,'   log_text("OVMF.PM_DELAY dispatch_rejected reason=");','   console_sample_quiet=false;log_text("OVMF.PM_DELAY dispatch_rejected reason=");')
    s=once(s,'#ifdef SYLE_FIRMWARE_HALT\n log_text("OVMF.HLT terminal completed=");',' console_sample_quiet=false;\n#ifdef SYLE_FIRMWARE_HALT\n log_text("OVMF.HLT terminal completed=");')
    s=once(s,'  native.resume=1;\n }\n','''#ifdef SYLE_FIRMWARE_CONSOLE_PROBE
  if(console_probe_complete){console_sample_quiet=false;observed++;log_text("OVMF.CONSOLE command_probe_complete\\n");break;}
#endif
  native.resume=1;
 }
''')
    s=once(s,'static void observation_finish(void){','''static void observation_finish(void){
#ifdef SYLE_FIRMWARE_CONSOLE
 extern void log_serial_drain_mode(void);log_serial_drain_mode();
#endif''')
    return s
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    repo=Path(__file__).resolve().parents[3];paths=[]
    for line in (a.base/'source.sha256').read_text().splitlines():
        digest,rel=line.split('  ',1);assert not Path(rel).is_absolute() and '..' not in Path(rel).parts
        src=a.base/'source'/rel;assert not src.is_symlink() and hashlib.sha256(src.read_bytes()).hexdigest()==digest;paths.append(rel)
    observer=a.base/'source/krnl/firmware_ovmf_quiet_observe.c'
    assert hashlib.sha256(observer.read_bytes()).hexdigest()=='40af7243414684a01a14e2dcbf988cb9d94d55d391bb6d76f3267381a916aca7'
    a.output.mkdir(exist_ok=False,parents=True);dest=a.output/'source'
    for rel in paths:
        target=dest/rel;target.parent.mkdir(exist_ok=True,parents=True);shutil.copy2(a.base/'source'/rel,target)
    for rel in FILES:shutil.copy2(repo/rel,dest/rel)
    (dest/'krnl/firmware_ovmf_quiet_observe.c').write_text(patch(observer.read_text()))
    (dest/'efi/firmware-console-runtime.mk').write_text('build/fconsole-observer.o: KFLAGS += -DFPM_DELAY_EXIT_CAP=131072 -DSYLE_FIRMWARE_CONSOLE_PROBE\n')
    (a.output/'source.sha256').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+str(f.relative_to(dest))+'\n' for f in sorted(dest.rglob('*')) if f.is_file()))
    (a.output/'preparation.json').write_text(json.dumps({'base':str(a.base),'added':FILES,'budgets':'131072 exits / 30s reference / 2MiB trace','prior_wbinvd':45},indent=2))
