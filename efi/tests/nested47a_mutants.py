#!/usr/bin/env python3
"""Assertion-killed N47A mutants; edits only isolated copied production files."""
import hashlib,json,pathlib,resource,shutil,subprocess,tempfile
R=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n47a-mutants-'));print('N47A_MUTANT_EVIDENCE',out,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
mutants=[
 ('close_handoff_gap','nested47a_core.py',[('krnl/typed_root_source_replace.inc','if(state==TSR_BOUND)return o->config.consumer_stopped(o->config.context)||m->hooks.stopped(m->context,(unsigned)o->bridge->directory.owner,t);','if(state==TSR_BOUND)return o->config.consumer_stopped(o->config.context);')]),
 ('final_bank_guard_skipped','nested47a_core.py',[('krnl/typed_root_source_replace.inc','||!sr_sources_ready(m)||!m->hooks.complete(m->context,t)||!sr_sources_ready(m)', '||(!sr_sources_ready(m)&&false)||!m->hooks.complete(m->context,t)||(!sr_sources_ready(m)&&false)')]),
 ('nonce_seed','nested47a_successor.py',[('krnl/nested_root_catalog.c','n->next_nonce=old->next_nonce','n->next_nonce=0'),('krnl/ad_updater_gate.c','.next_nonce=g->next_nonce','.next_nonce=0')]),
 ('drain_skipped','nested47a_core.py',[('krnl/typed_root_source_replace.inc','wait=d->operations||d->ops','wait=(d->operations||d->ops)&&false')]),
 ('owner_invept_forged','nested47a_core.py',[('krnl/typed_root_local_provider.c','if(!p->invalidate(p->context,scope.ticket.full_eptp,&local))return false;','local=(struct tr_local_invalidation){scope.ticket.full_eptp,UINT64_C(1)<<p->owner,p->owner,true};')]),
 ('early_pin_drop','nested47a_core.py',[('krnl/typed_root_source_replace.inc','struct tsr_receipt parked={0};','for(size_t n=0;n<count;n++){m->old[c][n].slot->directory.item->pins=0;}struct tsr_receipt parked={0};')]),
 ('receipt_identity_skipped','nested47a_core.py',[('krnl/typed_root_source_replace.inc','return a.success&&a.owner==c&&sr_ticket_eq(a.ticket,m->ticket)&&sr_root_eq(a.root,r);','(void)a;(void)m;(void)c;(void)r;return true;')]),
 ('missing_owner_prepare','nested47a_core.py',[('krnl/typed_root_source_replace.inc','m->prepared==3','m->prepared==1'),('krnl/typed_root_source_replace.inc','for(unsigned c=0;c<2&&ok;c++)ok=sr_quiet','for(unsigned c=0;c<1&&ok;c++)ok=sr_quiet')]),
 ('late_abort_ignored','nested47a_core.py',[('krnl/typed_root_source_replace.inc','ok=sr_valid(m,t)&&m->state==TSR_COMMITTING&&healthy_locked(m->owners[0])&&healthy_locked(m->owners[1]);','ok=true;')]),
 ('failure_reopens','nested47a_core.py',[('krnl/typed_root_source_replace.inc','m->state=TSR_POISONED;','m->state=TSR_OPEN;')]),
]
results=[]
for name,runner,changes in mutants:
 d=out/name;shutil.copytree(R/'krnl',d/'krnl');(d/'efi/tests').mkdir(parents=True)
 for src in (R/'efi/tests').glob('nested47a*'):shutil.copy2(src,d/'efi/tests'/src.name)
 edits=[]
 for rel,a,b in changes:
  f=d/rel;s=f.read_text();assert s.count(a)==1,(name,rel,a,s.count(a));f.write_text(s.replace(a,b));edits.append({'file':rel,'before':a,'after':b,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
 (d/'mutation.json').write_text(json.dumps(edits,indent=2)+'\n')
 r=subprocess.run(['python3',str(d/'efi/tests'/runner),'--sanitizer','none'],capture_output=True,text=True,timeout=100)
 log=r.stdout+r.stderr;(d/'run.log').write_text(log)
 assert r.returncode!=0 and 'Assertion' in log and 'failed.' in log and 'error:' not in log,(name,log)
 results.append({'mutant':name,'status':'ASSERTION_KILLED','exit':r.returncode});print('PASS N47A ASSERTION_KILLED',name,flush=True)
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
