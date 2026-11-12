#!/usr/bin/env python3
"""QEMU/OVMF high physical RAM is allocated, accessed, freed and inherited."""
import os, subprocess, shutil, time, re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
os.chdir(root)
subprocess.run(['make','-j8','all'],check=True)
for memory in ('256M','5G','8G'):
    esp=root/'build'/f'high-{memory}-esp'
    shutil.rmtree(esp,ignore_errors=True)
    shutil.copytree(root/'build/esp',esp)
    (esp/'syle/boot.cfg').write_text('timeout=0\n')
    path=root/'build'/f'high-{memory}.log'
    with path.open('w') as log:
        proc=subprocess.Popen(['scripts/run.sh'],env=dict(os.environ,ESP_DIR=str(esp),QEMU_MEMORY=memory),stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+120
            while time.monotonic()<deadline:
                text=path.read_text(errors='replace')
                if 'KRNL.IDLE' in text: break
                if 'PANIC' in text or proc.poll() is not None: raise AssertionError(text)
                time.sleep(.1)
            else: raise AssertionError(text)
            assert 'ERROR.' not in text and 'PANIC' not in text,text
            if memory=='256M': assert 'PMM.NO_FREE_HIGH_RAM' in text,text
            else:
                match=re.search(r'PMM.HIGH_WRITE_READ_FREE_PASS physical=0x([0-9a-f]+)',text)
                assert match and int(match[1],16)>=0x100000000,text
                assert 'RING3.HIGH_SUPERVISOR_MAPPING_PASS' in text,text
                pages=int(re.search(r'PMM.FREE_PAGES=0x([0-9a-f]+)',text)[1],16)
                assert pages>0x100000,text
            print(f'PASS {memory}: default boot, high allocation/access/free and user-root inheritance')
        finally:
            proc.terminate()
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait()

# Reuse the protected-kernel-address read test; with high RAM its target is a
# proven-present high direct-map page, so #PF error 5 proves privilege denial.
subprocess.run(['make','ring3'],check=True)
path=root/'build/high-user-denial.log'
with path.open('w') as log:
    result=subprocess.run(['scripts/run.sh','ring3-1'],env=dict(os.environ,QEMU_MEMORY='5G'),stdout=log,stderr=subprocess.STDOUT,timeout=120)
text=path.read_text(errors='replace')
assert result.returncode==33,text
assert 'RING3.HIGH_USER_DENIAL_TEST' in text,text
assert 'RING3.USER_FAULT error=0x0000000000000005' in text,text
assert 'RING3.PASS' in text and 'PANIC' not in text,text
print('PASS 5G: user read of present high supervisor mapping denied (#PF error 5)')
