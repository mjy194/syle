#!/usr/bin/env python3
"""MS1 058-064: exact-image debugger, step/read, invalid read, detach/reconnect.
The selected policy is QEMU detach resumes and reconnect stops. It is not a
promise for a lost TCP connection, physical probes, or arbitrary firmware.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import shutil
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
OUT=ROOT/'build/ms01-debugger'; OUT.mkdir(parents=True,exist_ok=True)

def stop(p):
    if p.poll() is None:
        p.terminate()
        try: p.wait(timeout=5)
        except subprocess.TimeoutExpired: p.kill();p.wait()

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def verify_identity(manifest):
    for path,expected in manifest.items():
        if sha(path)!=expected: raise ValueError('Debug symbol/image identity mismatch: '+path)

subprocess.run(['make','-j4','all'],check=True)
esp=OUT/'esp'
shutil.rmtree(esp,ignore_errors=True);shutil.copytree(ROOT/'build/esp',esp)
runenv=dict(os.environ,ESP_DIR=str(esp),QEMU_GDB_PORT='12435',OVMF_VARS_RUNTIME=str(OUT/'vars.fd'))
manifest={str(p):sha(p) for p in map(Path,['build/stage0.so','build/esp/EFI/BOOT/BOOTX64.EFI','build/krnl.elf','build/esp/syle/krnl.elf'])}
verify_identity(manifest)
wrong=dict(manifest);wrong['build/krnl.elf']='0'*64
try: verify_identity(wrong)
except ValueError: print('PASS DRAFT-MS1-062 mismatched identity rejected before debugger connection')
else: raise AssertionError('mismatch accepted')
assert sha('build/krnl.elf')==sha('build/esp/syle/krnl.elf')
(OUT/'image-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
# Read the exact ELF PT_LOAD bytes at entry. Debugger compares actual target
# memory before attributing any instruction to the supplied kernel symbols.
blob=Path('build/krnl.elf').read_bytes()
entry=struct.unpack_from('<Q',blob,24)[0]
phoff=struct.unpack_from('<Q',blob,32)[0]
phsize,phnum=struct.unpack_from('<HH',blob,54)
expected=None
for i in range(phnum):
    typ,flags,off,va,pa,filesz,memsz,align=struct.unpack_from('<IIQQQQQQ',blob,phoff+i*phsize)
    if typ==1 and va<=entry<va+filesz:
        expected=blob[off+entry-va:off+entry-va+16].hex();break
assert expected
pre=OUT/'normal-serial.log'
with pre.open('w') as log:
    p=subprocess.Popen(['scripts/run.sh'],stdout=log,stderr=subprocess.STDOUT,env=runenv)
    try:
        deadline=time.monotonic()+90
        while time.monotonic()<deadline:
            txt=pre.read_text(errors='replace')
            if 'KRNL.DIAGNOSTIC_IDLE' in txt or 'KRNL.IDLE' in txt:break
            if p.poll() is not None:raise AssertionError(txt)
            time.sleep(.1)
        else:raise TimeoutError(txt)
        assert 'STAGE0.POST_EBS' in txt
        base=re.search(r'IMAGE.BASE=(0x[0-9a-f]+)',txt)[1]
    finally:stop(p)
print('PASS DRAFT-MS1-064 normal boot reaches idle without debugger')
serial=OUT/'debug-serial.log'
driver=OUT/'driver.py'
driver.write_text('''import gdb

def check(ok, message):
    if not ok: raise gdb.GdbError(message)
def reach(symbol):
    address=int(gdb.parse_and_eval('&'+symbol))
    bp=gdb.Breakpoint('*'+symbol,type=gdb.BP_HARDWARE_BREAKPOINT,temporary=True)
    gdb.execute('continue')
    check(int(gdb.parse_and_eval('$pc'))==address and not bp.is_valid(),'unexpected breakpoint: '+symbol)
    gdb.write('MS01.DEBUG.HIT '+symbol+'\\n')
reach('efi_main')
reach('stage0_post_ebs')
gdb.execute('add-symbol-file build/krnl.elf')
reach('kernel_entry')
check(int(gdb.parse_and_eval('$rip'))==ENTRY,'ELF entry mismatch')
check(bytes(gdb.selected_inferior().read_memory(ENTRY,16)).hex()==EXPECTED,'target code differs from exact ELF')
old=int(gdb.parse_and_eval('$rip'))
gdb.execute('stepi')
check(int(gdb.parse_and_eval('$rip'))==old+1,'CLI instruction did not single step')
gdb.write('MS01.DEBUG.STEP_READ_PASS\\n')
reach('kernel_runtime')
rsp=int(gdb.parse_and_eval('$rsp'))
check(int(gdb.parse_and_eval('&kernel_startup_stack'))<=rsp<int(gdb.parse_and_eval('&kernel_startup_stack_top')),'not own stack')
# Canonical high hole in this BSP image, rather than reading a valid target.
try:
    gdb.selected_inferior().read_memory(0xffffc00000000000,16)
except gdb.MemoryError:
    gdb.write('MS01.DEBUG.INVALID_READ_REJECTED\\n')
else:
    raise gdb.GdbError('unmapped read unexpectedly succeeded')
# Explicit detach policy: QEMU resumes, then target remote stops it again.
gdb.execute('detach')
gdb.execute('target remote 127.0.0.1:12435')
check(gdb.selected_thread().is_stopped(),'reconnect did not stop target')
gdb.execute('info registers rip rsp eflags')
gdb.write('MS01.DEBUG.RECONNECT_STOPPED\\n')
gdb.execute('detach')
'''.replace('ENTRY',str(entry)).replace('EXPECTED',repr(expected)))
with serial.open('w') as log:
    p=subprocess.Popen(['scripts/run.sh','debug'],stdout=log,stderr=subprocess.STDOUT,env=runenv)
    try:
        time.sleep(.5)
        verify_identity(manifest)
        r=subprocess.run([os.environ.get('GDB','gdb'),'-q','-batch',
          '-ex','set pagination off','-ex','set confirm off',
          '-ex','set tcp auto-retry on','-ex','set remotetimeout 10',
          '-ex','target remote 127.0.0.1:12435',
          '-ex',f'add-symbol-file build/stage0.so -o {base}',
          '-ex',f'source {driver}'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
        (OUT/'gdb.log').write_text(r.stdout)
        print(r.stdout)
        assert r.returncode==0
        for marker in ['HIT efi_main','HIT stage0_post_ebs','HIT kernel_entry','HIT kernel_runtime','STEP_READ_PASS','INVALID_READ_REJECTED','RECONNECT_STOPPED']:
            assert 'MS01.DEBUG.'+marker in r.stdout,marker
        assert f'IMAGE.BASE={base}' in serial.read_text(errors='replace')
    finally:stop(p)
print('PASS DRAFT-MS1-058..064: current QEMU debug contract; abrupt TCP loss remains distinct')
