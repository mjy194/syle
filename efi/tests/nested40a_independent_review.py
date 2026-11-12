#!/usr/bin/env python3
"""Independent production catalog checks and exact negative mutation witnesses."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "krnl/nested_root_catalog.c"
TEST = ROOT / "krnl/nested_root_catalog_review_test.c"
FLAGS = ["-std=c11", "-O2", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-I" + str(ROOT / "krnl")]
ENV = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
MUTANTS = [
 ("canonical_key_only", "c->roots[i].full_eptp==key", "(c->roots[i].full_eptp&~UINT64_C(4095))==(key&~UINT64_C(4095))", "registration"),
 ("writable_table", "s.permissions!=1", "!(s.permissions&1)", "registration"),
 ("missing_bound", "bytes<=limit-base", "true", "registration"),
 ("missing_hpa_alias", "overlap(p->hpa,p->bytes,s.hpa,s.bytes)", "false", "registration"),
 ("missing_source_generation", "a.generation==b.generation&&", "", "authority_failures"),
 ("missing_object_lifetime", "p.lifetime!=span->lifetime||", "", "authority_failures"),
 ("missing_hpa_binding", "p.hpa!=span->hpa+offset||", "", "authority_failures"),
 ("missing_post_authority_guard", "!c->authority(s->authority_context,span,gpa,&p)||!nrc_guard(s,nonce)", "!c->authority(s->authority_context,span,gpa,&p)", "authority_failures"),
 ("missing_session_nonce", "s->nonce!=nonce||", "", "lifecycle"),
 ("missing_leaf_type", "r.leaf_type!=w.last.memory_type", "false", "memory_types"),
 ("missing_leaf_ipat", "r.ignore_pat!=w.last.ignore_pat", "false", "memory_types"),
 ("missing_l0_permission", "(w.last.permissions&access)!=access", "false", "statuses"),
]
def compile_run(src, binary, case="all"):
 subprocess.run(["cc", *FLAGS, str(TEST), str(src), str(ROOT / "krnl/nested_ept.c"), "-o", str(binary)], check=True)
 return subprocess.run([str(binary), case], env=ENV, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
with tempfile.TemporaryDirectory(prefix="syle-n40-independent-") as tmp:
 tmp=Path(tmp)
 base=compile_run(SOURCE,tmp/"baseline")
 print(base.stdout,end="")
 if base.returncode:
  raise SystemExit(f"Production review failed: {base.returncode}")
 source=SOURCE.read_text()
 for name,old,new,case in MUTANTS:
  if source.count(old)!=1:raise SystemExit(f"Mutation {name} expected one seam, got {source.count(old)}")
  mutant=tmp/(name+".c");mutant.write_text(source.replace(old,new))
  result=compile_run(mutant,tmp/name,case)
  (tmp/(name+".log")).write_text(result.stdout)
  if result.returncode!=-6 or "Assertion" not in result.stdout:
   print(result.stdout,end="")
   raise SystemExit(f"Mutation {name} did not hit an assertion witness: {result.returncode}")
  print(f"PASS independent mutant rejected: {name}")
BRIDGE = ROOT / "krnl/typed_root_registry.c"
BRIDGE_TEST = ROOT / "krnl/nested_root_catalog_registry_review_test.c"
BRIDGE_COMMON = ["typed_root_lease.c", "typed_directory_registry.c", "typed_directory.c", "nested_owner_pool.c", "nested_ept_contexts.c", "nested_ept_sparse.c", "nested_ept.c"]
with tempfile.TemporaryDirectory(prefix="syle-n40-bridge-review-") as tmp:
 tmp=Path(tmp)
 def bridge_run(src, binary):
  subprocess.run(["cc", *FLAGS, str(BRIDGE_TEST), str(src), *[str(ROOT / "krnl" / f) for f in BRIDGE_COMMON], "-o", str(binary)], check=True)
  return subprocess.run([str(binary)], env=ENV, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
 result=bridge_run(BRIDGE,tmp/"baseline")
 print(result.stdout,end="")
 if result.returncode:raise SystemExit(f"Production source bridge failed: {result.returncode}")
 source=BRIDGE.read_text()
 mutations=[
  ("missing_initial_source_guard", "if(source->guard(context,&snapshot)&&", "if(true&&"),
  ("missing_prefill_source_guard", "if(!source->guard(context,&snapshot))out->status=NESTED_EPT_L0_BACKING_DENIED;", "if(false)out->status=NESTED_EPT_L0_BACKING_DENIED;"),
  ("missing_postfill_source_guard", "if(!source->guard(context,&snapshot)) {", "if(false) {"),
 ]
 for name,old,new in mutations:
  if source.count(old)!=1:raise SystemExit(f"Bridge mutation {name} expected one seam")
  mutant=tmp/(name+".c");mutant.write_text(source.replace(old,new))
  result=bridge_run(mutant,tmp/name)
  if result.returncode!=-6 or "Assertion" not in result.stdout:
   print(result.stdout,end="")
   raise SystemExit(f"Bridge mutation {name} did not hit an assertion witness: {result.returncode}")
  print(f"PASS independent bridge mutant rejected: {name}")
with tempfile.TemporaryDirectory(prefix="syle-n40-concurrent-review-") as tmp:
 binary=Path(tmp)/"concurrent"
 subprocess.run(["cc", *FLAGS, "-pthread", str(ROOT / "krnl/nested_root_catalog_concurrent_review_test.c"), str(SOURCE), str(ROOT / "krnl/nested_ept.c"), "-o", str(binary)], check=True)
 result=subprocess.run([str(binary)],env=ENV,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
 print(result.stdout,end="")
 if result.returncode:raise SystemExit(f"Concurrent catalog review failed: {result.returncode}")
print("PASS N40A independent production boundaries, 15 mutants and 100 threaded schedules (host only)")
