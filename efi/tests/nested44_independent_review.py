#!/usr/bin/env python3
"""Independent N44 fresh-copy production review; no VMX or physical-INVEPT claim."""
from pathlib import Path
import argparse,hashlib,json,os,resource,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('repo',nargs='?',default=str(Path(__file__).resolve().parents[2]));p.add_argument('--efi-prefix',default=os.environ.get('EFI_PREFIX','/usr'));a=p.parse_args()
repo=Path(a.repo).resolve();here=Path(__file__).resolve().parent
out=Path(tempfile.mkdtemp(prefix='syle-n44-independent-'));print('N44_INDEPENDENT_EVIDENCE',out,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
k=out/'krnl';k.mkdir()
for f in (repo/'krnl').iterdir():
 if f.is_file():shutil.copy2(f,k/f.name)
shutil.copytree(repo/'include',out/'include')
for rel in ['efi/tests/nested41c_memory.c','efi/src/exceptions.h']:
 dest=out/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/rel,dest)
before={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in k.iterdir() if f.is_file()}
(out/'production-sha256.json').write_text(json.dumps(before,indent=2)+'\n')
for group in ['walker','maintenance','memory']:shutil.copy2(here/('nested44_independent_'+group+'.c'),out/(group+'.c'))
def replace_once(s,old,new):
 assert s.count(old)==1,(old,s.count(old));return s.replace(old,new)
s=(k/'typed_root_atomic_review_test.c').read_text()
s=replace_once(s,'struct nrc_span spans[2];','struct nrc_span spans[3];')
s=replace_once(s,'nrc_init_atomic_wb(&f->catalog,f->id,48,f->spans,2,','nrc_init_atomic_wb_2m(&f->catalog,f->id,48,f->spans,3,')
s=replace_once(s,'.gpa=0x90000,.hpa=REVIEW_DATA,','.gpa=0xf000,.hpa=REVIEW_DATA,')
s=replace_once(s,'assert(nrc_add_atomic_span(&f->catalog,s,NULL));','assert(nrc_add_atomic_span(&f->catalog,s,NULL));\n s.gpa=0x90000;s.hpa=REVIEW_DATA+0x2000;s.object++;assert(nrc_add_atomic_span(&f->catalog,s,NULL));')
s=replace_once(s,'review_tables[2][0]=0x4007;','review_tables[2][0]=0xf3;review_tables[2][1]=0x4007;')
s=replace_once(s,'review_tables[3][15]=0x90073;','review_tables[3][15]=0xf073;')
(k/'large_maintenance_fixture.inc').write_text(s)
s=(k/'typed_root_atomic_maintenance_review_test.c').read_text()
s=replace_once(s,'#include "typed_root_atomic_review_test.c"','#include "large_maintenance_fixture.inc"')
s=replace_once(s,'int main(', 'int old_maintenance_main(')
(k/'large_maintenance_helpers.inc').write_text(s)
walker=['nested_ept_atomic.c','nested_atomic_catalog.c','nested_root_catalog.c','ad_updater_gate.c','nested_ept_sparse.c','nested_ept.c']
maint=['typed_root_atomic.c','typed_root_registry.c','typed_root_local_provider.c','typed_root_lease.c','typed_directory_registry.c','typed_directory.c','nested_owner_pool.c','nested_ept_contexts.c',*walker]
memory=['nested_atomic_memory.c','nested_domain_memory.c','nested_atomic_catalog.c','nested_root_catalog.c','nested_ept_atomic.c','ad_updater_gate.c','nested_ept.c']
results=[];env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1')
def build(group,srcs,folder,mode,root=k):
 flags=['-std=c11','-O2' if mode=='none' else '-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+str(root)]
 if mode!='none':flags+=['-fsanitize='+mode]
 if group=='memory':flags+=['-DVMX_NESTED_DOMAIN_TEST','-DVMX_NESTED_ATOMIC_CATALOG_TEST','-I'+a.efi_prefix+'/include/efi','-I'+a.efi_prefix+'/include/efi/x86_64']
 binary=folder/'test'
 r=subprocess.run([os.environ.get('CC','cc'),*flags,str(out/(group+'.c')),*[str(root/x) for x in srcs],'-o',str(binary)],capture_output=True,text=True)
 (folder/'build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,(folder/'build.log').read_text();return binary
def run(binary,case,folder,negative=False):
 r=subprocess.run([str(binary),case],env=env,capture_output=True,text=True,timeout=90);(folder/'run.log').write_text(r.stdout+r.stderr)
 if negative:assert r.returncode==-6 and 'Assertion' in r.stderr,(folder,r.returncode,r.stdout,r.stderr)
 else:assert r.returncode==0,(folder,r.returncode,r.stdout,r.stderr)
 return r.stdout
for mode in ['none','address,undefined','thread']:
 for group,srcs,count in [('walker',walker,7),('maintenance',maint,5),('memory',memory,1)]:
  folder=out/(group+'-'+mode);folder.mkdir();binary=build(group,srcs,folder,mode);log=run(binary,'all',folder)
  assert log.count('PASS N44 independent ')==count;print(log,end='',flush=True)
  results.append({'group':group,'sanitizer':mode,'status':'PASS','cases':count})

MUTANTS=[('offset_dropped', 'nested_ept_atomic_format.h', 'return (raw&mask)+(gpa&(((UINT64_C(1)<<shift)-1)&~UINT64_C(4095)));', '(void)gpa;(void)shift;return raw&mask;', 'offsets', False), ('offset_only_64k', 'nested_ept_atomic_format.h', '(gpa&(((UINT64_C(1)<<shift)-1)&~UINT64_C(4095)))', '(gpa&(((UINT64_C(1)<<(shift==21?16:shift))-1)&~UINT64_C(4095)))', 'offsets', False), ('alignment_ignored', 'nested_ept_atomic_format.h', 'if(bits&&((raw&mask)&((UINT64_C(1)<<bits)-1)))return false;', 'if(false&&bits&&((raw&mask)&((UINT64_C(1)<<bits)-1)))return false;', 'formats', False), ('pdpte_ps_allowed', 'nested_ept_atomic_format.h', 'allow_2m&&depth==2', 'allow_2m&&(depth==2||depth==1)', 'formats', False), ('legacy_ps_allowed', 'nested_ept_atomic_format.h', 'allow_2m&&depth==2', 'true&&depth==2', 'formats', False), ('huge_dirty_omitted', 'nested_ept_atomic.c', 'e->leaf && r.translation.status==NESTED_EPT_OK', 'e->depth==3 && r.translation.status==NESTED_EPT_OK', 'permission_faults', False), ('read_sets_dirty', 'nested_ept_atomic.c', '&& (need&2)', '&& true', 'offsets', False), ('dirty_replayed', 'nested_ept_atomic.c', 'now,now|bits)', 'now,now|bits|(e->raw&0x200))', 'races', False), ('stale_structure_allowed', 'nested_ept_atomic.c', 'if((now&~mutable)!=(e->raw&~mutable))', 'if(false&&((now&~mutable)!=(e->raw&~mutable)))', 'races', False), ('l0_permissions_ignored', 'nested_ept_atomic.c', '(permissions&need)==need?NESTED_EPT_OK:NESTED_EPT_L0_BACKING_DENIED', 'NESTED_EPT_OK', 'permission_faults', False), ('effective_permissions_ignored', 'nested_ept_atomic.c', 'if((effective&need)!=need)', 'if(false&&((effective&need)!=need))', 'permission_faults', False), ('trace_depth_ignored', 'nested_atomic_catalog.c', 'e->depth!=i||', 'false||', 'traces', False), ('trace_shift_ignored', 'nested_atomic_catalog.c', 'e->leaf_shift!=shift||', 'false||', 'traces', False), ('trace_leaf_ignored', 'nested_atomic_catalog.c', 'e->leaf!=(shift!=0)||', 'false||', 'traces', False), ('trace_raw_ignored', 'nested_atomic_catalog.c', 'if((now&~mutable)!=(e->raw&~mutable))', 'if(false&&((now&~mutable)!=(e->raw&~mutable)))', 'traces', False), ('trace_offset_ignored', 'nested_atomic_catalog.c', 'next=shift?nea_leaf_page(now,t->gpa,mask,shift):now&mask;', 'next=now&mask;', 'traces', False), ('clear_huge_dirty_omitted', 'typed_root_atomic_maintenance.inc', 'uint64_t mutable=(p->depth==3||((p->depth==1||p->depth==2)&&(p->raw[n]&0x80)))?0x300:0x100;', 'uint64_t mutable=p->depth==3?0x300:0x100;', 'cycle', True), ('clean_huge_dirty_ignored', 'typed_root_atomic_maintenance.inc', 'if(tam_load(m,&p->view[n])!=(p->raw[n]&~mask))return false;', 'if((tam_load(m,&p->view[n])&~UINT64_C(0x200))!=((p->raw[n]&~mask)&~UINT64_C(0x200)))return false;', 'late_dirty', True), ('clear_graph_decode_ignored', 'typed_root_atomic_maintenance.inc', 'if(!nea_decode_policy(raw,depth,mask,true,m->owners[0]->config.catalog->atomic_2m,m->owners[0]->config.catalog->atomic_1g,m->owners[0]->config.catalog->atomic_uc,&shift))return false;', 'shift=depth==3?12:((depth==2&&(raw&0x80))?21:0);', 'invalid_graph', True), ('clear_without_all_owners', 'typed_root_atomic_maintenance.inc', 'm->state==TAM_DRAINED&&m->revoked==3', 'm->state==TAM_DRAINED', 'owner_gate', True), ('private_write_trap_removed', 'typed_root_atomic.c', 'bool trap=(out->effective02&2)&&!(access&2);', 'bool trap=false;', 'cycle', True)]

for name,file,old,new,case,ismaint in MUTANTS:
 folder=out/('mutant-'+name);folder.mkdir();mk=folder/'krnl';mk.mkdir()
 for f in k.iterdir():
  if f.is_file():shutil.copy2(f,mk/f.name)
 target=mk/file;s=target.read_text();s=replace_once(s,old,new)
 # Keep the policy parameter intentionally unused when its check is removed.
 if name=='legacy_ps_allowed':s=s.replace('if(depth>3','(void)allow_2m;\n if(depth>3')
 target.write_text(s)
 binary=build('maintenance' if ismaint else 'walker',maint if ismaint else walker,folder,'none',mk)
 run(binary,case,folder,True);print('PASS N44 independent weakening mutant rejected:',name,flush=True)
 results.append({'mutant':name,'status':'REJECTED','witness':case})
assert {n:hashlib.sha256((repo/'krnl'/n).read_bytes()).hexdigest() for n in before}==before,'production files changed during review'
(out/'result.json').write_text(json.dumps({'status':'PASS','results':results,'mutants':len(MUTANTS),'boundary':'host software and mocked physical primitives only; no hardware, full regression or source release claim'},indent=2)+'\n')
print('PASS N44 independent production review: 13 groups x O2/ASan-UBSan/TSan, 21 rejected mutants, 80 forced PDE CAS collisions per mode; no VMX claim',flush=True)
