#!/usr/bin/env python3
"""Fresh-copy actual N46B runtime/bootstrap tests. Never runs privileged code."""
import argparse, hashlib, json, os, pathlib, resource, shutil, subprocess, tempfile
p=argparse.ArgumentParser()
p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='address,undefined')
a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2]
out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n46b-runtime-'))
print('N46B_RUNTIME_EVIDENCE',out,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
for d in ('krnl','include','efi/tests','efi/src'): shutil.copytree(repo/d,out/d)
manifest={str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for d in ('krnl','efi/tests','efi/src','include') for f in (out/d).rglob('*') if f.is_file()}
(out/'source-sha256.json').write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
runtime=(out/'krnl/nested_atomic_uc_runtime.inc').read_text()
context=runtime[runtime.index('static struct {'):runtime.index('} atomic_runtime;')+len('} atomic_runtime;')]
(out/'efi/tests/runtime_context.h').write_text(context+'\n')
def function(text,name):
    start=text.index('static bool '+name+'(')
    brace=text.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return start,brace,end,text[start:end]
for name,dest in [('atomic_prepare','runtime_prepare.h'),('atomic_private_inventory','runtime_inventory.h')]:
    *_,body=function(runtime,name)
    (out/'efi/tests'/dest).write_text(body+'\n')
bootstrap=out/'krnl/nested_atomic_uc_bootstrap.inc';original=bootstrap.read_text()
start,brace,end,body=function(original,'atomic_uc_invalidate_fixed')
assert body.count('__asm__')==1 and 'invept ' in body
replacement=original[start:brace]+'{return mock_fixed_invept(eptp);}'
changed=original[:start]+replacement+original[end:]
assert changed[:start]==original[:start] and changed[start+len(replacement):]==original[end:]
assert '__asm__ volatile("invept ' not in changed
bootstrap.write_text(changed)
(out/'injections.json').write_text(json.dumps({'bootstrap_only_replaced_function':'atomic_uc_invalidate_fixed','verbatim_runtime_extracts':['atomic_runtime','atomic_prepare','atomic_private_inventory'],'controller_hook':'linker --wrap=nauc_run calls production controller and production dispatch','host_audit':'production cache_page.c; CPU snapshots, PMM, VMX and cache instructions mocked','protected_window':'actual target mprotect(PROT_NONE) before HOST_BREAK body until HOST_MAKE receipt','hardware_claim':False},indent=2)+'\n')
inc=os.environ.get('EFI_PREFIX','/usr')+'/include/efi'
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Wl,--wrap=nauc_run','-I'+inc,'-I'+inc+'/x86_64','-I'+str(out/'krnl')]
flags+=['-D'+x for x in ('VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST')]
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
sources=['nested_atomic_memory.c','nested_domain_memory.c','nested_root_catalog.c','nested_ept.c','nested_atomic_uc_provision.c','cache_page.c','typed_directory.c','typed_root_lease.c','ad_updater_gate.c']
cmd=[os.environ.get('CC','cc'),*flags,str(out/'efi/tests/nested46b_runtime.c'),*[str(out/'krnl'/s) for s in sources],'-o',str(out/'test')]
(out/'command.json').write_text(json.dumps(cmd,indent=2)+'\n')
r=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'build.log').write_text(r.stdout);assert r.returncode==0,r.stdout
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1')
r=subprocess.run([str(out/'test')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,timeout=180)
(out/'run.log').write_text(r.stdout);print(r.stdout,end='');assert r.returncode==0,r.returncode
assert 'N46B_RUNTIME_OK cases=286 hardware_claim=false' in r.stdout
(out/'results.json').write_text(json.dumps({'status':'PASS','cases':286,'sanitizer':a.sanitizer,'hardware_claim':False},indent=2)+'\n')
