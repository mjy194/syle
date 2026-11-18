#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('candidate',type=Path);p.add_argument('--ledger',type=int,required=True);a=p.parse_args();root=a.candidate.resolve();repo=Path(__file__).resolve().parents[3]
backing=root/'backing.img';assert not backing.exists();backing.write_bytes((root/'disk.img').read_bytes())
subprocess.run([sys.executable,str(repo/'experiments/m2-snapshots/persistent-progress/prepare-native.py'),str(root),'--backing',str(backing),'--token','9a473c02e61f8b5d7349ab1260ec572d','--phase','write','--ledger',str(a.ledger)],check=True)
f=root/'native/positive_once.py';s=f.read_text();a0=s.index(' commands=');b=s.index(';last_prompt=0',a0);s=s[:a0]+' commands=[];captures={};pending={}'+s[b:]
s=s.replace("  prompts=plain.count",'''  for label,marker in [('gop',b'SYLE_GOP_PATTERN_READY'),('linux',b'SYLE_FB_PATTERN_READY')]:
   seen=plain.count(marker)
   for index in range(1,seen+1):
    key=label+'-'+str(index)
    if key not in pending:pending[key]=time.monotonic()
    if key not in captures and time.monotonic()-pending[key]>=.25:
     filename=str((pathlib.Path.cwd()/(key+'.ppm')).resolve())
     s.sendall((json.dumps({'execute':'screendump','arguments':{'filename':filename}})+'\\n').encode())
     while True:
      reply=json.loads(f.readline());rec.append({'command':'screendump','response':reply})
      if 'error' in reply:raise RuntimeError(reply)
      if 'return' in reply:break
     captures[key]=filename
     json.dump(captures,open('captures.json','w'),indent=2)
  prompts=plain.count''')
f.write_text(s)
(root/'native/lifecycle-plan.json').write_text(json.dumps({'host_serial_input':[],'baseline_backing_sha256':hashlib.sha256(backing.read_bytes()).hexdigest(),'capture':'QMP screendump after 250ms stable guest GOP/Linux pattern, no display device graph changes'},indent=2))
