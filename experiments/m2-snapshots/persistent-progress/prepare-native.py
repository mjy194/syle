#!/usr/bin/env python3
"""Fresh one-shot run with an explicitly admitted persistent PIO backing file."""
from pathlib import Path
import argparse,hashlib,json,stat,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('candidate',type=Path);p.add_argument('--backing',type=Path,required=True);p.add_argument('--token',required=True);p.add_argument('--phase',choices=['write','read'],required=True);p.add_argument('--ledger',type=int,required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[3];root=a.candidate.resolve();backing=a.backing.resolve()
assert backing.is_file() and not a.backing.is_symlink() and backing.stat().st_size==67108864 and repo in backing.parents
assert 16<=len(a.token)<=96 and all(c in '0123456789abcdef' for c in a.token)
subprocess.run([sys.executable,str(repo/'experiments/m2-snapshots/disk-progress/prepare-native.py'),str(root),'--ledger',str(a.ledger)],check=True)
stage=root/'native';f=stage/'positive_once.py';s=f.read_text()
needle="json.dump(a,open('command.json','w'),indent=2)"
insert="""a+=['-drive','if=none,id=persist-backend,format=raw,cache=writeback,aio=threads,file='+BACKING,
 '-device','piix3-ide,id=persist-ide,addr=05.0',
 '-device','ide-hd,id=persist-drive,bus=persist-ide.0,unit=0,drive=persist-backend,serial=SYLE-PERSIST-0001,bootindex=2']
""".replace('BACKING',repr(str(backing)))
s=s.replace(needle,insert+needle)
s=s.replace(" for n in ['query-pci','query-memory-devices','query-fdsets']:assert query(n)==old[n],n", """ pci=query('query-pci');assert len(pci)==1 and pci[0]['bus']==0
 extra=[d for d in pci[0]['devices'] if d['slot']==5 and d['function']==0];assert len(extra)==1
 dev=extra[0];assert dev['qdev_id']=='persist-ide' and dev['id']['vendor']==0x8086 and dev['id']['device']==0x7010 and dev['class_info']['class']==0x101
 assert dev['regions']==[{'bar':4,'size':16,'address':-1,'type':'io'}],dev
 reduced=[dict(pci[0],devices=[d for d in pci[0]['devices'] if d is not dev])];assert reduced==old['query-pci']
 for n in ['query-memory-devices','query-fdsets']:assert query(n)==old[n],n
 blocks=query('query-block');disk=[b for b in blocks if b.get('inserted',{}).get('file')==BACKING];assert len(disk)==1
 inserted=disk[0]['inserted'];assert inserted['drv']=='raw' and inserted['ro'] is False and inserted['cache']['no-flush'] is False
""".replace('BACKING',repr(str(backing))))
s=s.replace(" result['admission_graph_equal']=True", """ backing=pathlib.Path(BACKING);meta=backing.stat();assert meta.st_size==67108864 and backing.is_file()
 descriptors=[k for k,v in fd.items() if v==str(backing)];assert len(descriptors)==1
 info=pathlib.Path('/proc/%d/fdinfo/%s'%(proc.pid,descriptors[0])).read_text()
 flags=int(next(line.split()[1] for line in info.splitlines() if line.startswith('flags:')),8);assert flags&3==os.O_RDWR
 result['qemu_pid']=proc.pid
 result['backing_identity']={'device':meta.st_dev,'inode':meta.st_ino,'bytes':meta.st_size,'path':str(backing)}
 result['admission_graph_equal']=True;result['admitted_extension']='one fixed PIIX3 PIO backend, no passthrough/DMA'
""".replace('BACKING',repr(str(backing))))
commands=[f'persistctl {a.phase} {a.token}\r','persistctl reboot\r']
s=s.replace('commands=[];last_prompt=0','commands='+repr([v.encode() for v in commands])+';last_prompt=0')
s=s.replace("prompts=plain.count(b'Shell>')", "prompts=plain.count(b'\\n# ') if b'SYLE_DISK_ROOT_READY' in plain else 0")
s=s.replace('time.monotonic()-start<180','time.monotonic()-start<300')
s=s.replace('start=time.monotonic();rec=[];',"import hashlib,fcntl\nbacking_lock=open("+repr(str(backing))+",'r+b');fcntl.flock(backing_lock,fcntl.LOCK_EX|fcntl.LOCK_NB)\nbefore_sha=hashlib.sha256(backing_lock.read()).hexdigest()\nstart=time.monotonic();rec=[];")
s=s.replace("result.update(returncode=proc.returncode,elapsed=time.monotonic()-start)","result.update(returncode=proc.returncode,elapsed=time.monotonic()-start,backing_before_sha256=before_sha,backing_after_sha256=hashlib.sha256(pathlib.Path("+repr(str(backing))+").read_bytes()).hexdigest())")
f.write_text(s)
record={'backing':str(backing),'phase':a.phase,'token':a.token,'commands':commands,'backing_initial_sha256':hashlib.sha256(backing.read_bytes()).hexdigest(),'explicit_graph_extension':'PIIX3 IDE00:05.0 + primary master serial SYLE-PERSIST-0001, raw rw, cache flush enabled'}
(stage/'persistent-plan.json').write_text(json.dumps(record,indent=2))
pre=stage/'preflight.json';meta=json.loads(pre.read_text());meta['cap']=1048576;meta['reference_seconds']=120;pre.write_text(json.dumps(meta,indent=2))
