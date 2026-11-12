#!/usr/bin/env python3
"""Execute the unchanged selected production final-report tail, not a parser.
Hardware/earlier completion is a trusted fixture boundary, not a VMX proof.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='none');p.add_argument('--source-root',type=Path);p.add_argument('--expect-legacy-contradiction',action='store_true');a=p.parse_args()
root=(a.source_root or Path(__file__).resolve().parents[2]).resolve();out=Path(tempfile.mkdtemp(prefix='syle-n47b-terminal-'));source=out/'source'
print('N47B_TERMINAL_EVIDENCE',out,flush=True)
shutil.copytree(root,source,ignore=shutil.ignore_patterns('build','.git','__pycache__'))
r=subprocess.run(['make','-s','--eval=print-n47b-flags:;@echo $(KFLAGS) $(N47BFLAGS)','print-n47b-flags'],cwd=source/'efi',capture_output=True,text=True);assert not r.returncode,r.stderr
flags=[f for f in r.stdout.split() if f not in ('-MMD','-MP')];assert '-DVMX_NESTED_SOURCE_REPLACE_TEST' in flags and '-DVMX_NESTED_DOMAIN_TEST' in flags
cmd=['cc','-E','-P',*flags,'-DSMP_NESTED_TEST','-DSMP_VMX_TEST',str(source/'krnl/smp.c')]
r=subprocess.run(cmd,cwd=source/'efi',capture_output=True,text=True);(out/'preprocess.log').write_text(r.stderr);assert not r.returncode,r.stderr
(out/'smp.c.i').write_text(r.stdout)
start=r.stdout.index('static void smp_nested_run(void)');begin=r.stdout.index('uint64_t *shared=paging_phys_to_virt(nested_shared);',start);end=r.stdout.index('\n}',r.stdout.index('SMP.N47B_PASS',begin))
tail=r.stdout[begin:end];assert 'DOMAIN_SHARED_READ=' in tail and 'SMP.NESTED_BSP_TRACE' in tail
(out/'selected-tail.inc').write_text(tail+'\n')
prefix=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
struct nested_owner_sparse {bool destroyed;uint64_t misses;struct {unsigned live;bool fault;} pool;};
struct nested_parallel_runtime {bool trace_overflow;const char *trace;struct nested_owner_sparse sparse;};
static struct nested_parallel_runtime nested_runs[2];
static struct {bool finalized;} atomic_runtime;
static struct {bool ready;} domain_memory;
static uint64_t report[16],nested_shared;
static char output[16384];static size_t used;static const char *why;static jmp_buf failure;
static void *paging_phys_to_virt(uint64_t p){assert(p==(uintptr_t)report);return report;}
static void log_text(const char *p){size_t n=strlen(p);assert(n<sizeof output-used);memcpy(output+used,p,n+1);used+=n;}
static void log_hex(uint64_t n){char s[32];snprintf(s,sizeof s,"%016llx\n",(unsigned long long)n);log_text(s);}
static _Noreturn void source_panic(const char *p){why=p;longjmp(failure,1);}
static void production_tail(void){
'''
suffix=r'''
}
static void setup(void){
 memset(nested_runs,0,sizeof nested_runs);memset(report,0,sizeof report);output[0]=0;used=0;why=NULL;
 nested_shared=(uintptr_t)report;atomic_runtime.finalized=true;domain_memory.ready=true;
 for(unsigned c=0;c<2;c++){
  unsigned s=c*8;report[s]=report[s+2]=0x3200+c;report[s+1]=0x47b00000+c;
  report[s+3]=UINT64_C(0x5566556655665566);report[s+4]=UINT64_C(0x47b1000000000000)+c*0x100;
  report[s+5]=8;report[s+6]=c?UINT64_C(0x99aa99aa99aa99aa):0;report[s+7]=c?UINT64_C(0x47b0000000000102):0;
  nested_runs[c].trace=c?"AP actual checkpoint trace\n":"BSP actual checkpoint trace\n";
  nested_runs[c].sparse.destroyed=true;nested_runs[c].sparse.misses=3;
 }
}
static bool execute(void){if(setjmp(failure))return false;production_tail();return true;}
static void positive(void){
 setup();assert(execute());assert(!why);
 assert(strstr(output,"SMP.NESTED_BSP_TRACE\nBSP actual checkpoint trace\n"));
 assert(strstr(output,"SMP.NESTED_AP_TRACE\nAP actual checkpoint trace\n"));
 assert(strstr(output,"N47B_ALL_BACKING_RETAINED=1\n"));
 assert(strstr(output,"SMP.N47B_PASS owners=2 roots=3 WB4K_IPAT1_AD1=1"));
 assert(strstr(output,"SMP.DOMAIN_SHARED_READ=5566556655665566\n"));
}
static void negative(void){assert(!execute());assert(why);assert(!strstr(output,"SMP.N47B_PASS"));}
int main(void){
#ifdef EXPECT_LEGACY_CONTRADICTION
 setup();negative();assert(strcmp(why,"DOMAIN SHARED READ FAILED RETAIN")==0);
 puts("REPRODUCED selected v2: legal N47 terminal rejected by inherited old sentinel; no final PASS");return 0;
#else
 positive();
 for(unsigned n=0;n<16;n++){setup();report[n]^=1;negative();}
 for(unsigned c=0;c<2;c++){
  setup();report[c*8+3]=UINT64_C(0x3344334433443344);negative();
  setup();nested_runs[c].trace_overflow=true;negative();
  setup();nested_runs[c].sparse.destroyed=false;negative();
  setup();nested_runs[c].sparse.misses=2;negative();
  setup();nested_runs[c].sparse.pool.live=1;negative();
  setup();nested_runs[c].sparse.pool.fault=true;negative();
 }
 setup();atomic_runtime.finalized=false;negative();
 setup();domain_memory.ready=false;negative();positive();
 puts(output);puts("PASS selected production terminal tail: legal two-owner reports, 30 failing state/field variants; no VMX claim");return 0;
#endif
}
'''
(out/'test.c').write_text(prefix+tail+suffix)
flags=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie']
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
if a.expect_legacy_contradiction:flags+=['-DEXPECT_LEGACY_CONTRADICTION','-Wno-unused-function']
cmd=['cc',*flags,str(out/'test.c'),'-o',str(out/'test')];r=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stderr
r=subprocess.run([str(out/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1'));(out/'run.log').write_text(r.stdout+r.stderr);assert not r.returncode,(r.returncode,r.stderr);print(r.stdout,end='')
if not a.expect_legacy_contradiction:
 needle='log_text("SMP.DOMAIN_SHARED_READ=");';assert needle in tail
 mutant=tail.replace(needle,'if(shared[slot+3]!=UINT64_C(0x3344334433443344))source_panic("DOMAIN SHARED READ FAILED RETAIN");\n'+needle,1)
 (out/'mutant.c').write_text(prefix+mutant+suffix)
 cmd=['cc',*flags,str(out/'mutant.c'),'-o',str(out/'mutant')];r=subprocess.run(cmd,capture_output=True,text=True);(out/'mutant-build.log').write_text(r.stdout+r.stderr);assert not r.returncode,r.stderr
 r=subprocess.run([str(out/'mutant')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1'));(out/'mutant-run.log').write_text(r.stdout+r.stderr);assert r.returncode!=0 and 'Assertion' in r.stderr and 'execute()' in r.stderr,(r.returncode,r.stderr)
 print('KILLED inherited-old-sentinel mutant by executing production terminal positive fixture')
(out/'results.json').write_text(json.dumps({'status':'PASS','sanitizer':a.sanitizer,'legacy_reproducer':a.expect_legacy_contradiction,'selected_tail_sha256':hashlib.sha256(tail.encode()).hexdigest(),'hardware_claim':False},indent=2)+'\n')
(out/'source-sha256.json').write_text(json.dumps({str(p.relative_to(source)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.rglob('*')) if p.is_file()},indent=2)+'\n')
