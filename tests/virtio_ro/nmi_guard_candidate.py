#!/usr/bin/env python3
"""Generate isolated copies only; fail if exact source anchors change."""
import pathlib,sys
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(sys.argv[1]);host='--host' in sys.argv
for name in ['efi/src/exceptions.c','krnl/ring3.c']:
 s=(repo/name).read_text()
 old,new=(('if (ring3_fault) ring3_fault(f, cr2);','if (f->vector != 2 && ring3_fault) ring3_fault(f, cr2);') if name.startswith('efi') else ('if ((f->cs & 3) != 3) return;','if (f->vector == 2 || (f->cs & 3) != 3) return;'))
 assert s.count(old)+s.count(new)==1,(name,'ambiguous guard anchor')
 if '--patched' in sys.argv:s=s.replace(old,new)
 else:s=s.replace(new,old) # explicit historical baseline comparison
 if '--probe' in sys.argv and name=='krnl/ring3.c':
  old='static void guest_owner_exit(void) {';assert s.count(old)==1
  s=s.replace(old,'volatile uint64_t nmi_probe_owner_exit_calls;\n'+old+'\n    ++nmi_probe_owner_exit_calls;')
 if host and name.startswith('efi'):
  old='__asm__ volatile("mov %%cr2,%0" : "=r"(cr2));';assert s.count(old)==1;s=s.replace(old,'cr2 = 0; /* HOST TEST: no CR2 instruction */')
  old='__asm__ volatile("cli" ::: "memory");';assert s.count(old)==1;s=s.replace(old,'(void)0; /* HOST TEST: no CLI instruction */')
 target=out/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(s)
