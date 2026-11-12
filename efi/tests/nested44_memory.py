#!/usr/bin/env python3
"""Fresh-source constructor and production-owner gates. No VMX claim."""
import argparse,os,pathlib,shutil,subprocess,tempfile,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='none');a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n44-memory-'));print('N44_MEMORY_EVIDENCE',out,flush=True)
for d in ('krnl','include','efi/tests','efi/src'):shutil.copytree(repo/d,out/d)
fixture=(out/'krnl/nested_owner_atomic_maintenance_test.c').read_text().replace('int main(void)','int unused_owner_main(void)')
assert fixture.count('assert(nested_atomic_memory_prepare(&m,&d,id));')==1
fixture=fixture.replace('assert(nested_atomic_memory_prepare(&m,&d,id));','assert(nested_atomic_memory_prepare_2m(&m,&d,id));')
(out/'krnl/n44_owner_fixture.inc').write_text(fixture)
shutil.copy2(out/'efi/tests/nested44_owner_test.c',out/'krnl/n44_owner_test.c')
inc=os.environ.get('EFI_PREFIX','/usr')+'/include/efi'
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+inc,'-I'+inc+'/x86_64','-I'+str(out/'krnl')]
flags+=['-D'+x for x in ('VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST')]
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
common='nested_atomic_memory.c nested_domain_memory.c nested_root_catalog.c nested_ept.c nested_ept_atomic.c ad_updater_gate.c'.split()
owner='nested_atomic_retrack.c nested_owner_atomic_maintenance.c nested_owner_atomic.c nested_vcpu_policy.c nested_domain_dispatch.c nested_domain.c nested_vmx.c typed_root_atomic.c typed_root_registry.c typed_root_lease.c typed_root_local_provider.c typed_directory_registry.c typed_directory.c nested_atomic_catalog.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c'.split()
(out/'source-sha256.json').write_text(json.dumps({str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for d in ('krnl','efi/tests') for f in (out/d).glob('*') if f.is_file()},indent=2))
for name,first,extra in [('memory',out/'efi/tests/nested44_memory.c',[]),('owner',out/'krnl/n44_owner_test.c',owner)]:
 cmd=[os.environ.get('CC','cc'),*flags,str(first),*[str(out/'krnl'/s) for s in common+extra],'-o',str(out/name)]
 result=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/(name+'-build.log')).write_text(result.stdout);assert result.returncode==0,result.stdout
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';env['TSAN_OPTIONS']='halt_on_error=1'
 result=subprocess.run([str(out/name)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,timeout=90);(out/(name+'-run.log')).write_text(result.stdout);print(result.stdout,end='');assert result.returncode==0,result.returncode
print('PASS N44 fresh memory/owner gates; sanitizer='+a.sanitizer+'; no hardware claim')
