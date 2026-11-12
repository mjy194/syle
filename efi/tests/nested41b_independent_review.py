#!/usr/bin/env python3
"""Independent production N41B review. Mocks are not hardware evidence."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
KRNL=ROOT/"krnl"
COMMON=["typed_root_atomic.c","typed_root_registry.c","typed_root_lease.c",
 "typed_root_local_provider.c","typed_directory_registry.c","typed_directory.c",
 "nested_atomic_catalog.c","nested_root_catalog.c","nested_ept_atomic.c","ad_updater_gate.c",
 "nested_owner_pool.c","nested_ept_contexts.c","nested_ept_sparse.c","nested_ept.c"]
FLAGS=["-std=c11","-O2","-g","-Wall","-Wextra","-Werror","-pthread","-I"+str(KRNL)]
ENV=dict(os.environ,ASAN_OPTIONS="detect_leaks=0")
TEMP=None
if os.environ.get("N41B_REVIEW_EVIDENCE"):
 EVIDENCE=Path(os.environ["N41B_REVIEW_EVIDENCE"])
 EVIDENCE.mkdir(parents=True,exist_ok=False)
else:
 TEMP=tempfile.TemporaryDirectory(prefix="syle-n41b-review-evidence-")
 EVIDENCE=Path(TEMP.name)


def compile_test(binary,replace=None,sanitize="address,undefined"):
 replace=replace or {}
 extra=["-fno-pie","-no-pie"] if sanitize=="thread" else []
 subprocess.run(["gcc",*FLAGS,"-fsanitize="+sanitize,*extra,
  str(KRNL/"typed_root_atomic_review_test.c"),
  *[str(replace.get(name,KRNL/name)) for name in COMMON],"-o",str(binary)],check=True)


def run(binary,case="all"):
 return subprocess.run([str(binary),case],env=ENV,text=True,stdout=subprocess.PIPE,
  stderr=subprocess.STDOUT,timeout=120)


MUTANTS=[
 ("typed_root_atomic.c","skip_creation_source_guard",
  'if(!sr_command_key(o,item->key)||!local(o)||!source_guard(o,a))return false;','if(!sr_command_key(o,item->key)||!local(o))return false;',"warm_identity"),
 ("nested_ept_contexts.c","skip_warm_selection_guard",
  "r->guards&&!r->guards->select(r->guard_context,p)","false","warm_identity"),
 ("typed_root_atomic.c","use_unregistered_trace",
  "&&t->usable&&t->registered)","&&t->usable)","trace_identity"),
 ("typed_root_atomic.c","skip_cached_trace_guard",
  "!nac_trace_guard(&f->permit,f->ticket,page,&f->record->trace)","false","trace_identity"),
 ("typed_root_atomic.c","skip_private_read_tracking",
  "bool trap=(out->effective02&2)&&!(access&2);","bool trap=false;","legacy_bypasses"),
 ("typed_root_atomic.c","ignore_final_guard_result",
  "if(!nac_complete(&f->permit,f->ticket))status=TR_ATOMIC_FATAL;",
  "(void)nac_complete(&f->permit,f->ticket);","guard_cleanup_failures"),
 ("typed_root_atomic.c","release_before_sparse_publication",
  "bool trap=(out->effective02&2)&&!(access&2);",
  "bool trap=(out->effective02&2)&&!(access&2); (void)nac_end(&f->permit,f->ticket);","shared_churn"),
 ("typed_root_atomic.c","forget_failed_transient_release",
  "if(ok)f->transient=false;","f->transient=false;","guard_cleanup_failures"),
 ("nested_ept_contexts.c","skip_pre_reset_revocation",
  "r->guards&&!r->guards->pre_reset(r->guard_context,p)","false","reset_generations"),
 ("nested_ept_contexts.c","skip_pre_retire_revocation",
  "r->guards&&!r->guards->pre_retire(r->guard_context,p)","false","shared_churn"),
 ("typed_root_atomic.c","ignore_source_pin_release_failure",
  "if(!pin_guard(o,a)||!nrc_end(&a->pin,a->pin_nonce))return false;",
  "(void)pin_guard(o,a); (void)nrc_end(&a->pin,a->pin_nonce);","failures"),
 ("typed_root_registry.c","restore_global_version_fence",
  "if(!b->root_scoped) {","if(directory->version!=version)return false;\n  if(!b->root_scoped) {","provider_overlap"),
 ("typed_root_registry.c","admit_legacy_atomic_helpers",
  "!healthy(b)||b->atomic_wb||!item||","!healthy(b)||!item||","legacy_bypasses"),
 ("typed_root_local_provider.c","omit_physical_cpu_postcheck",
  "local.cpu==p->owner&&","","failures"),
 ("typed_root_local_provider.c","omit_creation_source_pending",
  "b->atomic_hooks.pending&&b->atomic_hooks.pending(b->atomic_context,s,item)","true","retirement_source"),
 ("typed_root_atomic.c","drop_catalog_directory_domain",
  "l->domain!=c->catalog_identity.domain||","","profile_boundaries"),
 ("typed_root_atomic.c","drop_catalog_directory_incarnation",
  "l->incarnation!=c->catalog_identity.incarnation||","","profile_boundaries"),
 ("typed_root_lease.c","grant_legacy_backing_release_authority",
  "if(!l||l->atomic_wb||!e||!trl_freeze_valid(l,f)","if(!l||!e||!trl_freeze_valid(l,f)","profile_boundaries"),
 ("typed_root_lease.c","allow_atomic_type_transition",
  "return !l->atomic_wb&&epoch>l->type_epoch","return epoch>l->type_epoch","profile_boundaries"),
]

sources=[KRNL/"typed_root_atomic_review_test.c",KRNL/"typed_root_atomic_maintenance.inc",*[KRNL/name for name in COMMON],*sorted(KRNL.glob("*.h")),Path(__file__)]
before={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
results=[]
with tempfile.TemporaryDirectory(prefix="syle-n41b-review-") as tmp:
 tmp=Path(tmp)
 compile_test(tmp/"production")
 result=run(tmp/"production");(EVIDENCE/"production.log").write_text(result.stdout)
 print(result.stdout,end="",flush=True)
 if result.returncode:raise SystemExit(f"production review failed: {result.returncode}")
 results.append({"case":"production","status":"PASS"})
 for source,name,old,new,case in MUTANTS:
  original=(KRNL/source).read_text()
  if original.count(old)!=1:raise SystemExit(f"{name}: expected one mutation seam")
  changed=tmp/(name+".c");changed.write_text(original.replace(old,new))
  compile_test(tmp/name,{source:changed})
  result=run(tmp/name,case);(EVIDENCE/(name+".log")).write_text(result.stdout)
  if result.returncode!=-6 or "Assertion" not in result.stdout:
   print(result.stdout,end="");raise SystemExit(f"{name}: missing assertion witness ({result.returncode})")
  print("PASS independent N41B weakening mutant rejected:",name,flush=True)
  results.append({"case":name,"status":"REJECTED","witness":case})
 compile_test(tmp/"production-tsan",sanitize="thread")
 for case in ("concurrent_churn","provider_overlap"):
  result=run(tmp/"production-tsan",case);(EVIDENCE/(case+"-tsan.log")).write_text(result.stdout)
  print(result.stdout,end="",flush=True)
  if result.returncode:raise SystemExit(f"TSan {case} failed: {result.returncode}")
  results.append({"case":case+"-tsan","status":"PASS"})
after={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
if after!=before:raise SystemExit("production/review source changed during independent runner")
(EVIDENCE/"result.json").write_text(json.dumps({"status":"PASS","boundary":"host mocks only; no VMX, physical INVEPT or full regression claim",
 "results":results,"sha256":before},indent=2)+"\n")
print(f"PASS independent N41B actual production lifecycle/refill/provider review; {len(MUTANTS)} mutants; ASan/UBSan/TSan",flush=True)
if TEMP is not None:TEMP.cleanup()
