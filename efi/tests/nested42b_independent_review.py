#!/usr/bin/env python3
"""Host-only independent N42B review. Copies input, never edits production.
No VMX, full regression, host changes, or hardware acceptance are performed.
Each weakening mutant must compile and fail the same runtime assertions.
"""
import argparse, hashlib, json, os, pathlib, resource, shutil, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('repo',nargs='?',default='.');p.add_argument('--efi-prefix',default=os.environ.get('EFI_PREFIX','/usr'));p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='none');p.add_argument('--skip-mutants',action='store_true');a=p.parse_args()
repo=pathlib.Path(a.repo).resolve();out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n42b-independent-'))
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
for d in ['krnl','include']:shutil.copytree(repo/d,out/d)
for f in ['efi/tests/nested41c_memory.c','efi/src/exceptions.h']:
 t=out/f;t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/f,t)
fixture=(out/'krnl/nested_owner_atomic_maintenance_test.c').read_text();needle='int main(void)';assert fixture.count(needle)==1
(out/'krnl/n42b_fixture_base.inc').write_text(fixture.replace(needle,'int n42b_fixture_main(void)',1))
shutil.copy2(pathlib.Path(__file__).with_name('nested42b_independent_review_test.c'),out/'krnl/review.c')
sources='review.c nested_atomic_retrack.c nested_owner_atomic_maintenance.c nested_owner_atomic.c nested_atomic_memory.c nested_domain_memory.c nested_vcpu_policy.c nested_domain_dispatch.c nested_domain.c nested_vmx.c typed_root_atomic.c typed_root_registry.c typed_root_lease.c typed_root_local_provider.c typed_directory_registry.c typed_directory.c nested_atomic_catalog.c nested_root_catalog.c nested_ept_atomic.c ad_updater_gate.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c nested_ept.c'.split()
cc=os.environ.get('CC','cc');flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-I'+str(out/'krnl'),'-I'+a.efi_prefix+'/include/efi','-I'+a.efi_prefix+'/include/efi/x86_64']
flags += ['-D'+s for s in ['VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST']]
link=['-pthread','-no-pie']
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer];link+=['-fsanitize='+a.sanitizer]
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';env['TSAN_OPTIONS']='halt_on_error=1'
manifest={str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (out/'krnl').glob('*') if f.is_file()}
(out/'source-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
def run(cmd,log,timeout=90):
 with (out/log).open('w') as f:
  try:r=subprocess.run(cmd,cwd=out,stdout=f,stderr=subprocess.STDOUT,env=env,timeout=timeout);return r.returncode
  except subprocess.TimeoutExpired:return 124
objects=[]
for s in sources:
 obj=out/(s+'.o');assert run([cc,*flags,'-c',str(out/'krnl'/s),'-o',str(obj)],s+'.build.log')==0,'compile failed: '+s;objects.append(obj)
def execute(name,objs):
 binary=out/name;assert run([cc,*link,*map(str,objs),'-o',str(binary)],name+'.link.log')==0,'link failure'
 return run([str(binary)],name+'.log')
print('Evidence:',out,flush=True);assert execute('baseline',objects)==0,'baseline runtime failed';print('PASS independent baseline',flush=True)
mutants=[
 ('ignore_tam_nonce','nested_owner_atomic_maintenance.c',[('a.nonce==b.nonce&&','')]),
 ('ignore_owner_cpu','nested_owner_atomic_maintenance.c',[('a->config.current_cpu(a->config.context)==a->sparse->pool.owner&&','')]),
 ('ignore_busy','nested_owner_atomic_maintenance.c',[('!a->vcpu->busy&&','')]),
 ('ignore_local_launch','nested_owner_atomic_maintenance.c',[('*r->local_launched02==r->launched02&&','')]),
 ('ignore_reserved_tam_ticket','nested_owner_atomic_maintenance.c',[('r->maintenance->state==TAM_STOPPING&&noam_ticket_equal(t,r->maintenance->ticket)','true')]),
 ('ignore_vmcs01_reference','nested_owner_atomic_maintenance.c',[('||eptp!=r->parking_eptp)return fail(r,NOAM_READ01,eptp);',')return fail(r,NOAM_READ01,eptp);'),('if((eptp&ADDRESS)==r->maintenance->roots[c][n].hpa)','if(false&&(eptp&ADDRESS)==r->maintenance->roots[c][n].hpa)')]),
 ('ignore_parking_write_failure','nested_owner_atomic_maintenance.c',[('if(!r->ops.write_eptp(r->context,r->parking_eptp))return fail(r,NOAM_WRITE_PARK,0);','(void)r->ops.write_eptp(r->context,r->parking_eptp);')]),
 ('ignore_restore_write_failure','nested_owner_atomic_maintenance.c',[('if(!r->ops.write_eptp(r->context,p->full_eptp))return fail(r,NOAM_WRITE_RESTORE,0);','(void)r->ops.write_eptp(r->context,p->full_eptp);')]),
 ('discard_pending_restore','nested_owner_atomic_maintenance.c',[('r->pending_item=p->item;r->phase=NOAM_RESTORING;','r->pending_item=0;r->phase=NOAM_RESTORING;')]),
 ('omit_exact_invept','nested_owner_atomic_maintenance.c',[('if(!r->ops.invept(r->context,p->full_eptp))return fail(r,NOAM_INVEPT,p->full_eptp);','/* removed exact INVEPT */')]),
 ('omit_inactive_invept','nested_owner_atomic_maintenance.c',[('if(!r->ops.invept(r->context,p->full_eptp))','if(p->item==r->saved_item&&!r->ops.invept(r->context,p->full_eptp))')]),
 ('ignore_domain_ticket_epoch','nested_domain_dispatch.c',[('ticket.epoch==d->maintenance_epoch&&ticket.epoch&&\n  ticket.purpose==NESTED_DOMAIN_AD_RESET','ticket.epoch&&\n  ticket.purpose==NESTED_DOMAIN_AD_RESET')]),
 ('admit_changed_token_generation','nested_domain_dispatch.c',[('d->objects[t.slot].used&&d->objects[t.slot].generation==t.generation&&','d->objects[t.slot].used&&'),('t.slot==a->slot&&t.generation==a->generation&&\n  t.domain_id==d->id','t.slot==a->slot&&\n  t.domain_id==d->id')]),
 ('publish_outer_stamp_early','nested_atomic_retrack.c',[('if(!tam_close(&r->maintenance,&r->ticket))','r->outer_stamp->epoch++;\n if(!tam_close(&r->maintenance,&r->ticket))')]),
 ('writeable_read_retrack','typed_root_atomic.c',[('bool trap=(out->effective02&2)&&!(access&2);','bool trap=false;')]),
 ('ignore_current_vmcs_identity','nested_owner_atomic_maintenance.c',[('current!=r->vmcs02','false',2)]),
 ('select_before_empty_binding','nested_owner_atomic.c',[('(a->empty_ready&&!a->atomic.maintenance&&!a->atomic.replacement)||','')]),
 ('ignore_successful_readback_mismatch','nested_owner_atomic_maintenance.c',[('||eptp!=expected)return fail(r,why,eptp);','||((void)expected,false))return fail(r,why,eptp);')]),
 ('unpin_before_verified_parking','nested_owner_atomic_maintenance.c',[('r->phase=NOAM_REMOVING;', '''r->phase=NOAM_REMOVING;
 struct tr_atomic_owner *early=&r->owner->atomic;
 early->config.lock(early->config.context);
 for(size_t k=0;k<count;k++){
  struct tam_root *p=&r->maintenance->roots[c][k];
  if(p->item==r->saved_item&&td_unpin(early->bridge->directory.directory,
    &r->maintenance->operations[c],r->maintenance->operation_nonce[c],
    &p->binding->slot->directory.record,p->identity))p->item->pins--;
 }
 early->config.unlock(early->config.context);''')]),
 ('release_inside_open_entry','nested_atomic_retrack.c',[('return nested_domain_dispatch_maintenance_publish(r->dispatch,r->domain_ticket)&&','release(&r->released,1);\n return nested_domain_dispatch_maintenance_publish(r->dispatch,r->domain_ticket)&&')]),
 ('omit_final_commit_guard','nested_atomic_retrack.c',[('bool commit=healthy(r)&&r->maintenance.state==TAM_OPEN;','bool commit=true;')]),
]
results={}
if not a.skip_mutants:
 for name,source,edits in mutants:
  original=(out/'krnl'/source).read_text();changed=original
  for edit in edits:
   old,new,*expected=edit;count=expected[0] if expected else 1
   assert changed.count(old)==count,(name,'source drift',old,changed.count(old));changed=changed.replace(old,new,count)
  mutation=out/'krnl'/('mutant_'+source);mutation.write_text(changed);obj=out/(name+'.o')
  assert run([cc,*flags,'-c',str(mutation),'-o',str(obj)],name+'.build.log')==0,'mutant did not compile: '+name
  objs=objects.copy();objs[sources.index(source)]=obj;result=execute(name,objs);results[name]=result
  assert result not in (0,124),'mutant survived or timed out: '+name
  print('REJECTED',name,'exit',result,flush=True)
(out/'results.json').write_text(json.dumps({'baseline':0,'sanitizer':a.sanitizer,'mutants':results},indent=2)+'\n')
print('PASS N42B independent host review; privileged primitives mocked; no VMX claim',flush=True)
