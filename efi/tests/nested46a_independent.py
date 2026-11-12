#!/usr/bin/env python3
"""Reproducible N46A independent tests/mutants, exact imported assertions. No VMX."""
from pathlib import Path
import shutil,hashlib,json,subprocess,os,resource,tempfile,argparse
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
p=argparse.ArgumentParser();p.add_argument('repo',nargs='?',default=str(Path(__file__).resolve().parents[2]));a=p.parse_args()
root=Path(a.repo).resolve();out=Path(tempfile.mkdtemp(prefix='syle-n46a-independent-'));print('N46A_INDEPENDENT_EVIDENCE',out,flush=True)
k=out/'krnl';shutil.copytree(root/'krnl',k)
shutil.copy2(root/'efi/tests/nested46a_independent.c',out/'review.c')
(out/'source-sha256.json').write_text(json.dumps({str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted((root/'krnl').rglob('*')) if f.is_file()},sort_keys=True,indent=2)+'\n')
sources='nested_ept_atomic.c nested_atomic_catalog.c nested_root_catalog.c ad_updater_gate.c nested_ept_sparse.c nested_ept.c'.split()
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1')
def build(folder,mode,k=k):
 folder.mkdir(exist_ok=True);flags=['-std=c11','-O2' if mode=='none' else '-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+str(k)]
 if mode!='none':flags+=['-fsanitize='+mode]
 p=subprocess.run(['cc',*flags,str(out/'review.c'),*[str(k/s) for s in sources],'-o',str(folder/'test')],text=True,capture_output=True);(folder/'build.log').write_text(p.stdout+p.stderr);assert p.returncode==0,(folder/'build.log').read_text();return folder/'test'
def run(binary,case,folder,negative=False):
 p=subprocess.run([str(binary),case],text=True,capture_output=True,env=env,timeout=90);(folder/'run.log').write_text(p.stdout+p.stderr)
 if negative:assert p.returncode==-6 and 'Assertion' in p.stderr,(folder,p.returncode,p.stdout,p.stderr)
 else:assert p.returncode==0,(folder,p.returncode,p.stdout,p.stderr)
 return p.stdout
for mode in ('none','address,undefined','thread'):
 folder=out/('independent-'+mode);b=build(folder,mode);print(run(b,'all',folder),end='',flush=True)

M=[
('uc_source_execute','nested_ept_atomic_format.h',[('fixed_uc&&type==0&&!(raw&4)','fixed_uc&&type==0')],'formats'),
('uc_policy_huge_flags','nested_ept_atomic_format.h',[('(fixed_uc&&(!ipat||allow_2m||allow_1g))||','')],'formats'),
('walker_type_mismatch','nested_ept_atomic.c',[('||type!=leaf_type||','||false||')],'denials'),
('authority_type_mismatch','nested_root_catalog.c',[('p.backing.memory_type!=(c->atomic_uc&&gpa==c->uc_gpa?0:6)||','false||'),('span->allowed_types!=(1u<<p.backing.memory_type)||','false||')],'mappings'),
('authority_hpa','nested_root_catalog.c',[('p.backing.hpa!=span->hpa+offset||','false||')],'mappings'),
('authority_lifetime','nested_root_catalog.c',[('p.backing.lifetime!=span->lifetime||','false||')],'mappings'),
('policy_epoch','nested_atomic_catalog.c',[('return a.epoch==b.epoch&&a.uc_gpa','return a.uc_gpa')],'traces'),
('policy_gpa','nested_atomic_catalog.c',[('&&a.uc_gpa==b.uc_gpa','')],'traces'),
('policy_hpa','nested_atomic_catalog.c',[('&&a.uc_hpa==b.uc_hpa','')],'traces'),
('policy_post_callback','nested_atomic_catalog.c',[('  policy_equal(p->type_policy,type_policy(p->source.catalog))&&','  true&&')],'traces'),
('constructor_zero_epoch','nested_root_catalog.c',[('if(!epoch||bits<12','if(bits<12')],'constructor'),
('sparse_ad0_optin','nested_ept_sparse.c',[('if(!t||!t->hardware_ad||!nested_ept_sparse_set_typed_admission','if(!t||!nested_ept_sparse_set_typed_admission')],'sparse'),
('sparse_missing_optin','nested_ept_sparse.c',[('t->typed_atomic_uc&&r->leaf_type==0','true&&r->leaf_type==0')],'sparse'),
('sparse_uc_exec_source','nested_ept_sparse.c',[('&&!(r->effective12&4)','')],'sparse'),
('sparse_uc_pat0','nested_ept_sparse.c',[('(!r->ignore_pat&&!t->typed_allow_pat)','false'),('r->leaf_type==0&&r->ignore_pat&&','r->leaf_type==0&&true&&')],'sparse'),
('read_sets_dirty','nested_ept_atomic.c',[('&& (need&2)','&& true')],'mappings'),
('trace_raw','nested_atomic_catalog.c',[('if((now&~mutable)!=(e->raw&~mutable))','if(false&&((now&~mutable)!=(e->raw&~mutable)))')],'core'),
('private_write_trap','typed_root_atomic.c',[('bool trap=(out->effective02&2)&&!(access&2);','bool trap=false;')],'core'),
('tam_type_mismatch','typed_root_atomic_maintenance.inc',[('||leaf.backing.memory_type!=((raw>>3)&7)','')],'core'),
('tam_decode','typed_root_atomic_maintenance.inc',[('if(!nea_decode_policy(raw,depth,mask,true,m->owners[0]->config.catalog->atomic_2m,m->owners[0]->config.catalog->atomic_1g,m->owners[0]->config.catalog->atomic_uc,&shift))return false;','(void)mask;shift=depth==3?12:0;')],'core'),
('target_extent','nested_root_catalog.c',[('||s.bytes!=4096','')],'core'),
('target_nonexec','nested_root_catalog.c',[('||(s.permissions&4)||','||false||')],'core'),
('foreign_uc_span','nested_root_catalog.c',[('(s.allowed_types!=NRC_TYPE_WB||overlap','(false||overlap')],'constructor'),
('missing_source_epoch_binding','typed_root_atomic.c',[('c->type_epoch==l->type_epoch&&','')],'core'),
]
core_sources='typed_root_atomic.c typed_root_registry.c typed_root_local_provider.c typed_root_lease.c typed_directory_registry.c typed_directory.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c nested_ept.c nested_atomic_catalog.c nested_root_catalog.c nested_ept_atomic.c ad_updater_gate.c'.split()
results=[]
for name,file,changes,case in M:
 folder=out/('mutant-'+name);folder.mkdir(exist_ok=True);mk=folder/'krnl';shutil.copytree(k,mk,dirs_exist_ok=True)
 p=mk/file;s=p.read_text()
 for old,new in changes:
  assert s.count(old)==1,(name,old,s.count(old));s=s.replace(old,new)
 p.write_text(s)
 if case=='core':
  (mk/'n46a_fixture_base.inc').write_text((mk/'typed_root_atomic_maintenance_test.c').read_text().replace('int main(void)','int n42a_fixture_main(void)'))
  shutil.copy2(root/'efi/tests/nested46a_core_test.c',folder/'core.c')
  b=folder/'test';p=subprocess.run(['cc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+str(mk),str(folder/'core.c'),*[str(mk/s) for s in core_sources],'-o',str(b)],text=True,capture_output=True);(folder/'build.log').write_text(p.stdout+p.stderr);assert p.returncode==0,(name,p.stdout,p.stderr)
 else:b=build(folder,'none',mk)
 run(b,case,folder,True);results.append({'mutant':name,'status':'ASSERTION_REJECTED','witness':case});print('PASS mutant',name,flush=True)
(out/'mutants.json').write_text(json.dumps(results,indent=2)+'\n')

print('PASS N46A independent: 8 groups x3 modes, 24 assertion-killed mutants; no physical UC claim')
