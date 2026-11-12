#!/usr/bin/env python3
"""Fresh N47B coordinator in retained WB Makefile image; no source-replacement hardware claim."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,tempfile,struct,re
ROOT=Path(__file__).resolve().parents[2]
work=Path(tempfile.mkdtemp(prefix='syle-n47b-freestanding-'))
print('N47B_FREESTANDING_EVIDENCE',work,flush=True)
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
result=subprocess.run(['make','smp-nested-source-replace'],cwd=work/'efi',text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
(work/'freestanding-build.log').write_text(result.stdout)
assert result.returncode==0,result.stdout
assert '-nostdlib -static -T ../krnl/link.ld' in result.stdout
for name in ('nested_owner_source_replace.c','nested_source_replace_round.c','nested_source_replace_fixture.c','nested_source_replace_memory.c','typed_root_atomic.c'):
 commands=[line for line in result.stdout.splitlines() if ' -c ../krnl/'+name+' ' in line]
 assert len(commands)==1,(name,commands)
 assert all(flag in commands[0].split() for flag in ('-O1','-ffreestanding','-mgeneral-regs-only','-DSYLE_KERNEL'))
for name in ('k-nested47b-owner-replace.o','k-nested47b-round.o','k-nested47b-fixture.o','k-nested47b-replace-memory.o','k-nested47b-smp.o','k-typed-root-atomic.o','krnl-smp-nested-source-replace.elf'):
 path=work/'efi/build'/name
 symbols=subprocess.check_output(['nm','-u',str(path)],text=True)
 assert not {'memset','memcpy','memmove','bzero'}&{line.split()[-1] for line in symbols.splitlines()}
 if name.endswith('.elf'):assert not symbols.strip()
 print('FREESTANDING_SHA256',hashlib.sha256(path.read_bytes()).hexdigest(),name)
elf=(work/'efi/build/krnl-smp-nested-source-replace.elf').read_bytes()
assert elf==(work/'efi/build/smp-nested-source-replace-esp/syle/krnl.elf').read_bytes()
# Final strings must survive optimization in allocated sections and have real
# code references. Debug strings/source text alone are not emission evidence.
# Actual selected-tail execution is separately checked by nested47b_terminal.
shoff=struct.unpack_from('<Q',elf,40)[0];entsize,count=struct.unpack_from('<HH',elf,58)
sections=[struct.unpack_from('<IIQQQQIIQQ',elf,shoff+n*entsize) for n in range(count)]
disassembly=subprocess.check_output(['objdump','-d','--no-show-raw-insn',str(work/'efi/build/krnl-smp-nested-source-replace.elf')],text=True)
(work/'terminal-disassembly.log').write_text(disassembly)
emissions=[]
for marker in ('SMP.N47B_PASS owners=2 roots=3 WB4K_IPAT1_AD1=1','SMP.NESTED_BSP_TRACE','SMP.NESTED_AP_TRACE','N47B_ALL_BACKING_RETAINED=1'):
 found=[]
 for n,section in enumerate(sections):
  if not section[2]&2 or section[1]==8:continue
  data=elf[section[4]:section[4]+section[5]];offset=data.find(marker.encode())
  if offset<0:continue
  address=section[3]+offset
  refs=[line for line in disassembly.splitlines() if re.search(r'(?:0x|# )0*'+format(address,'x')+r'\b',line)]
  if refs:found.append({'section':n,'address':hex(address),'code_references':refs})
 assert found,('missing allocated code-referenced terminal emission',marker)
 emissions.append({'marker':marker,'locations':found})
(work/'terminal-emissions.json').write_text(json.dumps({'allocated_code_referenced':emissions,'terminal_execution_test':'nested47b_terminal.py','not_hardware_reachability':True},indent=2)+'\n')
print('PASS N47B final PASS, both owner trace delimiters and retained-backing marker survive in allocated ELF and have code references')

symbols=subprocess.check_output(['nm',str(work/'efi/build/krnl-smp-nested-source-replace.elf')],text=True)
for symbol in ('nrc_init_atomic_wb_successor','ad_updater_gate_init_successor','tsr_init','tsr_finish','nested_domain_dispatch_source_stopped','nosr_bind','nosr_validate','nsrr_run','nested_source_replace_fixture_change'):
 assert any(line.endswith(' T '+symbol) for line in symbols.splitlines()),symbol
obj=work/'source-memory.o'
cmd=['cc','-std=c11','-O1','-ffreestanding','-mgeneral-regs-only','-DSYLE_KERNEL','-Wall','-Wextra','-Werror','-c',str(work/'krnl/nested_source_replace_memory.c'),'-o',str(obj)]
r=subprocess.run(cmd,capture_output=True,text=True);(work/'memory-build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
u=subprocess.check_output(['nm','-u',str(obj)],text=True);assert not {'memset','memcpy','memmove','bzero'}&{line.split()[-1] for line in u.splitlines()}
(work/'memory-undefined.log').write_text(u)
print('PASS N47B new core symbols and actual WB kernel/ESP link; no undefined symbols; no source-replacement hardware claim')
