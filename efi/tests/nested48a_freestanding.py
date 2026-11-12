#!/usr/bin/env python3
"""N48A freestanding closure linked into an isolated retained WB image.
This is symbol/link evidence only: N48A has no real runtime profile yet.
"""
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
work=Path(tempfile.mkdtemp(prefix='syle-n48a-freestanding-'))
print('N48A_FREESTANDING_EVIDENCE',work,flush=True)
for directory,names,files in os.walk(ROOT):
 names[:]=sorted(n for n in names if n not in {'.git','build','__pycache__'})
 for name in sorted(files):
  source=Path(directory)/name;assert not source.is_symlink()
  target=work/source.relative_to(ROOT);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
(work/'source-sha256.json').write_text(json.dumps({str(f.relative_to(work)):hashlib.sha256(f.read_bytes()).hexdigest() for f in work.rglob('*') if f.is_file()},indent=2)+'\n')
# Kernel-only link needs no UEFI SDK/loader. This test promises no ESP build.
r=subprocess.run(['make','build/krnl-smp-nested-source-replace.elf'],cwd=work/'efi',capture_output=True,text=True,timeout=180)
log=r.stdout+r.stderr;(work/'freestanding-build.log').write_text(log);assert r.returncode==0,log
compile_line=next(l for l in log.splitlines() if ' -c ../krnl/nested_source_replace_round.c ' in l)
cmd=shlex.split(compile_line)
cmd[cmd.index('../krnl/nested_source_replace_round.c')]='../krnl/nested_source_replace_chain.c'
obj=work/'efi/build/k-n48a-chain.o';cmd[cmd.index('-o')+1]=str(obj)
commands=[cmd]
r=subprocess.run(cmd,cwd=work/'efi',capture_output=True,text=True)
(work/'n48a-chain-build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
link_line=next(l for l in log.splitlines() if '-nostdlib -static -T ../krnl/link.ld' in l and ' -o build/krnl-smp-nested-source-replace.elf' in l)
cmd=shlex.split(link_line);elf=work/'efi/build/krnl-n48a-link-only.elf';cmd[cmd.index('-o')+1]=str(elf);cmd.append(str(obj));commands.append(cmd)
r=subprocess.run(cmd,cwd=work/'efi',capture_output=True,text=True)
(work/'n48a-chain-link.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
for path in (obj,elf):
 u=subprocess.check_output(['nm','-u',str(path)],text=True)
 assert not {'memset','memcpy','memmove','bzero'}&{l.split()[-1] for l in u.splitlines()}
 if path==elf:assert not u.strip()
symbols=subprocess.check_output(['nm',str(elf)],text=True)
for symbol in ('nsrc_init','nsrc_run','nsrc_before_entry','nosr_checkpoint','tsr_init_successor_locked','tsr_departed_locked','nsrr_retain_stop'):
 assert any(l.endswith(' T '+symbol) for l in symbols.splitlines()),symbol
# Explicit Make dependency and generated compiler dependency both name .inc.
assert '../krnl/typed_root_source_successor.inc' in (work/'efi/Makefile').read_text()
assert '../krnl/typed_root_source_successor.inc' in (work/'efi/build/k-typed-root-atomic.d').read_text()
(work/'n48a-freestanding-result.json').write_text(json.dumps({'status':'PASS','commands':commands,'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'runtime_wiring':False,'hardware_claim':False},indent=2)+'\n')
print('PASS N48A chain/round/owner/successor freestanding compile and complete static ELF link; no undefined symbols; not runtime or hardware acceptance')
print('N48A_FREESTANDING_EVIDENCE',work)
