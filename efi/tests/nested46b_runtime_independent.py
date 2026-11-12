#!/usr/bin/env python3
"""Exact imported N46B independent cache/entry/runtime assertions and mutants.
Only ROOT/OUT and input materialization differ from the independent runners.
No privileged instruction or physical guest runs here.
"""
from pathlib import Path
import argparse,hashlib,json,os,resource,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('repo',nargs='?',default=str(Path(__file__).resolve().parents[2]));a=p.parse_args()
ROOT=Path(a.repo).resolve();OUT=Path(tempfile.mkdtemp(prefix='syle-n46b-runtime-independent-'))
print('N46B_RUNTIME_INDEPENDENT_EVIDENCE',OUT,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
for old,new in [('nested46b_independent_primitives.c','primitives.c'),('nested46b_independent_runtime_extra.c','runtime_extra.c')]:
 shutil.copy2(ROOT/'efi/tests'/old,OUT/new)
manifest={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for sub in ('krnl','include','efi/tests','efi/src') for f in sorted((ROOT/sub).rglob('*')) if f.is_file()}
(OUT/'source-sha256.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
from pathlib import Path
import subprocess,json,os,shutil
source=(ROOT/'krnl/cache_maintenance.c').read_text()
def replace_fn(t,name,new):
 start=t.index(name+'('); start=t.rfind('\n',0,start)+1
 brace=t.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(t[end]=='{')-(t[end]=='}');end+=1
 return t[:start]+new+t[end:]
source=replace_fn(source,'pat','')
source=replace_fn(source,'capture','static void capture(struct cache_cpu_snapshot *s){s->cr0=fake.cr0;s->cr3=fake.cr3;s->cr4=fake.cr4;s->pat=fake.pat;s->apic=fake.apic;}')
replacements={
 '__asm__ volatile("pushfq;popq %0":"=r"(flags));':'flags=fake_flags;',
 '__asm__ volatile("invlpg (%0)"::"r"(s->target):"memory");':'++inv_calls;last_inv=s->target;',
 '__asm__ volatile("wbinvd":::"memory");':'++cache_calls;'}
for a,b in replacements.items():assert source.count(a)==1;source=source.replace(a,b)
assert '__asm__' not in source
(OUT/'cache_maintenance_injected.c').write_text(source)
guest=(ROOT/'krnl/nested_guest.c').read_text()
needle='#if defined(VMX_NESTED_WT_TEST) || defined(VMX_NESTED_OWNER_TYPED_TEST) || defined(VMX_NESTED_ATOMIC_UC_TEST)'
assert guest.count(needle)==1
start=guest.index('\n',guest.index(needle));end=guest.index('#endif',start)
body=guest[start:end]
(OUT/'entry_guard.h').write_text('static bool actual_entry_guard(void){bool ok=false; do {\n'+body+'\n ok=true;}while(0);return ok;}\n')
for mode in ['none','address,undefined','thread']:
 d=OUT/('primitives-'+mode);d.mkdir(exist_ok=True)
 cmd=['cc','-D_GNU_SOURCE','-std=c11','-g','-O2' if mode=='none' else '-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fno-pie','-no-pie','-I'+str(ROOT/'krnl'),'-I'+os.environ.get('EFI_PREFIX','/usr')+'/include/efi','-I'+os.environ.get('EFI_PREFIX','/usr')+'/include/efi/x86_64',str(OUT/'primitives.c'),str(ROOT/'krnl/cache_page.c'),'-o',str(d/'test')]
 if mode!='none':cmd+=['-fsanitize='+mode]
 r=subprocess.run(cmd,capture_output=True,text=True);(d/'build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 r=subprocess.run([str(d/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1'));(d/'run.log').write_text(r.stdout+r.stderr);print(mode,r.stdout,r.stderr);assert r.returncode==0,r.returncode
(OUT/'primitive-substitutions.json').write_text(json.dumps({'reads':'capture and flags substituted','instructions':['INVLPG','WBINVD'],'production_predicates':'unchanged cache_maintenance.c and cache_page.c; exact entry loop guard block','hardware_claim':False},indent=2)+'\n')

from pathlib import Path
import shutil,subprocess,os,json
# Reuse the supplied fresh-copy generator only for its mechanical extraction and
# one privileged INVEPT substitution; extend the fixture, never the algorithm.
fresh=subprocess.run(['python3',str(ROOT/'efi/tests/nested46b_runtime.py'),'--sanitizer','none'],capture_output=True,text=True,timeout=180)
(OUT/'runtime-base.log').write_text(fresh.stdout+fresh.stderr)
assert fresh.returncode==0,fresh.stdout+fresh.stderr
base=Path(fresh.stdout.splitlines()[0].split()[1])
d=OUT/'runtime-source'
if d.exists():shutil.rmtree(d)
for sub in ('krnl','include','efi/tests','efi/src'):shutil.copytree(base/sub,d/sub)
s=(d/'efi/tests/nested46b_runtime.c').read_text()
runtime=(ROOT/'krnl/nested_atomic_uc_runtime.inc').read_text()
def function(text,name):
 start=text.index(name+'(');start=text.rfind('\n',0,start)+1;b=text.index('{',start);end=b+1;n=1
 while n:n+=(text[end]=='{')-(text[end]=='}');end+=1
 return start,end,text[start:end]
for name in ['atomic_failed','atomic_fail']:
 a,b,_=function(s,name);s=s[:a]+function(runtime,name)[2]+s[b:]
s=s.replace('static struct nested_domain_memory domain_memory;','static struct nested_domain_memory domain_memory;\nstatic struct nested_domain_dispatch domain_dispatch;\nstatic unsigned late_fail_at,late_calls;')
s=s.replace(' return snapshot_stable(s)&&cache_page_audit_current(', ' if(atomic_runtime.memory.catalog.published&&late_fail_at&&++late_calls==late_fail_at)return false;\n return snapshot_stable(s)&&cache_page_audit_current(')
assert s.count('++late_calls')==1
s=s.replace(' restore_target();memset(&atomic_runtime,0,sizeof atomic_runtime);',' restore_target();assert(nested_domain_dispatch_init(&domain_dispatch,0x5333,1));memset(&atomic_runtime,0,sizeof atomic_runtime);')
s=s.replace('int main(void){successful();','int supplied_runtime_main(void){successful();')
s+='\n'+(OUT/'runtime_extra.c').read_text()
(d/'efi/tests/nested46b_runtime.c').write_text(s)
cmd=json.loads((base/'command.json').read_text())
cmd=[v.replace(str(base),str(d)) for v in cmd]
(OUT/'runtime-base-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
cmd+= [str(d/'krnl'/n) for n in ['nested_domain_dispatch.c','nested_domain.c','nested_vmx.c','nested_parallel.c']]
for mode in ['none','address,undefined','thread']:
 p=OUT/('runtime-'+mode);p.mkdir(exist_ok=True)
 c=cmd[:];c[c.index('-o')+1]=str(p/'test');c[c.index('-O1')]='-O2' if mode=='none' else '-O1'
 if mode!='none':c+=['-fsanitize='+mode]
 r=subprocess.run(c,capture_output=True,text=True);(p/'build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 r=subprocess.run([str(p/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1'),timeout=180)
 (p/'run.log').write_text(r.stdout+r.stderr);print(mode,r.stdout,r.stderr);assert r.returncode==0,r.returncode
(OUT/'runtime-substitutions.json').write_text(json.dumps({'basis':'supplied actual bootstrap fixture','production_additions':['atomic_failed verbatim','atomic_fail verbatim','nested_domain_dispatch.c','nested_domain.c','nested_vmx.c','nested_parallel.c'],'fixture_additions':'host guard injection after published, real dispatcher initialization','hardware_claim':False},indent=2)+'\n')

from pathlib import Path
import subprocess,os,shutil,json,resource
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
M=OUT/'mutants';M.mkdir(exist_ok=True)
# Exact needles modify only copied production predicates or instruction seams.
primitives=[
 ('host_cpu','cache_maintenance_injected.c','now.apic==s->apic&&',''),
 ('host_cr3','cache_maintenance_injected.c','now.cr3==s->cr3&&',''),
 ('paging_root','cache_maintenance_injected.c','now.cr3==paging_root()&&',''),
 ('host_pat','cache_maintenance_injected.c','now.pat==s->pat&&',''),
 ('host_cd_nw','cache_maintenance_injected.c','!(now.cr0&UINT64_C(0x60000000))&&',''),
 ('host_cr4','cache_maintenance_injected.c','!(now.cr4&((1u<<7)|(1u<<12)|(1u<<17)))','true'),
 ('host_interrupts','cache_maintenance_injected.c','!(flags&0x200)&&',''),
 ('host_mtrr','cache_maintenance_injected.c','s->mtrr_wb&&',''),
 ('invlpg_noop','cache_maintenance_injected.c','++inv_calls;last_inv=s->target;','last_inv=s->target;'),
 ('wbinvd_noop','cache_maintenance_injected.c','++cache_calls;',''),
 ('invlpg_target','cache_maintenance_injected.c','last_inv=s->target;','last_inv=s->target+4096;'),
 ('table_path_cache','cache_page.c','|| !ordinary_path ||','||'),
 ('target_alias','cache_page.c','if(++a->matches!=1)return CACHE_PAGE_ALIAS;\n        if(next_va!=a->target)return CACHE_PAGE_ALIAS;','++a->matches;'),
 ('target_nx','cache_page.c','|| !(e&NX) || !next_rw || !next_nx ||','|| !next_rw ||'),
 ('entry_cr0','entry_guard.h','(cr0&UINT64_C(0x60000000))||',''),
 ('entry_shadow','entry_guard.h','(shadow&UINT64_C(0x60000000))||',''),
 ('entry_mask','entry_guard.h','(mask&UINT64_C(0x60000000))!=UINT64_C(0x60000000)||',''),
 ('entry_msr','entry_guard.h','(primary&(1u<<28))','false'),
]
runtime=[
 ('dispatch_fault_latch','efi/tests/nested46b_runtime.c','nested_domain_dispatch_markfail(&domain_dispatch);','(void)domain_dispatch;'),
 ('skip_fixed_invept','krnl/nested_atomic_uc_bootstrap.inc','if(!atomic_uc_invalidate_fixed(q->eptp)||','if('),
 ('host_make_wb','krnl/nested_atomic_uc_bootstrap.inc','atomic_uc.broken|UINT64_C(0x19)','atomic_uc.broken|UINT64_C(1)'),
 ('skip_host_break','krnl/nested_atomic_uc_bootstrap.inc','__atomic_store_n(&pte[atomic_uc.host.index],atomic_uc.broken,__ATOMIC_RELEASE);','(void)pte;'),
 ('skip_final_inventory','krnl/nested_atomic_uc_bootstrap.inc','!nested_atomic_memory_ready(&atomic_runtime.memory)||!atomic_private_inventory()','!nested_atomic_memory_ready(&atomic_runtime.memory)'),
 ('skip_publish_readiness','krnl/nested_atomic_memory.c','return nested_atomic_memory_ready(m);','return true;'),
 ('early_release','krnl/nested_atomic_uc_bootstrap.inc','if(!nauc_run(&atomic_uc.controller,','__atomic_store_n(&atomic_uc.released,1,__ATOMIC_RELEASE);\n if(!nauc_run(&atomic_uc.controller,'),
 ('drop_ad_cap','krnl/nested_atomic_uc_bootstrap.inc','||!(ept&(UINT64_C(1)<<21))',''),
 ('drop_optional_msr_gate','krnl/nested_atomic_uc_bootstrap.inc','if(!(primary&(UINT64_C(1)<<63)))return false;',''),
]
results=[]
def buildrun(name,cmd,d):
 r=subprocess.run(cmd,capture_output=True,text=True);(d/'build.log').write_text(r.stdout+r.stderr)
 if r.returncode: raise AssertionError((name,'COMPILE_FAILURE',r.stderr))
 try:r=subprocess.run([str(d/'test')],capture_output=True,text=True,timeout=15)
 except subprocess.TimeoutExpired as e:raise AssertionError((name,'TIMEOUT_NOT_ASSERTION'))
 (d/'run.log').write_text(r.stdout+r.stderr)
 assert r.returncode!=0 and 'Assertion' in r.stderr,(name,'SURVIVED_OR_OTHER',r.returncode,r.stdout,r.stderr)
 results.append({'name':name,'compiled':True,'returncode':r.returncode,'assertion':r.stderr.splitlines()[-1]});print('ASSERTION_KILLED',name,flush=True)
for name,file,a,b in primitives:
 d=M/name;d.mkdir(exist_ok=True)
 for f in ['primitives.c','cache_maintenance_injected.c','entry_guard.h']:shutil.copy2(OUT/f,d/f)
 shutil.copy2(ROOT/'krnl/cache_page.c',d/'cache_page.c')
 p=d/file;s=p.read_text();assert s.count(a)==1,(name,s.count(a));p.write_text(s.replace(a,b))
 cmd=['cc','-D_GNU_SOURCE','-std=c11','-g','-O2','-Wall','-Wextra','-Werror','-Wno-unused-but-set-variable','-Wno-unused-function','-Wno-misleading-indentation','-fno-pie','-no-pie','-I'+str(ROOT/'krnl'),'-I'+os.environ.get('EFI_PREFIX','/usr')+'/include/efi','-I'+os.environ.get('EFI_PREFIX','/usr')+'/include/efi/x86_64',str(d/'primitives.c'),str(d/'cache_page.c'),'-o',str(d/'test')]
 buildrun(name,cmd,d)
base=OUT/'runtime-source'
basecmd=json.loads((OUT/'runtime-base-command.json').read_text())
for name,file,a,b in runtime:
 d=M/name
 if d.exists():shutil.rmtree(d)
 shutil.copytree(OUT/'runtime-source',d)
 p=d/file;s=p.read_text();assert s.count(a)==1,(name,s.count(a));p.write_text(s.replace(a,b))
 c=[x.replace(str(base),str(d)) for x in basecmd];c[c.index('-O1')]='-O2';c+=['-Wno-unused-function']
 c+=[str(d/'krnl'/n) for n in ['nested_domain_dispatch.c','nested_domain.c','nested_vmx.c','nested_parallel.c']]
 buildrun(name,c,d)
(OUT/'mutants.json').write_text(json.dumps(results,indent=2)+'\n')

assert len(results)==27
for name,digest in manifest.items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
(OUT/'results.json').write_text(json.dumps({'status':'PASS','primitive_cases':100,'additional_runtime_cases':7,'supplied_runtime_cases':286,'modes':['none','address,undefined','thread'],'compiled_assertion_killed_mutants':27,'hardware_claim':False},indent=2)+'\n')
print('PASS N46B independent cache/entry/runtime: 100+7 cases with supplied 286 in three modes; 27 compiled assertion-killed mutants; no hardware claim')
