"""Static assertions for the isolated default-kernel-shaped link proof only."""
import hashlib,json,pathlib,subprocess,sys
build,out=map(pathlib.Path,sys.argv[1:])
def cmd(*args):return subprocess.check_output(args,text=True)
elf=out/'authority-link-proof.elf'
objects=sorted(build.glob('*.o'))
assert not (build/'k-memory.o').exists()
assert (build/'k-authority.o').is_file()
assert objects
for p in objects:
    assert '.gnu.lto_' not in cmd('readelf','-SW',str(p)),p
    assert not any(t in cmd('nm','-u',str(p)) for t in ('pmm_raw_','pmm_owned_','vlm_')),p
symbols=cmd('nm','-g','--defined-only',str(elf))
assert not any(tag in symbols for tag in ('pmm_raw_','pmm_owned_','vlm_','pna_test_'))
for name in ('pmm_init','pmm_free_page','pmm_alloc_page','pna_start','pna_end','vro_wb_publish','kernel_entry'):
    assert sum(line.split()[-1]==name for line in symbols.splitlines())==1,name
if any(p.name.endswith('linux_guest_memory.o') for p in objects):
    for name in ('lgm_prepare','lgm_prepare_roles','lvh_open','lvh_open_roles'):
        assert sum(line.split()[-1]==name for line in symbols.splitlines())==1,name
assert not cmd('nm','-u',str(elf)).strip()
values={line.split()[-1]:int(line.split()[0],16) for line in symbols.splitlines()}
assert values['__kernel_start']==0x04000000
assert 0<values['__kernel_end']-values['__kernel_start']<=0x800000
assert 'EXEC (Executable file)' in cmd('readelf','-h',str(elf))
assert 'There are no relocations' in cmd('readelf','-r',str(elf))
assert 'There is no dynamic section' in cmd('readelf','-d',str(elf))
assert 'mfence' in cmd('objdump','-d','--disassemble=vro_wb_publish',str(elf))
assert 'rdmsr' in cmd('objdump','-d',str(elf))
linkmap=(out/'kernel.map').read_text()
assert 'build/k-memory.o' not in linkmap and 'build/k-authority.o' in linkmap
sources={}
for d in build.glob('*.d'):
    deps=d.read_text().replace('\\\n',' ').splitlines()[0].split(':',1)[1].split()
    for name in deps:
        p=pathlib.Path(name)
        if not p.is_absolute():p=build.parent/p
        if p.is_file():sources[str(p.resolve())]=hashlib.sha256(p.read_bytes()).hexdigest()
here=pathlib.Path(__file__).resolve().parent
for name in ('native_candidate.c','native_candidate.h','lease_model.c','lease_model.h','pmm_authority.c','pmm_authority.h','wb_publish.c','wb_publish.h','../../krnl/memory.c','../../krnl/memory.h','../../krnl/link.ld','../../efi/Makefile'):
    p=(here/name).resolve();sources[str(p)]=hashlib.sha256(p.read_bytes()).hexdigest()
manifest={'sources':sources,'kernel_memory_bytes':values['__kernel_end']-values['__kernel_start'],'scope':'static final-link proof only; NOT booted, no DMA integration',
          'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),
          'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objects},
          'elf_bytes':elf.stat().st_size,'no_lto_sections':True,'native_executed':False}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('final ELF static checks: PASS (no native runtime proof)')
