#!/usr/bin/env python3
"""Fresh-copy production N44 core/typed/TAM tests; physical primitives mocked."""
import argparse,hashlib,json,os,pathlib,resource,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='address,undefined');a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n44-core-'));print('N44_CORE_EVIDENCE',out,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));shutil.copytree(repo/'krnl',out/'krnl')
fixture=(out/'krnl/typed_root_atomic_maintenance_test.c').read_text();assert fixture.count('int main(void)')==1
(out/'krnl/n44_fixture_base.inc').write_text(fixture.replace('int main(void)','int n42a_fixture_main(void)'))
shutil.copy2(repo/'efi/tests/nested44_core_test.c',out/'krnl/test.c')
sources='test.c typed_root_atomic.c typed_root_registry.c typed_root_local_provider.c typed_root_lease.c typed_directory_registry.c typed_directory.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c nested_ept.c nested_atomic_catalog.c nested_root_catalog.c nested_ept_atomic.c ad_updater_gate.c'.split()
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+str(out/'krnl')]
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
(out/'source-sha256.json').write_text(json.dumps({str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (out/'krnl').glob('*') if f.is_file()},indent=2)+'\n')
with (out/'build.log').open('w') as f:r=subprocess.run([os.environ.get('CC','cc'),*flags,*[str(out/'krnl'/s) for s in sources],'-o',str(out/'test')],stdout=f,stderr=subprocess.STDOUT)
assert r.returncode==0,(out/'build.log').read_text()
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';env['TSAN_OPTIONS']='halt_on_error=1'
with (out/'run.log').open('w') as f:r=subprocess.run([str(out/'test')],stdout=f,stderr=subprocess.STDOUT,env=env,timeout=90)
print((out/'run.log').read_text(),end='');assert r.returncode==0,('runtime failed',r.returncode)
cases=sum(line.startswith(('PASS N44 fault case','PASS N44 maintenance case','PASS N44 private')) for line in (out/'run.log').read_text().splitlines());assert cases==12,cases
(out/'results.json').write_text(json.dumps({'sanitizer':a.sanitizer,'cases':cases,'status':'PASS','hardware_claim':False},indent=2)+'\n')
print('PASS N44 focused core; sanitizer='+a.sanitizer+'; 12 cases; no VMX claim')
