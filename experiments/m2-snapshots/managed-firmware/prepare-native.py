#!/usr/bin/env python3
"""Use admitted PIO fixture; all guest input comes from the Ring3 manager."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('candidate',type=Path);p.add_argument('--ledger',type=int,required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[3];root=a.candidate.resolve()
backing=root/'backing.img';assert not backing.exists()
seed=(repo/'efi/build/persistent-accepted-20261118/disk.img').read_bytes()
assert hashlib.sha256(seed).hexdigest()=='638fe8556e27ecb9a196871e33e790e8ee96bd7ad7df258b07c2f060173db9f6'
(root/'disk.img').write_bytes(seed)
backing.write_bytes(seed)
subprocess.run([sys.executable,str(repo/'experiments/m2-snapshots/persistent-progress/prepare-native.py'),str(root),'--backing',str(backing),'--token','9a473c02e61f8b5d7349ab1260ec572d','--phase','write','--ledger',str(a.ledger)],check=True)
f=root/'native/positive_once.py';s=f.read_text();a0=s.index(' commands=');b=s.index(';last_prompt=0',a0);s=s[:a0]+' commands=[]'+s[b:];f.write_text(s)
(root/'native/lifecycle-plan.json').write_text(json.dumps({'host_serial_input':[], 'manager':'embedded RX-only ELF via strict Ring3 loader', 'sequence':['create','boot Linux disk root','write token','stop','256 stopped status and rejected step','start same handle','verify PID/variable and token','stop','destroy','stale status','create new handle','boot UEFI again','verify disk token','stop','destroy','stale status','owner exit'], 'baseline_backing_sha256':hashlib.sha256(backing.read_bytes()).hexdigest()},indent=2))
