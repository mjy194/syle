#!/usr/bin/env python3
"""Independent N42A production protocol review. Host hooks are not VMX evidence."""
from pathlib import Path
import concurrent.futures
import hashlib
import json
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
K=ROOT/'krnl'
COMMON=['typed_root_atomic.c','typed_root_registry.c','typed_root_local_provider.c','typed_root_lease.c',
 'typed_directory_registry.c','typed_directory.c','nested_owner_pool.c','nested_ept_contexts.c',
 'nested_ept_sparse.c','nested_ept.c','nested_atomic_catalog.c','nested_root_catalog.c','nested_ept_atomic.c','ad_updater_gate.c']
TEST='typed_root_atomic_maintenance_review_test.c'
FLAGS=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pthread','-I'+str(K)]
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
TEMP=None
if os.environ.get('N42A_REVIEW_EVIDENCE'):
 EVIDENCE=Path(os.environ['N42A_REVIEW_EVIDENCE']);EVIDENCE.mkdir(parents=True,exist_ok=False)
else:
 TEMP=tempfile.TemporaryDirectory(prefix='syle-n42a-independent-');EVIDENCE=Path(TEMP.name)

MUTANTS=[
 ('typed_root_atomic_maintenance.inc','discard_aliased_init_hooks',
  'm->hooks=hooks;', 'm->hooks=*h;(void)hooks;', 'mr_init_storage'),
 ('typed_root_atomic.c','bypass_closed_admission',
  'return (!o->maintenance||o->maintenance->state==TAM_OPEN)&&(!o->replacement||o->replacement->state==TSR_OPEN||o->replacement->state==TSR_RELEASED);',
  'return (!o->maintenance||o->maintenance->state==TAM_OPEN||true)&&(!o->replacement||o->replacement->state==TSR_OPEN||o->replacement->state==TSR_RELEASED);','mr_order_and_tickets'),
 ('typed_root_atomic_maintenance.inc','ignore_reference_remove_failure',
  'if(!m->hooks.remove_refs(m->context,c,t)||!m->hooks.stopped(m->context,c,t))return tam_fail(m);',
  '(void)m->hooks.remove_refs(m->context,c,t);if(!m->hooks.stopped(m->context,c,t))return tam_fail(m);','mr_failure_retention'),
 ('typed_root_atomic_maintenance.inc','leave_old_trace_usable',
  'trace->usable=false;','trace->usable=trace->usable;','mr_cycle'),
 ('typed_root_atomic_maintenance.inc','accept_foreign_persistent_refill',
  'if(ok)ok=!record->pins&&!r->item->pins&&!record->refills;',
  'if(ok)ok=!record->pins&&!r->item->pins;','mr_failure_retention'),
 ('typed_root_atomic_maintenance.inc','skip_all_owner_revoke_ack',
  'm->state==TAM_DRAINED&&m->revoked==3&&!m->owner_busy[0]',
  'm->state==TAM_DRAINED&&!m->owner_busy[0]','mr_order_and_tickets'),
 ('typed_root_atomic_maintenance.inc','allow_remote_owner_maintenance',
  'if(o->config.current_cpu(o->config.context)!=c)return false;',
  'if(o->config.current_cpu(o->config.context)>=64)return false;','mr_order_and_tickets'),
 ('typed_root_atomic_maintenance.inc','omit_receipt_cpu',
  'got.cpu==call->cpu&&got.acked==','got.acked==','mr_receipt_failures'),
 ('typed_root_atomic_maintenance.inc','omit_receipt_complete_eptp',
  'got.full_eptp==r->full_eptp&&got.hpa==r->hpa&&',
  'got.hpa==r->hpa&&','mr_receipt_failures'),
 ('typed_root_atomic_maintenance.inc','omit_receipt_generation_pair',
  'got.old_generation==r->old_generation&&got.new_generation==r->new_generation&&',
  'true&&','mr_receipt_failures'),
 ('typed_root_atomic_maintenance.inc','omit_receipt_allocation_identity',
  'tam_ticket_eq(got.ticket,call->ticket)&&identity_equal(got.identity,r->identity)&&',
  'tam_ticket_eq(got.ticket,call->ticket)&&','mr_receipt_failures'),
 ('typed_root_atomic_maintenance.inc','omit_receipt_round_ticket',
  'got.success&&tam_ticket_eq(got.ticket,call->ticket)&&',
  'got.success&&','mr_receipt_failures'),
 ('typed_root_atomic_maintenance.inc','omit_receipt_success',
  'bool ok=got.success&&tam_ticket_eq(got.ticket,call->ticket)&&',
  'bool ok=tam_ticket_eq(got.ticket,call->ticket)&&','mr_receipt_failures'),
 ('nested_ept_sparse.c','free_before_exact_receipt',
  'if(receipt&&!receipt(context,t)) {t->poisoned=true;return false;}\n    free_leaves(t);\n    /* Root may',
  'free_leaves(t);\n    if(receipt&&!receipt(context,t)) {t->poisoned=true;return false;}\n    /* Root may','mr_receipt_failures'),
 ('nested_ept_sparse.c','ignore_exact_receipt_failure',
  'if(receipt&&!receipt(context,t)) {t->poisoned=true;return false;}\n    free_leaves(t);\n    /* Root may',
  'if(receipt)(void)receipt(context,t);\n    free_leaves(t);\n    /* Root may','mr_receipt_failures'),
 ('typed_root_atomic_maintenance.inc','skip_final_source_authority',
  'if(!tam_source_clean(m)||!tam_clear_guard(m,t))return tam_fail(m);',
  'if(!tam_clear_guard(m,t))return tam_fail(m);','mr_final_authority'),
 ('nested_ept_atomic_format.h','accept_nonleaf_dirty_bit',
  'uint64_t allowed=mask|7|0x100|',
  'uint64_t allowed=mask|7|0x300|','mr_source_structure'),
 ('typed_root_atomic_maintenance.inc','accept_inconsistent_table_depth',
  'if(p->visited)return p->depth==depth;',
  'if(p->visited)return true;','mr_source_structure'),
 ('typed_root_atomic_maintenance.inc','mutate_nonpresent_payload',
  'uint64_t old=p->raw[n];if(!(old&7))continue;',
  'uint64_t old=p->raw[n];if(!(old&7)&&!(old&0x300))continue;','mr_source_structure'),
 ('nested_atomic_catalog.c','reuse_old_ad_stamp',
  '!cache_equal(t->cache,p->cache)||!stamp_equal(t->path.stamp,p->stamp)||',
  '!cache_equal(t->cache,p->cache)||','mr_stale_trace'),
 ('typed_root_atomic_maintenance.inc','accept_overbroad_receipt_ack',
  'got.cpu==call->cpu&&got.acked==(UINT64_C(1)<<call->cpu)&&!(r->history&~got.acked)',
  'got.cpu==call->cpu&&!(r->history&~got.acked)','mr_receipt_failures'),
 ('typed_root_atomic_maintenance.inc','omit_unselected_source_identity',
  'm->sources[n].full_eptp,&current)||!root_equal(current,m->sources[n]))return false;',
  'm->sources[n].full_eptp,&current))return false;','mr_final_source_identity'),
 ('typed_root_atomic_maintenance.inc','skip_finish_source_audit',
  'if(!tam_stopped(m,t)||!tam_source_clean(m)||!ad_updater_gate_reopen',
  'if(!tam_stopped(m,t)||!ad_updater_gate_reopen','mr_final_source_identity'),
 ('typed_root_atomic.c','skip_read_only_retracking',
  'bool trap=(out->effective02&2)&&!(access&2);',
  'bool trap=false;','mr_cycle'),
]


def compile_one(source,target,sanitize):
 extra=['-fno-pie'] if sanitize=='thread' else []
 subprocess.run(['gcc',*FLAGS,'-fsanitize='+sanitize,*extra,'-c',str(source),'-o',str(target)],check=True)


def build_objects(folder,sanitize):
 folder.mkdir()
 paths=[TEST,*COMMON];objects={name:folder/(name+'.o') for name in paths}
 with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
  futures=[pool.submit(compile_one,K/name,objects[name],sanitize) for name in paths]
  for f in futures:f.result()
 return objects


def link(binary,objects,sanitize):
 extra=['-no-pie'] if sanitize=='thread' else []
 subprocess.run(['gcc','-pthread','-fsanitize='+sanitize,*extra,*map(str,objects.values()),'-o',str(binary)],check=True)


def run(binary,case='all'):
 return subprocess.run([str(binary),case],env=ENV,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)


sources=[K/TEST,K/'typed_root_atomic_review_test.c',K/'typed_root_atomic_maintenance.inc',
 *[K/name for name in COMMON],*sorted(K.glob('*.h')),Path(__file__)]
before={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
results=[]
with tempfile.TemporaryDirectory(prefix='syle-n42a-independent-build-') as tmp:
 tmp=Path(tmp);objects=build_objects(tmp/'asan','address,undefined');link(tmp/'production',objects,'address,undefined')
 result=run(tmp/'production');(EVIDENCE/'production.log').write_text(result.stdout)
 print(result.stdout,end='',flush=True)
 if result.returncode:raise SystemExit(f'production review failed: {result.returncode}')
 results.append({'case':'production','status':'PASS'})
 for source,name,old,new,case in MUTANTS:
  original=(K/source).read_text()
  if original.count(old)!=1:raise SystemExit(f'{name}: expected exactly one mutation seam')
  folder=tmp/name;folder.mkdir();changed=folder/source;changed.write_text(original.replace(old,new))
  object_source=source
  if source.endswith(('.inc','.h')):
   # The nonleaf-D check moved from TAM into the shared decoder. Compile
   # its actual TAM consumer beside the changed header; all other production
   # objects and the legacy 4K fixture/assertions stay exactly unchanged.
   object_source='typed_root_atomic.c';changed=folder/object_source;changed.write_bytes((K/object_source).read_bytes())
   if source.endswith('.h'):
    # The decoder is included by this .inc, so it too must be local for the
    # quoted header to resolve to the mutant rather than the original.
    (folder/'typed_root_atomic_maintenance.inc').write_bytes((K/'typed_root_atomic_maintenance.inc').read_bytes())
  mutated=folder/'mutant.o';compile_one(changed,mutated,'address,undefined')
  replacements=dict(objects);replacements[object_source]=mutated;binary=folder/'test';link(binary,replacements,'address,undefined')
  result=run(binary,case);(EVIDENCE/(name+'.log')).write_text(result.stdout)
  if result.returncode!=-6 or 'Assertion' not in result.stdout:
   print(result.stdout,end='');raise SystemExit(f'{name}: missing assertion rejection ({result.returncode})')
  results.append({'case':name,'status':'REJECTED','witness':case});print('PASS independent N42A negative mutant rejected:',name,flush=True)
 tsan=build_objects(tmp/'tsan','thread');link(tmp/'production-tsan',tsan,'thread')
 for case in ('mr_admission_race','mr_entry_close_race','mr_parallel_owners'):
  result=run(tmp/'production-tsan',case);(EVIDENCE/(case+'-tsan.log')).write_text(result.stdout)
  print(result.stdout,end='',flush=True)
  if result.returncode:raise SystemExit(f'TSan {case} failed: {result.returncode}')
  results.append({'case':case+'-tsan','status':'PASS'})
after={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
if after!=before:raise SystemExit('production/review source changed during independent runner')
(EVIDENCE/'result.json').write_text(json.dumps({'status':'PASS',
 'boundary':'actual production software protocol and pools, mocked physical hooks; no VMX, physical INVEPT, source backing release or full regression claim',
 'results':results,'sha256':before},indent=2)+'\n')
print(f'PASS independent N42A production global AD retrack; {len(MUTANTS)} negative mutants; ASan/UBSan/TSan',flush=True)
if TEMP is not None:TEMP.cleanup()
