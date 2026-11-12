#!/usr/bin/env python3
"""Imported N46B independent adapter/controller assertions; no hardware claim."""
from pathlib import Path
import shutil,subprocess,os,hashlib,json,resource,tempfile,argparse
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
p=argparse.ArgumentParser();p.add_argument('repo',nargs='?',default=str(Path(__file__).resolve().parents[2]));a=p.parse_args()
root=Path(a.repo).resolve();out=Path(tempfile.mkdtemp(prefix='syle-n46b-independent-'));print('N46B_INDEPENDENT_EVIDENCE',out,flush=True)
copy=out/'source';copy.mkdir()
for x in ('memory','controller'):shutil.copy2(root/('efi/tests/nested46b_independent_'+x+'.c'),out/(x+'_review.c'))
for d in ('krnl','include','efi/tests','efi/src'):shutil.copytree(root/d,copy/d,dirs_exist_ok=True)
shutil.copy2(out/'memory_review.c',copy/'efi/tests/memory_review.c')
(out/'source-sha256.json').write_text(json.dumps({str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted((root/'krnl').rglob('*')) if f.is_file()},sort_keys=True,indent=2)+'\n')
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1');inc=os.environ.get('EFI_PREFIX','/usr')+'/include/efi'
for mode in ('none','address,undefined','thread'):
 for group in ('memory','controller'):
  folder=out/(group+'-independent-'+mode);folder.mkdir(exist_ok=True);flags=['-std=c11','-O2' if mode=='none' else '-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+str(copy/'krnl')]
  if mode!='none':flags+=['-fsanitize='+mode]
  if group=='memory':
   flags+=['-I'+inc,'-I'+inc+'/x86_64']+['-D'+d for d in ('VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST')]
   sources=[copy/'efi/tests/memory_review.c']+[copy/'krnl'/s for s in ('nested_atomic_memory.c','nested_domain_memory.c','nested_root_catalog.c','nested_ept.c')]
  else:sources=[out/'controller_review.c',copy/'krnl/nested_atomic_uc_provision.c']
  r=subprocess.run(['cc',*flags,*map(str,sources),'-o',str(folder/'test')],text=True,capture_output=True);(folder/'build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,(folder,r.stdout,r.stderr)
  r=subprocess.run([str(folder/'test')],text=True,capture_output=True,env=env,timeout=120);(folder/'run.log').write_text(r.stdout+r.stderr);print(r.stdout,end='',flush=True);assert r.returncode==0,(folder,r.returncode,r.stderr)

src=out/'source'
M=[
('receipt_nonce','controller','nested_atomic_uc_provision.c','a->nonce!=b->nonce||',''),
('receipt_epoch','controller','nested_atomic_uc_provision.c','a->type_epoch!=b->type_epoch||',''),
('receipt_complete','controller','nested_atomic_uc_provision.c','||!r.completed||','||false||'),
('receipt_ordinal','controller','nested_atomic_uc_provision.c','||r.action.ordinal!=q.ordinal||','||false||'),
('receipt_cpu','controller','nested_atomic_uc_provision.c','||r.action.cpu!=q.cpu||','||false||'),
('receipt_eptp','controller','nested_atomic_uc_provision.c','||r.action.eptp!=q.eptp||','||false||'),
('receipt_gate','controller','nested_atomic_uc_provision.c','(r.proofs&q.required_proofs)!=q.required_proofs','false'),
('missing_both_cpu','controller','nested_atomic_uc_provision.c','for(unsigned i=0;i<NAUC_CPUS;i++)if(!step','for(unsigned i=0;i<1;i++)if(!step'),
('missing_root','controller','nested_atomic_uc_provision.c','for(unsigned j=0;j<NAUC_ROOTS;j++)\n  if(!step','for(unsigned j=0;j<NAUC_ROOTS-1;j++)\n  if(!step'),
('missing_first_invept','controller','nested_atomic_uc_provision.c','!roots(c,f,x,NAUC_INVEPT_BEFORE)||','false||'),
('missing_second_invept','controller','nested_atomic_uc_provision.c','!roots(c,f,x,NAUC_INVEPT_AFTER)||','false||'),
('missing_second_cache','controller','nested_atomic_uc_provision.c','!both(c,f,x,NAUC_WBINVD_AFTER,0)||','false||'),
('owned_snapshot','memory','nested_atomic_memory.c','(snapshot&&v[i]!=m->owned[i])','(false&&snapshot&&v[i]!=m->owned[i])'),
('missing_last_root_audit','memory','nested_atomic_memory.c','for(unsigned r=0;r<5;r++){','for(unsigned r=0;r<4;r++){'),
('missing_last_slot_audit','memory','nested_atomic_memory.c','for(unsigned n=0;n<512;n++){','for(unsigned n=0;n<511;n++){'),
('missing_stage_epoch','memory','nested_atomic_memory.c','m->catalog.type_epoch==m->type_epoch;','true;'),
('missing_initial_retention','memory','nested_atomic_memory.c','d->atomic_catalog_retained=true;m->uc_staged=true;return true;','d->atomic_catalog_retained=false;m->uc_staged=true;return true;'),
('early_publication','memory','nested_atomic_memory.c','d->atomic_catalog_retained=true;m->uc_staged=true;return true;','d->atomic_catalog_retained=true;m->uc_staged=true;return nrc_publish(&m->catalog);'),
('missing_final_physical_audit','memory','nested_atomic_memory.c','return uc_stage_flags(m)&&m->uc_detached&&m->uc_installed&&owned(m,true)&&physical_maps(m,true)&&sources(m,true);','return uc_stage_flags(m)&&m->uc_detached&&m->uc_installed&&owned(m,true)&&sources(m,true);'),
]
results=[]
for name,group,file,old,new in M:
 folder=out/('mutant-'+name);copy=folder/'source';folder.mkdir(exist_ok=True);shutil.copytree(src,copy,dirs_exist_ok=True);p=copy/'krnl'/file;s=p.read_text();assert old in s
 if name not in ('missing_last_root_audit','missing_last_slot_audit'):assert s.count(old)==1,(name,s.count(old))
 p.write_text(s.replace(old,new,1))
 flags=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+str(copy/'krnl')]
 if group=='memory':
  flags+=['-I'+inc,'-I'+inc+'/x86_64']+['-D'+d for d in ('VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST')]
  sources=[copy/'efi/tests/memory_review.c']+[copy/'krnl'/s for s in ('nested_atomic_memory.c','nested_domain_memory.c','nested_root_catalog.c','nested_ept.c')]
 else:sources=[out/'controller_review.c',copy/'krnl/nested_atomic_uc_provision.c']
 r=subprocess.run(['cc',*flags,*map(str,sources),'-o',str(folder/'test')],text=True,capture_output=True);(folder/'build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,(folder,r.stdout,r.stderr)
 r=subprocess.run([str(folder/'test')],text=True,capture_output=True,timeout=120);(folder/'run.log').write_text(r.stdout+r.stderr);assert r.returncode==-6 and 'Assertion' in r.stderr,(folder,r.returncode,r.stdout,r.stderr)
 results.append({'name':name,'status':'COMPILED_ASSERTION_REJECTED','witness':group});print('PASS N46B mutant',name,flush=True)
(out/'mutants.json').write_text(json.dumps(results,indent=2)+'\n')

print('PASS N46B independent adapters/controller: three modes, 19 assertion-killed mutants; no hardware claim')
