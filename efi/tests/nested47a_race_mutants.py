#!/usr/bin/env python3
"""Three isolated source regressions must produce real TSan data-race reports."""
import json,pathlib,resource,shutil,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n47a-race-mutants-'));print('N47A_RACE_MUTANT_EVIDENCE',out,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));results=[]
mutants=[
 ('pre_drain_local_scan','if(d->operations||d->ops)return true;','/* mutant: scan mutable owner fields before drain */'),
 ('released_local_health','m->state==TSR_RELEASED&&sr_ticket_eq(m->ticket,t)&&shared_healthy_locked(m->owners[0])&&shared_healthy_locked(m->owners[1])','m->state==TSR_RELEASED&&sr_ticket_eq(m->ticket,t)&&healthy_locked(m->owners[0])&&healthy_locked(m->owners[1])'),
 ('remote_registry_write','for(unsigned c=0;c<2;c++)m->owners[c]->bridge->directory.faulted=true;','for(unsigned c=0;c<2;c++){m->owners[c]->registry->poisoned=true;m->owners[c]->bridge->directory.faulted=true;}'),
]
for name,a,b in mutants:
 d=out/name;shutil.copytree(R/'krnl',d/'krnl');(d/'efi/tests').mkdir(parents=True)
 for src in (R/'efi/tests').glob('nested47a*'):shutil.copy2(src,d/'efi/tests'/src.name)
 f=d/'krnl/typed_root_source_replace.inc';s=f.read_text();assert s.count(a)==1,(name,s.count(a));f.write_text(s.replace(a,b));(d/'mutation.json').write_text(json.dumps({'before':a,'after':b},indent=2)+'\n')
 r=subprocess.run(['python3',str(d/'efi/tests/nested47a_core.py'),'--sanitizer','thread'],capture_output=True,text=True,timeout=100);log=r.stdout+r.stderr;(d/'run.log').write_text(log)
 assert r.returncode!=0 and 'WARNING: ThreadSanitizer: data race' in log and "('runtime failed', 66)" in log,(name,log)
 results.append({'mutant':name,'status':'TSAN_RACE_DETECTED','instrumented_exit':66});print('PASS N47A TSAN_RACE_DETECTED',name,flush=True)
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
