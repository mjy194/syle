#!/usr/bin/env python3
import hashlib,json,pathlib,subprocess,sys
kernel=pathlib.Path(sys.argv[1]);managers=pathlib.Path(sys.argv[2]);out=pathlib.Path(sys.argv[3]);assert not out.exists()
layout={k:int(v) for k,v in (line.split('=') for line in (managers/'layout.txt').read_text().splitlines())}
rows=[r.split() for r in subprocess.check_output(['nm','-Sn',str(kernel)],text=True).splitlines()]
def symbol(name,size):
 found=[r for r in rows if len(r)==4 and r[2] in 'bBdD' and r[3]==name and int(r[1],16)==size];assert len(found)==1,(name,found)
 return {'address':int(found[0][0],16),'size':size}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inventory={name:symbol(name,size) for name,size in [('session',layout['session.size']),('managed_guest',layout['managed_guest.size']),('nmi_probe_owner_exit_calls',8),('stats',24),('available_count',4),('available',4096*16)]}
m={'kernel':str(kernel),'kernel_sha256':sha(kernel),'snapshots':inventory,'layout':layout,'managers':{}}
for mode in [0,1]:
 p=managers/f'mgmt-nmi-{mode}.elf';syms=[r.split() for r in subprocess.check_output(['nm','-n',str(p)],text=True).splitlines()];wait=[r for r in syms if len(r)==3 and r[2]=='nmi_probe_wait'];assert len(wait)==1
 m['managers'][str(mode)]={'path':str(p),'sha256':sha(p),'wait_rip':int(wait[0][0],16),'state':1+mode}
out.write_text(json.dumps(m,indent=2)+'\n')
