#!/usr/bin/env python3
"""Focused N43 production two-thread fixture. No VMX execution claim."""
import argparse,hashlib,json,os,pathlib,resource,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='address,undefined');p.add_argument('--efi-prefix',default=os.environ.get('EFI_PREFIX','/usr'));a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n43-host-'));print('N43_HOST_EVIDENCE',out,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
for d in ['krnl','include']:shutil.copytree(repo/d,out/d)
for f in ['efi/tests/nested41c_memory.c','efi/src/exceptions.h']:
 target=out/f;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/f,target)
fixture=(out/'krnl/nested_owner_atomic_maintenance_test.c').read_text();assert fixture.count('int main(void)')==1
(out/'krnl/n43_fixture_base.inc').write_text(fixture.replace('int main(void)','int n42b_fixture_main(void)'))
shutil.copy2(repo/'efi/tests/nested43_test.c',out/'krnl/test.c')
sources='test.c nested_atomic_multiround.c nested_atomic_retrack.c nested_owner_atomic_maintenance.c nested_owner_atomic.c nested_atomic_memory.c nested_domain_memory.c nested_vcpu_policy.c nested_domain_dispatch.c nested_domain.c nested_vmx.c typed_root_atomic.c typed_root_registry.c typed_root_lease.c typed_root_local_provider.c typed_directory_registry.c typed_directory.c nested_atomic_catalog.c nested_root_catalog.c nested_ept_atomic.c ad_updater_gate.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c nested_ept.c'.split()
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+str(out/'krnl'),'-I'+a.efi_prefix+'/include/efi','-I'+a.efi_prefix+'/include/efi/x86_64']
flags+=['-D'+v for v in ['VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST']]
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
(out/'source-sha256.json').write_text(json.dumps({str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (out/'krnl').glob('*') if f.is_file()},indent=2)+'\n')
with (out/'build.log').open('w') as f:r=subprocess.run([os.environ.get('CC','cc'),*flags,*[str(out/'krnl'/s) for s in sources],'-o',str(out/'test')],stdout=f,stderr=subprocess.STDOUT)
assert r.returncode==0,(out/'build.log').read_text()
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';env['TSAN_OPTIONS']='halt_on_error=1'
with (out/'run.log').open('w') as f:r=subprocess.run([str(out/'test')],stdout=f,stderr=subprocess.STDOUT,env=env,timeout=90)
print((out/'run.log').read_text(),end='');assert r.returncode==0,('runtime failed',r.returncode)
cases=sum(line.startswith('PASS N43 ') for line in (out/'run.log').read_text().splitlines());assert cases==31,cases
(out/'results.json').write_text(json.dumps({'sanitizer':a.sanitizer,'cases':cases,'status':'PASS','hardware_claim':False,'trace_exhaustion_clear_rounds':3},indent=2)+'\n')
print('PASS N43 focused host; sanitizer='+a.sanitizer+'; 31 cases; no VMX claim')
