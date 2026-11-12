#!/usr/bin/env python3
"""SC-M1-063: SIGKILL a real connected GDB (no detach), observe via QMP.
QEMU-specific observed behavior is pinned by assertions; this does not turn a
candidate disconnect policy into a user-confirmed cross-debugger requirement.
"""
import fcntl, hashlib, json, os
from pathlib import Path
import shutil, socket, subprocess, time
ROOT=Path(__file__).resolve().parents[1];os.chdir(ROOT)
OUT=ROOT/'build/ms01-debugger-abrupt';OUT.mkdir(parents=True,exist_ok=True)
# A failed rerun must not leave a previous successful component report.
(OUT/'report.json').unlink(missing_ok=True)
PORT=12444;QMP=14444

def stop(p):
 if p is not None and p.poll() is None:
  p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()

def wait_until(predicate,timeout=90):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  value=predicate()
  if value:return value
  time.sleep(.05)
 raise TimeoutError('condition did not occur')

with (ROOT/'build/.ms01-build.lock').open('w') as lock:
 fcntl.flock(lock,fcntl.LOCK_EX)
 subprocess.run(['make','-j4','all'],check=True)
 esp=OUT/'esp';shutil.rmtree(esp,ignore_errors=True);shutil.copytree(ROOT/'build/esp',esp)
 (esp/'syle/boot.cfg').write_text('default=diagnostic\ntimeout=0\n')
 elf=OUT/'krnl.elf';shutil.copyfile(ROOT/'build/krnl.elf',elf)
manifest={str(p.relative_to(OUT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [elf,esp/'syle/krnl.elf',esp/'EFI/BOOT/BOOTX64.EFI']}
assert manifest['krnl.elf']==manifest['esp/syle/krnl.elf']
(OUT/'image-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
results=[]
for mode in ('stopped','running'):
 ready=OUT/(mode+'.ready');ready.unlink(missing_ok=True)
 driver=OUT/(mode+'.gdb')
 driver.write_text(f'''set pagination off
set confirm off
file {elf}
target remote 127.0.0.1:{PORT}
hbreak vmx_probe
continue
delete breakpoints
python
import gdb
from pathlib import Path
assert int(gdb.parse_and_eval('$rip')) == int(gdb.parse_and_eval('&vmx_probe'))
Path({str(ready)!r}).write_text(str(int(gdb.parse_and_eval('$rip'))))
end
'''+('continue\n' if mode=='running' else 'python import time; time.sleep(1000)\n'))
 serial=OUT/(mode+'.serial.log');glog=OUT/(mode+'.gdb.log');qlog=[]
 proc=None;gdb=None;sock=None
 with serial.open('w') as stream,glog.open('w') as gs:
  try:
   env=dict(os.environ,ESP_DIR=str(esp),QEMU_GDB_PORT=str(PORT),QMP_PORT=str(QMP),OVMF_VARS_RUNTIME=str(OUT/(mode+'-vars.fd')))
   proc=subprocess.Popen(['scripts/run.sh','debug'],env=env,stdout=stream,stderr=subprocess.STDOUT)
   def connect():
    global sock
    try:sock=socket.create_connection(('127.0.0.1',QMP),timeout=2);return True
    except ConnectionRefusedError:return False
   wait_until(connect,10);f=sock.makefile('rwb',buffering=0);json.loads(f.readline())
   def qmp(command):
    f.write((json.dumps({'execute':command})+'\n').encode())
    while True:
     response=json.loads(f.readline());qlog.append(response)
     if 'error' in response:raise AssertionError(response)
     if 'return' in response:return response['return']
   qmp('qmp_capabilities')
   gdb=subprocess.Popen([os.environ.get('GDB','gdb'),'-q','-batch','-x',str(driver)],stdout=gs,stderr=subprocess.STDOUT)
   wait_until(lambda:ready.exists() or (gdb.poll() is not None and (_ for _ in ()).throw(AssertionError(glog.read_text()))))
   if mode=='running':
    wait_until(lambda:'KRNL.DIAGNOSTIC_IDLE' in serial.read_text(errors='replace'))
   before=qmp('query-status');assert before['running']==(mode=='running'),before
   # SIGKILL cannot run GDB cleanup or send the remote D (detach) packet.
   gdb.kill();assert gdb.wait(timeout=5)==-9
   samples=[]
   for i in range(6):
    time.sleep(.1);samples.append(qmp('query-status'))
   (OUT/(mode+'.qmp.json')).write_text(json.dumps({'before':before,'after':samples,'protocol':qlog},indent=2)+'\n')
   # Observed baseline for this QEMU launch: abrupt loss preserves run state.
   expected_status='debug' if mode=='stopped' else 'running'
   assert all(v['running']==(mode=='running') and v['status']==expected_status for v in samples),samples
   print('PASS abrupt state retained',mode,expected_status,flush=True)
   # Reconnect, prove target state is inspectable, then explicitly resume.
   reconnect=OUT/(mode+'-reconnect.gdb')
   reconnect.write_text(f'''set pagination off
set confirm off
file {elf}
target remote 127.0.0.1:{PORT}
python
import gdb
assert gdb.selected_thread().is_stopped()
assert int(gdb.parse_and_eval('$rip')) != 0
assert {mode!r} != 'stopped' or int(gdb.parse_and_eval('$rip')) == {int(ready.read_text())}
print('RECONNECTED_STOPPED')
end
info registers rip rsp eflags
detach
quit
''')
   r=subprocess.run([os.environ.get('GDB','gdb'),'-q','-batch','-x',str(reconnect)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=20)
   (OUT/(mode+'-reconnect.log')).write_text(r.stdout)
   assert r.returncode==0 and 'RECONNECTED_STOPPED' in r.stdout,r.stdout
   wait_until(lambda:'KRNL.DIAGNOSTIC_IDLE' in serial.read_text(errors='replace'))
   results.append({'case':'abrupt-'+mode,'before':before,'after':samples,'reconnect':'stopped and registers readable','termination':'SIGKILL, no graceful detach','policy':'abrupt loss retains prior stopped/running state for this QEMU launch','postLossSamples':6})
  finally:
   stop(gdb)
   if sock:sock.close()
   stop(proc)
(OUT/'report.json').write_text(json.dumps(results,indent=2)+'\n')
print('PASS SC-M1-063 abrupt stopped/running debugger loss and reconnect')
