"""Retain observed build inputs for replay; this is not a native execution claim."""
import hashlib,json,pathlib,shutil,subprocess,sys
out=pathlib.Path(sys.argv[1]);here=pathlib.Path(__file__).resolve().parent;repo=here.parent.parent
m=json.loads((out/'manifest.json').read_text())
files={pathlib.Path(p):h for p,h in m['sources'].items()}
# Build control/assembly/input-checker sources aren't all emitted in C .d files.
for root,pattern in [(repo/'efi','*.mk'),(repo/'krnl','*.S'),(repo/'efi/src','*.S'),(repo/'guest/linux-baseline','*.py'),(here,'*.sh'),(here,'*.py'),(here,'*.c'),(here,'*.h')]:
    for p in root.rglob(pattern):
        if 'build' not in p.parts:files[p.resolve()]=hashlib.sha256(p.read_bytes()).hexdigest()
for p,h in files.items():
    assert hashlib.sha256(p.read_bytes()).hexdigest()==h,('source changed after manifest',p)
    dest=out/'source_snapshot'/p.relative_to(p.anchor);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,dest)
    assert hashlib.sha256(dest.read_bytes()).hexdigest()==h
m['source_snapshot']={str(p):h for p,h in files.items()}
m['toolchain']={t:subprocess.check_output([t,'--version'],text=True).splitlines()[0] for t in ['cc','gcc','ld','objcopy','nm','readelf']}
if len(sys.argv)>2:
    inputs=pathlib.Path(sys.argv[2]);dest=out/'inputs';dest.mkdir(exist_ok=True)
    for name,h in m['inputs'].items():
        shutil.copyfile(inputs/name,dest/name);assert hashlib.sha256((dest/name).read_bytes()).hexdigest()==h
m['replay_note']='Use captured sources/build scripts with captured EFI header root and inputs; build paths/debug data may alter binary hash. No boot/typed callback/DMA integration.'
(out/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(f'proof snapshot retained: {len(files)} source/control files')
