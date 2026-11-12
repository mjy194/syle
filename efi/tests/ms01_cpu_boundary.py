#!/usr/bin/env python3
"""Debugger-directed ABI/state negatives; actual CPUID NX absence in TCG."""
import json, os, pathlib, shutil, subprocess, time
root=pathlib.Path(__file__).resolve().parents[1]; os.chdir(root)
subprocess.run(['make','all'],check=True,stdout=subprocess.DEVNULL)
out=root/'build/ms01-cpu-boundary';out.mkdir(exist_ok=True)
esp=out/'esp'
if esp.exists():shutil.rmtree(esp)
shutil.copytree(root/'build/esp',esp)
elf=out/'krnl.elf';shutil.copyfile(root/'build/krnl.elf',elf)
cases={
 'invalid-abi': ('kernel_runtime','set ((struct syle_boot_info*)$rdi)->version = 0\n','exception_stop','INVALID BOOT INFO'),
 'invalid-framebuffer': ('kernel_runtime','set ((struct syle_boot_info*)$rdi)->framebuffer.width = 1\n','vmx_probe','KRNL.EARLY_OWN_CR3='),
 'normalize-wp-nxe': ('paging_init','set $cr0 = $cr0 & ~(1 << 16)\nset $efer = $efer & ~(1 << 11)\n','vmx_probe','KRNL.PAGING_RX_RO_NX_NULL_GUARD'),
 'missing-nx': ('paging_init','','exception_stop','PAGING NX REQUIRED'),
}
results=[]
for name,(checkpoint,setup,stop,marker) in cases.items():
 script=out/(name+'.gdb'); log=out/(name+'.serial.log')
 cmd=f'set pagination off\nset confirm off\nfile {elf}\ntarget remote :1234\nhbreak {checkpoint}\ncontinue\ndelete breakpoints\n{setup}hbreak {stop}\ncontinue\n'
 if name=='invalid-framebuffer':
  cmd+='python\nimport gdb\nassert int(gdb.parse_and_eval("screen.pixels")) == 0\nassert int(gdb.parse_and_eval("kernel_runtime::normalized.framebuffer.valid")) == 0\nend\n'
 if name=='normalize-wp-nxe':
  cmd+='python\nimport gdb\nassert int(gdb.parse_and_eval("$cr0")) & (1<<16)\nassert int(gdb.parse_and_eval("$efer")) & (1<<11)\nassert not int(gdb.parse_and_eval("$cr4")) & ((1<<7)|(1<<12)|(1<<17)|(1<<21))\nend\n'
 if stop=='exception_stop':
  cmd+='stepi\npython\nimport gdb\nassert not (int(gdb.parse_and_eval("$eflags")) & 512)\nassert "hlt" in gdb.execute("x/i $pc",to_string=True)\nend\n'
 cmd+='echo BOUNDARY_PASS\\n\ndetach\nquit\n';script.write_text(cmd)
 env=dict(os.environ,ESP_DIR=str(esp),QEMU_CPU='max,-nx' if name=='missing-nx' else 'max')
 with log.open('w') as stream:
  proc=subprocess.Popen(['scripts/run.sh','debug'],env=env,stdout=stream,stderr=subprocess.STDOUT)
  try:
   time.sleep(.5)
   run=subprocess.run([os.environ.get('GDB','gdb'),'-q','-batch','-x',str(script)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
   (out/(name+'.gdb.log')).write_text(run.stdout)
   assert run.returncode==0 and 'BOUNDARY_PASS' in run.stdout,run.stdout
   time.sleep(.2);assert marker in log.read_text(errors='replace'),log.read_text(errors='replace')
   results.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
  finally:
   proc.terminate()
   try:proc.wait(timeout=5)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
(out/'report.json').write_text(json.dumps(results,indent=2)+'\n')
