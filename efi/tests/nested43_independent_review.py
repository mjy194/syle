#!/usr/bin/env python3
"""Independent host-only N43 core review on a private source snapshot.
No production edits, VMX execution, full regression or host changes.
Every weakening mutant must compile and fail a runtime assertion.
"""
import argparse,pathlib,subprocess,os,json,hashlib,resource,shutil,tempfile
p=argparse.ArgumentParser()
p.add_argument('repo',nargs='?',default='.')
p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='none')
p.add_argument('--efi-prefix',default=os.environ.get('EFI_PREFIX','/usr'))
p.add_argument('--skip-mutants',action='store_true')
a=p.parse_args();repo=pathlib.Path(a.repo).resolve()
root=pathlib.Path(tempfile.mkdtemp(prefix='syle-n43-independent-'))
print('N43_INDEPENDENT_EVIDENCE',root,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
for directory in ['krnl','include']:shutil.copytree(repo/directory,root/directory)
for name in ['efi/tests/nested41c_memory.c','efi/src/exceptions.h']:
 target=root/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/name,target)
fixture=(root/'krnl/nested_owner_atomic_maintenance_test.c').read_text()
assert fixture.count('int main(void)')==1
(root/'krnl/n43_fixture_base.inc').write_text(fixture.replace('int main(void)','int n42b_fixture_main(void)'))
fixture=(repo/'efi/tests/nested43_test.c').read_text()
assert fixture.count('int main(void)')==1
(root/'krnl/n43_tests_base.inc').write_text(fixture.replace('int main(void)','int candidate_fixture_main(void)'))
shutil.copy2(pathlib.Path(__file__).with_name('nested43_independent_review_test.c'),root/'krnl/review.c')
sanitizer=a.sanitizer;mutate=not a.skip_mutants;prefix=a.efi_prefix
sources='review.c nested_atomic_multiround.c nested_atomic_retrack.c nested_owner_atomic_maintenance.c nested_owner_atomic.c nested_atomic_memory.c nested_domain_memory.c nested_vcpu_policy.c nested_domain_dispatch.c nested_domain.c nested_vmx.c typed_root_atomic.c typed_root_registry.c typed_root_lease.c typed_root_local_provider.c typed_directory_registry.c typed_directory.c nested_atomic_catalog.c nested_root_catalog.c nested_ept_atomic.c ad_updater_gate.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c nested_ept.c'.split()
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-I'+str(root/'krnl'),'-I'+prefix+'/include/efi','-I'+prefix+'/include/efi/x86_64']
flags+=['-D'+s for s in ['VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST']]
link=['-pthread','-no-pie']
if sanitizer!='none':flags+=['-fsanitize='+sanitizer];link+=['-fsanitize='+sanitizer]
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';env['TSAN_OPTIONS']='halt_on_error=1'
out=root/sanitizer;out.mkdir(exist_ok=True)
cc=os.environ.get('CC','cc')
def run(cmd,name,timeout=90):
 with (out/name).open('w') as log:
  try: result=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,env=env,timeout=timeout)
  except subprocess.TimeoutExpired: return 124
 return result.returncode
objects=[]
for source in sources:
 obj=out/(source+'.o');rc=run([cc,*flags,'-c',str(root/'krnl'/source),'-o',str(obj)],source+'.build.log')
 assert rc==0,(source,(out/(source+'.build.log')).read_text());objects.append(obj)
def execute(name,objs):
 binary=out/name;assert run([cc,*link,*map(str,objs),'-o',str(binary)],name+'.link.log')==0
 return run([str(binary)],name+'.log')
rc=execute('baseline',objects);print('BASELINE',sanitizer,rc,flush=True)
assert rc==0,(out/'baseline.log').read_text()
mutants=[
 ('ignore_tam_nonce','nested_owner_atomic_maintenance.c',[('a.nonce==b.nonce&&','')]),
 ('fresh_rearm','nested_owner_atomic_maintenance.c',[('if(!reuse&&(r->ticket.nonce||r->committed_stamp.epoch||r->pending_item||r->failure!=NOAM_OK))return false;','')]),
 ('permit_restored_reuse','nested_owner_atomic_maintenance.c',[('(r->phase!=NOAM_FRESH&&r->phase!=NOAM_COMMITTED)','false'),('if(!reuse&&(r->ticket.nonce||r->committed_stamp.epoch||r->pending_item||r->failure!=NOAM_OK))return false;','')]),
 ('discard_pending_restore','nested_owner_atomic_maintenance.c',[('r->pending_item=p->item;r->phase=NOAM_RESTORING;','r->pending_item=0;r->phase=NOAM_RESTORING;')]),
 ('omit_inactive_invept','nested_owner_atomic_maintenance.c',[('if(!r->ops.invept(r->context,p->full_eptp))','if(p->item==r->saved_item&&!r->ops.invept(r->context,p->full_eptp))')]),
 ('ignore_round_tags','nested_atomic_multiround.c',[('return (g-1)*NAMR_TAG_STRIDE+(unsigned)command;','return (void)g,(unsigned)command;')]),
 ('old_release_is_commit','nested_atomic_multiround.c',[('bool committed=released>=g;','bool committed=released!=0;')]),
 ('omit_busy_guard','nested_atomic_multiround.c',[('!r->maintenance.owner_busy[0]&&!r->maintenance.owner_busy[1]','true')]),
 ('omit_operation_guard','nested_atomic_multiround.c',[('!r->maintenance.operations[0].live&&!r->maintenance.operations[1].live&&',''),('!r->maintenance.operation_nonce[0]&&!r->maintenance.operation_nonce[1]&&','')]),
 ('omit_completion_barrier','nested_atomic_multiround.c',[('return wait_value(m,&m->completed[c^1],g)?true:fail(m,g);','return true;')]),
 ('omit_peer_consumed','nested_atomic_multiround.c',[('acquire(&m->completed[c^1])==g-1&&','')]),
 ('publish_epoch_early','nested_atomic_multiround.c',[('if(!noam_ticket_equal(t0,t1)||','r->outer_stamp->epoch++;\n if(!noam_ticket_equal(t0,t1)||')]),
 ('reset_nonce','typed_root_atomic.c',[]),
 ('premature_evidence_reset','nested_atomic_multiround.c',[('lock(m);\n bool admitted=', 'r->ticket=(struct tam_ticket){0};\n lock(m);\n bool admitted=')]),
 ('accept_prior_receipt','typed_root_atomic.c',[]),
 ('ignore_bound_tag','nested_atomic_multiround.c',[('acquire(&m->bound[c])!=g||','')]),
 ('ignore_round_overflow','nested_atomic_multiround.c',[('if(!r||c>1||!g||g>NAMR_MAX_GENERATION||','if(!r||c>1||!g||')]),
 ('ignore_bound_tickets','nested_atomic_multiround.c',[('noam_ticket_equal(e->ticket,t)&&domain_equal(e->domain_ticket,r->domain_ticket)&&','noam_ticket_equal(t,t)&&domain_equal(r->domain_ticket,r->domain_ticket)&&'),('e->token,e->domain_ticket);','e->token,r->domain_ticket);')]),
 ('ignore_prepare_ack','nested_atomic_multiround.c',[('acquire(&m->ack[0])!=tag(g,NART_PREPARE)||',''),('acquire(&m->ack[1])!=tag(g,NART_PREPARE)||','')]),
]
results={}
if mutate:
 for name,source,edits in mutants:
  if name in ('reset_nonce','accept_prior_receipt'):
   inc=root/'krnl/typed_root_atomic_maintenance.inc';orig=inc.read_text()
   if name=='reset_nonce':
    old='++m->next_nonce';assert orig.count(old)==1;inc.write_text(orig.replace(old,'(m->next_nonce=1)'))
   else:
    changed=orig
    for old,new in [('got.success&&tam_ticket_eq(got.ticket,call->ticket)&&','got.success&&'),('got.old_generation==r->old_generation&&got.new_generation==r->new_generation&&','')]:
     assert changed.count(old)==1;changed=changed.replace(old,new)
    inc.write_text(changed)
   changed=(root/'krnl'/source).read_text()
  else:
   original=(root/'krnl'/source).read_text();changed=original
   for old,new in edits:assert changed.count(old)==1,(name,old,changed.count(old));changed=changed.replace(old,new)
  path=root/'krnl'/('mutant_'+source);path.write_text(changed);obj=out/(name+'.o')
  rc=run([cc,*flags,'-c',str(path),'-o',str(obj)],name+'.build.log')
  if name in ('reset_nonce','accept_prior_receipt'):inc.write_text(orig)
  assert rc==0,(name,'compile',(out/(name+'.build.log')).read_text())
  objs=objects.copy();objs[sources.index(source)]=obj;rc=execute(name,objs);results[name]=rc
  assert rc not in (0,124),(name,'survived/timeout',rc)
  log=(out/(name+'.log')).read_text();assert 'Assertion' in log,(name,'not runtime assertion',rc,log)
  print('REJECTED',name,rc,flush=True)
manifest={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'krnl').glob('*') if p.is_file() and not p.name.startswith('mutant_')}
(out/'source-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
(out/'results.json').write_text(json.dumps({'baseline':0,'sanitizer':sanitizer,'mutants':results},indent=2)+'\n')

print('PASS independent N43 core review; sanitizer='+sanitizer+'; host primitives mocked; no VMX claim',flush=True)
