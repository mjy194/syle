#!/usr/bin/env python3
"""Independent N41A tests against production, including weakening mutants."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
KRNL=ROOT/"krnl"
FLAGS=["-std=c11","-O2","-g","-Wall","-Wextra","-Werror","-pthread","-I"+str(KRNL)]
ENV=dict(os.environ,ASAN_OPTIONS="detect_leaks=0")
TEMP_EVIDENCE=None
if os.environ.get("N41A_REVIEW_EVIDENCE"):
 EVIDENCE=Path(os.environ["N41A_REVIEW_EVIDENCE"])
 EVIDENCE.mkdir(parents=True,exist_ok=True)
else:
 TEMP_EVIDENCE=tempfile.TemporaryDirectory(prefix="syle-n41a-review-evidence-")
 EVIDENCE=Path(TEMP_EVIDENCE.name)

def run(binary,case="all"):
 return subprocess.run([str(binary),case],env=ENV,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)

def compile_walker(source,binary,sanitize="address,undefined"):
 extra=["-fno-pie","-no-pie"] if sanitize=="thread" else []
 subprocess.run(["gcc",*FLAGS,"-fsanitize="+sanitize,*extra,str(KRNL/"nested_atomic_wb_review_test.c"),str(source),"-o",str(binary)],check=True)

mutants=[
 ("drop_ipat_requirement","(ipat&&!(raw&0x40))","false","explicit_memory"),
 ("drop_wb_requirement","type!=6","false","format_and_faults"),
 ("drop_explicit_memory",".memory_explicit=ipat",".memory_explicit=false","explicit_memory"),
 ("reject_nonpresent_payload","if(!(raw&7)){denied=true;break;}","if(!(raw&7)){denied=true;return r;}","format_and_faults"),
 ("dirty_read","&& (need&2)","&& true","partial_and_replay"),
 ("replay_saved_dirty","now,now|bits)","now,now|bits|(e->raw&UINT64_C(0x200)))","partial_and_replay"),
 ("drop_nonad_identity","if((now&~mutable)!=(e->raw&~mutable))","if(false&&((now&~mutable)!=(e->raw&~mutable)))","partial_and_replay"),
 ("drop_revisit_hpa","verify_hpa!=e->table_hpa","false","partial_and_replay"),
 ("premature_retry_success","if(!done){r.status=NEA_RETRY;return r;}","if(!done){r.status=NEA_DONE;return r;}","partial_and_replay"),
 ("drop_l0_permissions","(permissions&need)==need?NESTED_EPT_OK:NESTED_EPT_L0_BACKING_DENIED","NESTED_EPT_OK","format_and_faults"),
]
with tempfile.TemporaryDirectory(prefix="syle-n41a-review-") as tmp:
 tmp=Path(tmp);source=KRNL/"nested_ept_atomic.c"
 compile_walker(source,tmp/"walker")
 result=run(tmp/"walker");(EVIDENCE/"walker.log").write_text(result.stdout)
 print(result.stdout,end="")
 if result.returncode:raise SystemExit(f"Production walker failed: {result.returncode}")
 original=source.read_text()
 # N44 moved only these format predicates into the shared decoder. Mutate
 # that same predicate beside a copied production walker so quoted-header
 # lookup selects the changed decoder; legacy 4K assertions are unchanged.
 format_mutants={"drop_ipat_requirement","drop_wb_requirement"}
 for name,old,new,case in mutants:
  target=KRNL/"nested_ept_atomic_format.h" if name in format_mutants else source
  text=target.read_text()
  if text.count(old)!=1:raise SystemExit(f"{name}: expected one mutation seam")
  folder=tmp/name;folder.mkdir()
  changed=folder/"nested_ept_atomic.c"
  if name in format_mutants:
   changed.write_text(original)
   (folder/target.name).write_text(text.replace(old,new))
  else:changed.write_text(text.replace(old,new))
  compile_walker(changed,folder/"test")
  result=run(folder/"test",case);(EVIDENCE/(name+".log")).write_text(result.stdout)
  if result.returncode!=-6 or "Assertion" not in result.stdout:
   print(result.stdout,end="");raise SystemExit(f"{name}: no assertion witness ({result.returncode})")
  print("PASS independent walker weakening mutant rejected:",name)
 compile_walker(source,tmp/"walker-tsan","thread")
 result=run(tmp/"walker-tsan","real_collision");(EVIDENCE/"walker-tsan.log").write_text(result.stdout)
 print(result.stdout,end="")
 if result.returncode:raise SystemExit(f"Production walker TSan failed: {result.returncode}")
print("PASS independent N41A walker sanitizer and mutation review (host only)")

CATALOG_COMMON=["nested_ept_atomic.c","ad_updater_gate.c","nested_ept.c","nested_ept_sparse.c"]
def compile_catalog(atomic,catalog,binary,sanitize="address,undefined"):
 extra=["-fno-pie","-no-pie"] if sanitize=="thread" else []
 subprocess.run(["gcc",*FLAGS,"-fsanitize="+sanitize,*extra,str(KRNL/"nested_atomic_catalog_review_test.c"),
  str(atomic),str(catalog),*[str(KRNL/f) for f in CATALOG_COMMON],"-o",str(binary)],check=True)

atomic_mutants=[
 ("drop_source_generation","a.generation==b.generation&&","","durable_identity"),
 ("drop_source_lifetime","a.generation==b.generation&&a.lifetime==b.lifetime&&","a.generation==b.generation&&","durable_identity"),
 ("drop_cache_lifetime","a.full_eptp==b.full_eptp&&a.lifetime==b.lifetime&&a.generation==b.generation","a.full_eptp==b.full_eptp&&a.generation==b.generation","durable_identity"),
 ("drop_ad_epoch","a.epoch==b.epoch&&a.lifetime==b.lifetime;","a.lifetime==b.lifetime;","durable_identity"),
 ("drop_expected_gpa","t->gpa!=expected_gpa||","false||","durable_identity"),
 ("drop_path_pointer","&page[slot]!=e->pointer||","","durable_identity"),
 ("drop_path_nonad_identity","(now&~mutable)!=(e->raw&~mutable)","(false&&((now&~mutable)!=(e->raw&~mutable)))","durable_identity"),
 ("drop_translation_ipat","a.ignore_pat==b.ignore_pat&&","","durable_identity"),
 ("drop_final_guard","bool valid=nac_guard(p,ticket);","bool valid=true;","cleanup_retention"),
 ("forget_transient_stale","||b.stale","","transient_cache_guards"),
 ("end_before_publication","trace->complete=r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK;","trace->complete=r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK;\n (void)nac_end(p,ticket);","shared_and_terminal"),
]
catalog_mutants=[
 ("drop_pinned_writable","(view?!p.pinned_writable:p.pinned_writable)","false","authority_boundaries"),
 ("drop_atomic_pointer","p.atomic_view!=view||","","authority_boundaries"),
 ("drop_atomic_backing_lifetime","p.backing.lifetime!=span->lifetime||","","authority_boundaries"),
 ("drop_atomic_wb","p.backing.memory_type!=(c->atomic_uc&&gpa==c->uc_gpa?0:6)||\n    span->allowed_types!=(1u<<p.backing.memory_type)||","","authority_boundaries"),
 ("allow_atomic_uc_span","(c->atomic_wb&&s.allowed_types!=NRC_TYPE_WB&&!(c->atomic_uc&&s.allowed_types==NRC_TYPE_UC))","false","shared_and_terminal"),
]
with tempfile.TemporaryDirectory(prefix="syle-n41a-catalog-review-") as tmp:
 tmp=Path(tmp);atomic=KRNL/"nested_atomic_catalog.c";catalog=KRNL/"nested_root_catalog.c"
 compile_catalog(atomic,catalog,tmp/"catalog")
 result=run(tmp/"catalog");(EVIDENCE/"catalog.log").write_text(result.stdout)
 print(result.stdout,end="")
 if result.returncode:raise SystemExit(f"Production catalog failed: {result.returncode}")
 for target,mutants in [(atomic,atomic_mutants),(catalog,catalog_mutants)]:
  original=target.read_text()
  for name,old,new,case in mutants:
   if original.count(old)!=1:raise SystemExit(f"{name}: expected one mutation seam")
   # Keep the unused expected-GPA parameter explicit under Werror in this mutant.
   changed=original.replace(old,new)
   if name=="drop_expected_gpa":changed=changed.replace(" /* No trace pointer", " (void)expected_gpa;\n /* No trace pointer")
   mutant=tmp/(name+".c");mutant.write_text(changed)
   compile_catalog(mutant if target==atomic else atomic,mutant if target==catalog else catalog,tmp/name)
   result=run(tmp/name,case);(EVIDENCE/(name+".log")).write_text(result.stdout)
   if result.returncode!=-6 or "Assertion" not in result.stdout:
    print(result.stdout,end="");raise SystemExit(f"{name}: no assertion witness ({result.returncode})")
   print("PASS independent catalog weakening mutant rejected:",name)
 compile_catalog(atomic,catalog,tmp/"catalog-tsan","thread")
 result=run(tmp/"catalog-tsan","concurrent_catalog");(EVIDENCE/"catalog-tsan.log").write_text(result.stdout)
 print(result.stdout,end="")
 if result.returncode:raise SystemExit(f"Production catalog TSan failed: {result.returncode}")
print("PASS independent N41A production review: ASan/UBSan/TSan, 26 mutants, real sparse retention, host only")
if TEMP_EVIDENCE is not None:TEMP_EVIDENCE.cleanup()
