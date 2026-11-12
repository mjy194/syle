#!/usr/bin/env python3
"""Independent N41C production-slice review, host mocks only, no VMX claim."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
K=ROOT/'krnl'
EFI=Path(os.environ.get('EFI_INC',str(Path(os.environ.get('EFI_PREFIX','/usr'))/'include/efi')))
if not (EFI/'efi.h').is_file() and Path('/workspace/shared/qemu-deps/root/usr/include/efi/efi.h').is_file():
 EFI=Path('/workspace/shared/qemu-deps/root/usr/include/efi')
BASE=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined',
 '-I'+str(K),'-I'+str(EFI),'-I'+str(EFI/'x86_64')]
COMMON=['typed_root_atomic.c','typed_root_registry.c','typed_root_lease.c','typed_root_local_provider.c',
 'typed_directory_registry.c','typed_directory.c','nested_atomic_catalog.c','nested_root_catalog.c',
 'nested_ept_atomic.c','ad_updater_gate.c','nested_owner_pool.c','nested_ept_contexts.c','nested_ept_sparse.c','nested_ept.c']
DISPATCH=['nested_domain_dispatch.c','nested_domain.c','nested_vmx.c']
SUITES={
 'entry':(['typed_root_atomic_entry_review_test.c',*COMMON],[]),
 'dispatch':(['nested_atomic_dispatch_review_test.c',*DISPATCH],[]),
 'fixture':(['nested_atomic_fixture_review_test.c','ept.c','nested_domain_memory.c','nested_atomic_memory.c','nested_root_catalog.c','nested_ept.c'],['EPT_HOST_TEST','VMX_NESTED_TEST','VMX_NESTED_DOMAIN_TEST']),
 'adapter':(['nested_owner_atomic_review_test.c','nested_owner_atomic.c','nested_atomic_memory.c','nested_domain_memory.c','nested_vcpu_policy.c',*DISPATCH,*COMMON],
  ['VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST']),
}
# Each mutation must compile and then fail a semantic assertion. A compiler
# rejection is not credited as test sensitivity. All mutants stay temporary.
MUTANTS=[
 ('entry','late','typed_root_atomic.c','skip_final_entry_revalidation',[
  ('if(ok)ok=healthy_locked(o)&&(admission_locked(o)||cancelled)&&!d->closed&&!d->sealed','if(ok&&o->replacement)ok=healthy_locked(o)&&(admission_locked(o)||cancelled)&&!d->closed&&!d->sealed',1)]),
 ('entry','corruption','typed_root_atomic.c','ignore_actual_entry_eptp',[
  ('eptp!=item->tree.eptp','eptp==UINT64_MAX',1),('eptp==item->tree.eptp&&','eptp!=UINT64_MAX&&',1)]),
 ('entry','positive','typed_root_atomic.c','omit_entry_residency',[
  ('(!resident||td_resident(d,&o->entry_operation,o->entry_nonce,&a->slot->directory.record,\n    a->slot->directory.identity,(unsigned)b->directory.owner))','true',1)]),
 ('entry','late','typed_root_atomic.c','forget_failed_entry_cleanup',[
  ('if(ended){o->entry_active=false;o->entry_nonce=0;}','{o->entry_active=false;o->entry_nonce=0;}',1)]),
 ('entry','positive','typed_root_atomic.c','reject_live_peer_updater',[
  ('ad_updater_gate_open(o->config.gate,o->config.stamp)','ad_updater_gate_idle(o->config.gate,o->config.stamp)',2)]),
 ('adapter','gates','nested_owner_atomic.c','ignore_busy_physical_consumer',[
  ('!a->vcpu->busy&&!a->vcpu->poison','!a->vcpu->poison',1)]),
 ('adapter','gates','nested_owner_atomic.c','ignore_physical_ad_capability',[
  ('!a->failed&&a->physical_ad&&','!a->failed&&',1)]),
 ('adapter','gates','nested_owner_atomic.c','ignore_actual_vmcs',[
  ('vmcs!=(l2?a->vcpu->vmcs02:a->vcpu->vmcs01)','vmcs==UINT64_MAX',1)]),
 ('adapter','gates','nested_owner_atomic.c','ignore_shared_failure_latch',[
  ('(!a->healthy||a->healthy(a->healthy_context))&&','',1)]),
 ('adapter','qualification','nested_owner_atomic.c','ignore_qualification_actual_permissions',[
  ('actual!=((q>>3)&7)','actual>7',1)]),
 ('adapter','qualification','nested_owner_atomic.c','allow_impossible_gla_validity',[
  ('if((q&0x100)&&!(q&0x80))','if(false)',1)]),
 ('adapter','ignored','nested_owner_atomic.c','reject_undefined_mbec_bit',[
  ('if(!(q&7))','if((q&0x40)||!(q&7))',1)]),
 ('adapter','ignored','nested_owner_atomic.c','reject_undefined_pagewalk_metadata',[
  ('if(!(q&7))','if(((q&0xe00)&&(q&0x180)!=0x180)||!(q&7))',1)]),
 ('adapter','ignored','nested_owner_atomic.c','reject_undefined_shadowstack_bit',[
  ('if(!(q&7))','if((q&0x4000)||!(q&7))',1)]),
 ('adapter','ignored','nested_owner_atomic.c','reject_unassigned_high_bits',[
  ('if(!(q&7))','if((q&~UINT64_C(0x1ffff))||!(q&7))',1)]),
 ('adapter','ignored','nested_owner_atomic.c','reject_undefined_nmi_bit',[
  ('((q&0x1000)&&(!(pin&8)||(pin&32)))','(q&0x1000)',1)]),
 ('adapter','diagnostic','nested_owner_atomic.c','ignore_meaningful_nmi_bit',[
  ('((q&0x1000)&&(!(pin&8)||(pin&32)))','false',1)]),
 ('adapter','diagnostic','nested_owner_atomic.c','allow_unsupported_event',[
  ('(q&UINT64_C(0x1a000))','false',1)]),
 ('adapter','qualification','nested_owner_atomic.c','omit_pagewalk_rw_guard',[
  ('if((q&0x80)&&!(q&0x100)&&(q&3)!=3)','if(false)',1)]),
 ('adapter','diagnostic','nested_owner_atomic.c','lose_raw_qualification',[
  ('a->rejection.qualification=q;','a->rejection.qualification=q&0x1ff;',1)]),
 ('adapter','diagnostic','nested_owner_atomic.c','overwrite_first_rejection',[
  ('if(!a||__atomic_load_n(&a->rejection.valid,__ATOMIC_ACQUIRE))return;','if(!a)return;',1)]),
 ('adapter','diagnostic','nested_owner_atomic.c','lose_bridge_status',[
  ('NOA_REJECT_BRIDGE,true,actual,status','NOA_REJECT_BRIDGE,true,actual,-1',1)]),
 ('adapter','authority','nested_atomic_memory.c','ignore_host_writable_authority',[
  ('paging_page_rw_wb(h)&&','',1)]),
 ('adapter','failures','typed_root_atomic.c','ignore_source_release_failure',[
  ('if(!pin_guard(o,a)||!nrc_end(&a->pin,a->pin_nonce))return false;',
   '(void)pin_guard(o,a);(void)nrc_end(&a->pin,a->pin_nonce);',1)]),
 ('fixture','all','nested_atomic_memory.c','confuse_owned_started_with_running',[
  ('||!d->backing.started)return false;','||d->backing.started)return false;',1)]),
]
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
EVIDENCE_TEMP=None
if os.environ.get('N41C_REVIEW_EVIDENCE'):
 EVIDENCE=Path(os.environ['N41C_REVIEW_EVIDENCE']);EVIDENCE.mkdir(parents=True,exist_ok=False)
else:
 EVIDENCE_TEMP=tempfile.TemporaryDirectory(prefix='syle-n41c-review-evidence-');EVIDENCE=Path(EVIDENCE_TEMP.name)

def compile_test(suite,out,replace=None):
 sources,macros=SUITES[suite];replace=replace or {}
 subprocess.run(['gcc',*BASE,*['-D'+x for x in macros],
  *[str(replace.get(s,K/s)) for s in sources],'-o',str(out)],check=True)

def run(binary,case='all'):
 return subprocess.run([str(binary),case],env=ENV,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)

paths={K/s for sources,_ in SUITES.values() for s in sources}
paths.update(K.glob('*.h'))
paths.update([K/'typed_root_atomic_review_test.c',K/'tests/nested_domain_dispatch.c',ROOT/'efi/tests/nested41c_memory.c',Path(__file__),
 K/'typed_root_atomic_maintenance.inc',K/'nested_atomic_runtime.inc',K/'nested_guest.c',K/'smp_nested.inc',K/'nested_atomic_catalog_l1.inc',K/'nested_atomic_catalog_l2.inc',K/'paging.c'])
def hashes():return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}
before=hashes();results=[]
with tempfile.TemporaryDirectory(prefix='syle-n41c-review-build-') as temporary:
 tmp=Path(temporary)
 for name in SUITES:
  binary=tmp/name;compile_test(name,binary);result=run(binary)
  (EVIDENCE/(name+'.log')).write_text(result.stdout);print(result.stdout,end='',flush=True)
  if result.returncode:raise SystemExit(f'{name} production review failed: {result.returncode}')
  results.append({'suite':name,'status':'PASS'})
 for suite,case,source,name,edits in MUTANTS:
  changed=(K/source).read_text()
  for old,new,count in edits:
   if changed.count(old)!=count:raise SystemExit(f'{name}: expected {count} mutation seams')
   changed=changed.replace(old,new)
  source_path=tmp/(name+'.c');source_path.write_text(changed);binary=tmp/name
  compile_test(suite,binary,{source:source_path});result=run(binary,case)
  (EVIDENCE/(name+'.log')).write_text(result.stdout)
  if result.returncode!=-6 or 'Assertion' not in result.stdout:
   print(result.stdout,end='');raise SystemExit(f'{name}: no semantic assertion witness ({result.returncode})')
  print('PASS N41C independent weakening mutant rejected:',name,flush=True)
  results.append({'suite':suite,'case':case,'mutant':name,'status':'REJECTED'})
after=hashes()
if after!=before:raise SystemExit('review/production source changed during independent review')
(EVIDENCE/'result.json').write_text(json.dumps({'status':'PASS','boundary':'Host mocks only; no VMX/physical INVEPT/full regression claim',
 'suites':len(SUITES),'mutants':len(MUTANTS),'results':results,'sha256':before},indent=2)+'\n')
print(f'PASS independent N41C {len(SUITES)} production slices, {len(MUTANTS)} rejected weakening mutants, O2/Werror/ASan/UBSan; {len(before)} unchanged files',flush=True)
if EVIDENCE_TEMP:EVIDENCE_TEMP.cleanup()
