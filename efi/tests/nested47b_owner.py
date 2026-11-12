#!/usr/bin/env python3
"""N47B owner-local VMCS adapter: real coordinator/dispatch/pool, injected VMX."""
import argparse,hashlib,json,os,pathlib,re,resource,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='address,undefined');a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n47b-owner-'));print('N47B_OWNER_EVIDENCE',out,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));shutil.copytree(repo/'krnl',out/'krnl')
s=(out/'krnl/typed_root_atomic_test.c').read_text().replace('int main(void)','int legacy_fixture_main(void)')
s=s.replace('#include <assert.h>','#include <assert.h>\n#include "nested_owner_source_replace.h"\n#include "nested_owner_sparse.h"\n#include <pthread.h>\nstatic pthread_mutex_t metadata_lock=PTHREAD_MUTEX_INITIALIZER;\nstatic _Thread_local unsigned tls_depth,tls_cpu;\n#if defined(__SANITIZE_ADDRESS__)\n#include <sanitizer/asan_interface.h>\n#define POISON(p,n) __asan_poison_memory_region((p),(n))\n#define UNPOISON(p,n) __asan_unpoison_memory_region((p),(n))\n#else\n#define POISON(p,n) ((void)0)\n#define UNPOISON(p,n) ((void)0)\n#endif')
s=s.replace('#define SLOTS 16u','#define SLOTS NESTED_ATOMIC_SLOTS')
s=s.replace(' struct tr_atomic_owner atomic;',' struct nested_owner_atomic wrapper;\n struct nested_vcpu vcpu;\n struct ept_fixture parking;\n uint64_t current,physical[2],composed;\n bool launched[2],parking_ok;\n unsigned calls[4],fail_kind,fail_n,bad_kind,bad_n,uncertain_write;')
for decl in [' struct typed_root_registry bridge;',' struct nested_ept_contexts registry;',' struct nested_owner_pool pool;',' struct tr_registry_slot slots[SLOTS];struct tr_atomic_binding bindings[SLOTS];',' struct tr_atomic_trace traces[TRACES];',' struct tr_local_provider provider;struct tr_local_completed completed[SLOTS];']:
    assert decl in s;s=s.replace(decl,'')
s=s.replace(' struct nested_vcpu vcpu;',' struct nested_vcpu vcpu;\n struct nested_owner_sparse sparse;')
s=s.replace('struct nrc_span spans[11]','struct nrc_span spans[12]').replace('f->spans,11,','f->spans,12,').replace('for(unsigned i=0;i<2;i++){\n  struct nrc_span','for(unsigned i=0;i<3;i++){\n  struct nrc_span')
s=s.replace('o->f->depth','tls_depth').replace('o->f->cpu','tls_cpu').replace('f->depth','tls_depth').replace('f->cpu','tls_cpu')
s=s.replace('static void lock(void *p){struct owner *o=p;assert(!tls_depth);tls_depth=1;}','static void lock(void *p){(void)p;assert(!tls_depth);assert(!pthread_mutex_lock(&metadata_lock));tls_depth=1;}')
s=s.replace('static void unlock(void *p){struct owner *o=p;assert(tls_depth==1);tls_depth=0;}','static void unlock(void *p){(void)p;assert(tls_depth==1);tls_depth=0;assert(!pthread_mutex_unlock(&metadata_lock));}')
s=s.replace('static unsigned current_cpu(void *p){struct owner *o=p;return tls_cpu;}','static unsigned current_cpu(void *p){(void)p;return tls_cpu;}')
s=s.replace('return nested_owner_pool_alloc(&o->pool,out);','bool ok=nested_owner_pool_alloc(&o->pool,out);if(ok)UNPOISON(out->view,4096);return ok;')
s=s.replace('assert(!o->pool.fault);','assert(!o->pool.fault);POISON(page.view,4096);')
s=s.replace('memset(f,0,sizeof *f);memset(tables,0,sizeof tables);','UNPOISON(memory,sizeof memory);memset(f,0,sizeof *f);memset(tables,0,sizeof tables);')
for name in ['atomic','bridge','slots','bindings','traces','provider','completed']:
    s=re.sub(r'(->|\.)'+name+r'\b',r'\1wrapper.'+name,s)
s=re.sub(r'(->|\.)registry\b',r'\1sparse.contexts',s)
s=re.sub(r'(->|\.)pool\b',r'\1sparse.pool',s)
# Compound initializers refer to real API field names, not owner member aliases.
s=s.replace('.wrapper.slots=', '.slots=').replace('.wrapper.bindings=', '.bindings=').replace('.wrapper.traces=', '.traces=').replace('.wrapper.bridge=', '.bridge=').replace('.wrapper.completed=', '.completed=')
(out/'krnl/n47b_owner_fixture.inc').write_text(s)
shutil.copy2(repo/'efi/tests/nested47b_owner_test.c',out/'krnl/test.c')
sources='test.c nested_owner_source_replace.c nested_source_replace_round.c nested_owner_atomic.c typed_root_atomic.c typed_root_registry.c typed_root_local_provider.c typed_root_lease.c typed_directory_registry.c typed_directory.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c nested_ept.c nested_atomic_catalog.c nested_root_catalog.c nested_ept_atomic.c ad_updater_gate.c nested_source_replace_memory.c nested_domain.c nested_domain_dispatch.c nested_vmx.c nested_vcpu_policy.c'.split()
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-DVMX_NESTED_ATOMIC_CATALOG_TEST','-Wl,--wrap=nested_domain_dispatch_source_plan','-I'+str(out/'krnl')]
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
cmd=[os.environ.get('CC','cc'),*flags,*[str(out/'krnl'/s) for s in sources],'-o',str(out/'test')]
(out/'command.json').write_text(json.dumps(cmd,indent=2)+'\n')
(out/'source-sha256.json').write_text(json.dumps({str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (out/'krnl').glob('*') if f.is_file()},indent=2)+'\n')
with (out/'build.log').open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
assert r.returncode==0,(out/'build.log').read_text()
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1')
with (out/'run.log').open('w') as f:r=subprocess.run([str(out/'test')],stdout=f,stderr=subprocess.STDOUT,env=env,timeout=90)
print((out/'run.log').read_text(),end='');assert r.returncode==0,('runtime failed',r.returncode)
(out/'results.json').write_text(json.dumps({'sanitizer':a.sanitizer,'status':'PASS','hardware_claim':False},indent=2)+'\n')
print('PASS N47B owner adapter and command round; sanitizer='+a.sanitizer+'; no VMX claim')
