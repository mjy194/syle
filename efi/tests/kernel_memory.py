#!/usr/bin/env python3
"""Hardware page permission faults after early kernel CR3 switch (QEMU TCG)."""
import os, subprocess, shutil, time
from pathlib import Path
root = Path(__file__).resolve().parents[1]
os.chdir(root)
subprocess.run(['make', '-j8', 'all', 'memory-checks', 'build/krnl-memory-4.elf', 'build/krnl-memory-5.elf'], check=True)
for case, error in ((1,3),(2,17),(3,0),(4,3),(5,17)):
    esp=root/'build'/f'memory-{case}-esp'
    shutil.rmtree(esp,ignore_errors=True)
    shutil.copytree(root/'build/esp',esp)
    shutil.copy(root/'build'/f'krnl-memory-{case}.elf',esp/'syle/krnl.elf')
    (esp/'syle/boot.cfg').write_text('timeout=0\n')
    path=root/'build'/f'memory-{case}.log'
    with path.open('w') as log:
        proc=subprocess.Popen(['scripts/run.sh'],env=dict(os.environ,ESP_DIR=str(esp)),stdout=log,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+60
            while time.monotonic()<deadline:
                text=path.read_text(errors='replace')
                if 'PANIC.COMPLETE' in text: break
                if proc.poll() is not None: raise AssertionError(text)
                time.sleep(.1)
            else: raise AssertionError(text)
            assert 'KRNL.EARLY_OWN_CR3=' in text and 'MEMORY.PROTECTION_TEST' in text,text
            assert 'vector=0x000000000000000e' in text,text
            assert f'error=0x{error:016x}' in text,text
            assert 'MEMORY PROTECTION FAILED' not in text,text
            print(f'PASS early paging case {case} #PF error={error}')
        finally:
            proc.terminate()
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired: proc.kill();proc.wait()
