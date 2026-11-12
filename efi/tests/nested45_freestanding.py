#!/usr/bin/env python3
"""Fresh actual N45 Makefile compile/link; no execution/hardware claim."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
work=Path(tempfile.mkdtemp(prefix='syle-n45-freestanding-'))
print('N45_FREESTANDING_EVIDENCE',work,flush=True)
for directory,names,files in os.walk(ROOT):
 names[:]=sorted(n for n in names if n not in {'.git','build','__pycache__'})
 for name in sorted(files):
  source=Path(directory)/name
  assert not source.is_symlink()
  target=work/source.relative_to(ROOT);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
(work/'source-sha256.json').write_text(json.dumps({str(f.relative_to(work)):hashlib.sha256(f.read_bytes()).hexdigest() for d in ('krnl','efi/tests','efi/scripts') for f in (work/d).glob('*') if f.is_file()},indent=2)+'\n')
manifest=json.loads((work/'source-sha256.json').read_text())
manifest['efi/Makefile']=hashlib.sha256((work/'efi/Makefile').read_bytes()).hexdigest()
(work/'source-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
assert not (work/'efi/build').exists()
result=subprocess.run(['make','smp-nested-atomic-gigapage'],cwd=work/'efi',text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
(work/'freestanding-build.log').write_text(result.stdout)
assert result.returncode==0,result.stdout
assert '-nostdlib -static -T ../krnl/link.ld' in result.stdout
for name in ('nested_owner_atomic_maintenance.c','nested_atomic_retrack.c','nested_atomic_multiround.c','typed_root_atomic.c'):
 commands=[line for line in result.stdout.splitlines() if ' -c ../krnl/'+name+' ' in line]
 assert len(commands)==1,(name,commands)
 assert all(flag in commands[0].split() for flag in ('-O1','-ffreestanding','-mgeneral-regs-only','-DSYLE_KERNEL'))
for name in ('k-nested45-maintenance.o','k-nested45-retrack.o','k-nested45-multiround.o','k-nested45-smp.o','k-typed-root-atomic.o','krnl-smp-nested-atomic-gigapage.elf'):
 path=work/'efi/build'/name
 symbols=subprocess.check_output(['nm','-u',str(path)],text=True)
 assert not {'memset','memcpy','memmove','bzero'}&{line.split()[-1] for line in symbols.splitlines()}
 if name.endswith('.elf'):assert not symbols.strip()
 print('FREESTANDING_SHA256',hashlib.sha256(path.read_bytes()).hexdigest(),name)
elf=(work/'efi/build/krnl-smp-nested-atomic-gigapage.elf').read_bytes()
assert elf==(work/'efi/build/smp-nested-atomic-gigapage-esp/syle/krnl.elf').read_bytes()
print('PASS N45 fresh production freestanding objects and actual kernel/ESP link; no unresolved libc or other symbols')
