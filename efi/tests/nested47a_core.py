#!/usr/bin/env python3
"""N47A production coordinator/bridge/pool integration, physical operations mocked."""
import argparse,hashlib,json,os,pathlib,resource,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='address,undefined');a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n47a-core-'));print('N47A_CORE_EVIDENCE',out,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));shutil.copytree(repo/'krnl',out/'krnl')
s=(out/'krnl/typed_root_atomic_test.c').read_text();s=s.replace('int main(void)','int legacy_fixture_main(void)')
s=s.replace('#include <assert.h>','#include <assert.h>\n#include <pthread.h>\n#include <stdatomic.h>\nstatic pthread_mutex_t metadata_lock=PTHREAD_MUTEX_INITIALIZER;\nstatic _Thread_local unsigned tls_depth,tls_cpu;\nstatic void pause_authority(void);\nstatic void pause_provider(void);\nstatic void pause_allocator(void);')
s=s.replace('struct nrc_span spans[11]', 'struct nrc_span spans[12]').replace('f->spans,11,','f->spans,12,').replace('for(unsigned i=0;i<2;i++){\n  struct nrc_span', 'for(unsigned i=0;i<3;i++){\n  struct nrc_span')
s=s.replace('o->f->depth','tls_depth').replace('o->f->cpu','tls_cpu').replace('f->depth','tls_depth').replace('f->cpu','tls_cpu')
s=s.replace('static void lock(void *p){struct owner *o=p;assert(!tls_depth);tls_depth=1;}','static void lock(void *p){(void)p;assert(!tls_depth);assert(!pthread_mutex_lock(&metadata_lock));tls_depth=1;}')
s=s.replace('static void unlock(void *p){struct owner *o=p;assert(tls_depth==1);tls_depth=0;}','static void unlock(void *p){(void)p;assert(tls_depth==1);tls_depth=0;assert(!pthread_mutex_unlock(&metadata_lock));}')
s=s.replace('static unsigned current_cpu(void *p){struct owner *o=p;return tls_cpu;}','static unsigned current_cpu(void *p){(void)p;return tls_cpu;}')
s=s.replace('o->authorities++;','o->authorities++;pause_authority();').replace('bool ok=tr_local_provider_flush','pause_provider();\n bool ok=tr_local_provider_flush').replace('o->allocations++;','o->allocations++;pause_allocator();')
(out/'krnl/n47_fixture.inc').write_text(s)
shutil.copy2(repo/'efi/tests/nested47a_core_test.c',out/'krnl/test.c')
sources='test.c typed_root_atomic.c typed_root_registry.c typed_root_local_provider.c typed_root_lease.c typed_directory_registry.c typed_directory.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c nested_ept.c nested_atomic_catalog.c nested_root_catalog.c nested_ept_atomic.c ad_updater_gate.c nested_source_replace_memory.c nested_domain.c'.split()
flags=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+str(out/'krnl'),'-Wl,--wrap=nrc_end','-Wl,--wrap=nrc_guard','-Wl,--wrap=typed_root_registry_begin_guarded','-Wl,--wrap=ad_updater_gate_idle']
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
(out/'source-sha256.json').write_text(json.dumps({str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (out/'krnl').glob('*') if f.is_file()},indent=2)+'\n')
with (out/'build.log').open('w') as f:r=subprocess.run([os.environ.get('CC','cc'),*flags,*[str(out/'krnl'/s) for s in sources],'-o',str(out/'test')],stdout=f,stderr=subprocess.STDOUT)
assert r.returncode==0,(out/'build.log').read_text()
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';env['TSAN_OPTIONS']='halt_on_error=1'
with (out/'run.log').open('w') as f:r=subprocess.run([str(out/'test')],stdout=f,stderr=subprocess.STDOUT,env=env,timeout=90)
print((out/'run.log').read_text(),end='');assert r.returncode==0,('runtime failed',r.returncode)
(out/'results.json').write_text(json.dumps({'sanitizer':a.sanitizer,'status':'PASS','hardware_claim':False},indent=2)+'\n')
print('PASS N47A production core; sanitizer='+a.sanitizer+'; no VMX claim')
