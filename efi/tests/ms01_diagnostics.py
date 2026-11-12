#!/usr/bin/env python3
"""TCG/OVMF CPU faults plus debugger-directed adverse-state injection.
Never claim that INT 8 is #DF or INT 2 is NMI. Firmware/platform hardware
coverage is explicitly outside these emulator tests.
"""
import json, os, pathlib, subprocess, time, shutil
root = pathlib.Path(__file__).resolve().parents[1]
os.chdir(root)
subprocess.run(['make','all'],check=True,stdout=subprocess.DEVNULL)
out=root/'build/ms01-diagnostics';out.mkdir(exist_ok=True)
(out/'report.json').unlink(missing_ok=True)
# Immutable test inputs prevent concurrent aggregate builds changing FAT content.
esp=out/'esp'
if esp.exists(): shutil.rmtree(esp)
shutil.copytree(root/'build/esp',esp)
elf=out/'krnl.elf'; shutil.copyfile(root/'build/krnl.elf',elf)
# Instructions execute on the CPU after GDB patches a stopped kernel text site.
cases = {
 'ud': ('0x0f,0x0b', '', 6),
 'divide': ('0x48,0x31,0xd2,0xb8,1,0,0,0,0x48,0x31,0xc9,0x48,0xf7,0xf1', '', 0),
 'breakpoint': ('0xcc', '', 3),
 'gp': ('0xb8,0xff,0xff,0,0,0x8e,0xd0', '', 13), # mov $ffff,eax; mov ax,ss
 'pf': ('0x48,0x31,0xc0,0x48,0x8b,0', '', 14), # read unmapped null
 'df': ('0x0f,0x0b', 'set tss.ist[0] = 0\n', 8), # failed exception stack delivery
 'fallback': ('0xcd,0xff', '', 255),
 'fallback-32': ('0xcd,0x20', '', 32),
 'fallback-127': ('0xcd,0x7f', '', 127),
 'fallback-128': ('0xcd,0x80', '', 128),
 'fallback-254': ('0xcd,0xfe', '', 254),
 'screen-fault': ('0x0f,0x0b', 'set screen.pixels = (void*)0xffff800000000000\n', 6),
 'no-screen': ('0x0f,0x0b', 'set screen.pixels = 0\n', 6),
 'no-uart': ('0x0f,0x0b', 'set uart_present = 0\n', 6),
 'no-channels': ('0x0f,0x0b', 'set uart_present = 0\nset screen.pixels = 0\n', 6),
 'nested-guard': ('0x0f,0x0b', 'set {unsigned char}&exception_entered = 1\n', None),
 'nmi': (None, 'monitor nmi\n', 2),
 'nmi-repeat-request': (None, 'monitor nmi\n', 2),
 'nmi-repeat-guard': (None, 'set {unsigned char}&nmi_entered = 1\nmonitor nmi\n', None),
}
results=[]
for name,(code,setup,vector) in cases.items():
    if os.environ.get('DIAG_CASES') and name not in os.environ['DIAG_CASES'].split(','): continue
    log=out/(name+'.serial.log');glog=out/(name+'.gdb.log');script=out/(name+'.gdb')
    # vmx_probe is reached only after kernel GDT/IDT/IST and own paging exist.
    command=f'''set pagination off
set confirm off
set architecture i386:x86-64
file {elf}
target remote :1234
hbreak vmx_probe
continue
delete breakpoints
python
import gdb
assert int(gdb.parse_and_eval('sizeof(idt)')) == 4096
for i in range(256):
    assert int(gdb.parse_and_eval('idt[%d].flags'%i)) == 0x8e
    assert int(gdb.parse_and_eval('idt[%d].selector'%i)) == 8
    gate=gdb.parse_and_eval('idt[%d]'%i)
    address=int(gate['offset_low']) | (int(gate['offset_mid'])<<16) | (int(gate['offset_high'])<<32)
    assert address==int(gdb.parse_and_eval('&exception_%d'%i))
assert int(gdb.parse_and_eval('$cr0')) & (1<<16)
assert int(gdb.parse_and_eval('$efer')) & (1<<11)
assert not (int(gdb.parse_and_eval('$eflags')) & 512)
assert int(gdb.parse_and_eval('tss.ist[0]')) != int(gdb.parse_and_eval('tss.ist[1]'))
assert int(gdb.parse_and_eval('tss.ist[0]')) != int(gdb.parse_and_eval('tss.ist[2]'))
end
{setup.replace('monitor nmi'+chr(10), '')}'''
    if code:
        command+='set $rbx = 0x123456789abcdef0\n'
        data=code.split(',')
        command+='set $probe = $rip\n'
        for i,b in enumerate(data):command+=f'set {{unsigned char}}($probe+{i}) = {b}\n'
    if vector is not None:
        command+='hbreak exception_panic\n' + ('monitor nmi\n' if name in ('nmi','nmi-repeat-request') else '') + 'continue\n'
        stack = 'double_fault_stack' if vector==8 else 'nmi_stack' if vector==2 else 'exception_stack'
        command+=f"python\nf=gdb.parse_and_eval('(struct frame*)$rdi')\nassert int(f['vector']) == {vector}\nassert int(gdb.parse_and_eval('&{stack}')) <= int(gdb.parse_and_eval('$rdi')) < int(gdb.parse_and_eval('&{stack}')) + int(gdb.parse_and_eval('sizeof({stack})'))\nend\ndelete breakpoints\n"
    if code and vector is not None:
        rip_offset = 11 if name=='divide' else 1 if name=='breakpoint' else 5 if name=='gp' else 3 if name=='pf' else 2 if name.startswith('fallback') else 0
        error = 0xfffc if name=='gp' else 0
        # #DF saved RIP is architecturally undefined; do not assert it.
        if name!='df': command+=f"python\nassert int(f['rip']) == int(gdb.parse_and_eval('$probe')) + {rip_offset}\nassert int(f['rbx']) == 0x123456789abcdef0\nend\n"
        command+=f"python\nassert int(f['error']) == {error}\nend\n"
        if name=='pf': command+="python\nassert int(gdb.parse_and_eval('$cr2')) == 0\nend\n"
    if name=='nmi-repeat-request':
        command+='monitor nmi\n'
    command+='hbreak exception_stop\n'  + ('monitor nmi\n' if name=='nmi-repeat-guard' else '') + 'continue\n' 
    if vector is not None:
        command+=f'''python
# First exception frame sits at the dedicated IST top. #DF uses IST2, NMI IST3.
# RIP and error code are tested through the serial dump when UART is enabled.
assert int(gdb.parse_and_eval('*(unsigned char*)&exception_entered')) == 1
end
'''
    if name in ('nmi','nmi-repeat-request'):
        command+="python\nassert int(gdb.parse_and_eval('tss.ist[2]')) == int(gdb.parse_and_eval('&nmi_spare_top'))\nend\n"
    if name=='no-uart':
        command+="python\nassert int(gdb.parse_and_eval('screen.pixels')) != 0\nassert int(gdb.parse_and_eval('screen.y')) > 0\nassert int(gdb.parse_and_eval('screen.background')) in (0x58080c,0x0c0858)\nassert int(gdb.parse_and_eval('screen.pixels[screen.width-1]')) == int(gdb.parse_and_eval('screen.background'))\nprint('FRAMEBUFFER_PANIC_RENDERED')\nend\n"
    # Observe the CLI-to-HLT endpoints instead of relying on debugger single-step.
    # Never assign guest RIP here: continue must execute the real CLI instruction.
    command+='set $halt_entry = (unsigned long)&exception_stop\npython\nassert int(gdb.parse_and_eval("$rip")) == int(gdb.parse_and_eval("$halt_entry"))\nassert bytes(gdb.selected_inferior().read_memory(int(gdb.parse_and_eval("$halt_entry")),2)) == bytes([0xfa,0xf4])\nend\ndelete breakpoints\nhbreak *($halt_entry+1)\ncontinue\ndelete breakpoints\npython\nassert int(gdb.parse_and_eval("$rip")) == int(gdb.parse_and_eval("$halt_entry"))+1\nassert (int(gdb.parse_and_eval("$eflags")) & 512) == 0\nassert "hlt" in gdb.execute("x/i $pc",to_string=True)\nprint("SAFE_HALT_IF0")\nend\ndetach\nquit\n'
    script.write_text(command)
    with log.open('w') as stream:
        proc=subprocess.Popen(['scripts/run.sh','debug'],env=dict(os.environ,ESP_DIR=str(esp)),stdout=stream,stderr=subprocess.STDOUT)
        try:
            time.sleep(.5)
            completed=subprocess.run([os.environ.get('GDB','gdb'),'-q','-batch','-x',str(script)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
            glog.write_text(completed.stdout)
            assert completed.returncode==0 and 'SAFE_HALT_IF0' in completed.stdout,completed.stdout
            time.sleep(.2)
            serial=log.read_text(errors='replace')
            if vector is not None and name not in ('no-uart','no-channels'):
                assert f'vector=0x{vector:016x}' in serial,serial
                assert 'PANIC.COMPLETE' in serial,serial
            if name in ('no-uart','no-channels'): assert 'PANIC.COMPLETE' not in serial
            if name.startswith('fallback'):assert 'UNHANDLED VECTOR (SAFE FALLBACK)' in serial
            results.append({'case':name,'status':'PASS','vector':vector,'method':('QEMU hardware NMI requests, second request subject to architectural NMI blocking' if name=='nmi-repeat-request' else 'QEMU hardware NMI injection' if name=='nmi' else 'synthetic guard state plus QEMU NMI request' if name=='nmi-repeat-guard' else 'CPU execution with debugger-directed code/state injection'),'safe_halt':'IF=0, next instruction HLT; no IRET'})
            print('PASS',name,flush=True)
        finally:
            proc.terminate()
            try:proc.wait(timeout=5)
            except subprocess.TimeoutExpired:proc.kill();proc.wait()
(out/'report.json').write_text(json.dumps(results,indent=2)+'\n')
