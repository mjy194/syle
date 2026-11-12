#!/usr/bin/env python3
"""BSP physical-page W^X: supervisor writes via both aliases fault after seal.
Run serially with other QEMU tests: scripts/run.sh owns build/vars-run.fd.
"""
import os, re, shutil, subprocess, time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
os.chdir(root)
subprocess.run(['make','-j8','-f','Makefile','-f','tests/wx_alias.mk','all','wx-alias'],check=True)
for case in (1,2,3):
    esp=root/'build'/f'wx-{case}-esp'
    shutil.rmtree(esp,ignore_errors=True)
    shutil.copytree(root/'build/esp',esp)
    shutil.copy(root/'build'/f'krnl-wx-{case}.elf',esp/'syle/krnl.elf')
    (esp/'syle/boot.cfg').write_text('timeout=0\n')
    path=root/'build'/f'wx-{case}.log'
    with path.open('w') as log:
        proc=subprocess.Popen(['scripts/run.sh'],env=dict(os.environ,ESP_DIR=str(esp)),stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+90
            while time.monotonic()<deadline:
                text=path.read_text(errors='replace')
                if 'PANIC.COMPLETE' in text: break
                if proc.poll() is not None: raise AssertionError(text)
                time.sleep(.1)
            else: raise AssertionError(text)
            assert 'RING3.ALL_CODE_ALIASES_RO' in text,text
            target=re.search(r'WX.ALIAS_WRITE_TEST address=0x([0-9a-f]+)',text)
            assert target,text
            assert 'vector=0x000000000000000e' in text,text
            assert 'error=0x0000000000000003' in text,text
            assert f'cr2=0x{int(target[1],16):016x}' in text.lower(),text
            assert 'WX ALIAS WRITE WAS ALLOWED' not in text,text
            stage='seal flush before any root switch' if case==3 else 'post-seal active user root'
            print(f'PASS W^X alias {case}: #PF error=3, exact CR2, {stage}')
        finally:
            proc.terminate()
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired: proc.kill();proc.wait()
