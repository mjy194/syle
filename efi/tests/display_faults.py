#!/usr/bin/env python3
"""Real CPU exceptions after EBS; capture framebuffer through QMP, no GUI required."""
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import time

root = Path(__file__).resolve().parents[1]
os.chdir(root)

def run_case(mode, vector=None, no_uart=False):
    name = mode + ('-no-uart' if no_uart else '')
    log_path = root / f'build/{name}.log'
    qmp_port = 14445
    env = dict(os.environ, QMP_PORT=str(qmp_port))
    if no_uart:
        env['NO_UART'] = '1'
    with log_path.open('w') as logfile:
        process = subprocess.Popen(['scripts/run.sh', mode], env=env, stdout=logfile, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 60
            if no_uart:
                time.sleep(8)
            else:
                marker = 'PANIC.COMPLETE' if vector is not None else 'IDT+IST READY'
                while time.monotonic() < deadline:
                    text = log_path.read_text(errors='replace')
                    if marker in text: break
                    if process.poll() is not None: raise RuntimeError(text)
                    time.sleep(.1)
                else: raise TimeoutError(text)
                if vector is not None:
                    assert f'vector=0x{vector:016x}' in text, text
                    assert 'REAL CPU EXCEPTION' in text
                    assert 'STAGE0.POST_EBS' in text
                    for field in ('error','rip','rsp','rflags','rax','r15','cr2'):
                        assert re.search(rf'{field}=0x[0-9a-f]{{16}}',text), field
                    if vector == 14:
                        assert 'cr2=0xffff800000000000' in text
                        assert 'error=0x0000000000000002' in text # supervisor write, not present
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect(('127.0.0.1', qmp_port))
                f = s.makefile('rwb', buffering=0)
                json.loads(f.readline())
                def command(name, **args):
                    f.write((json.dumps(dict(execute=name, arguments=args))+'\n').encode())
                    while True:
                        result = json.loads(f.readline())
                        if 'error' in result: raise RuntimeError(result)
                        if 'return' in result: return result['return']
                command('qmp_capabilities')
                command('screendump', filename=str(root / f'build/{name}.ppm'))
            assert process.poll() is None, 'Panic should remain halted, not restart or terminate'
            print(f'PASS {name}: halted guest retained framebuffer; screenshot build/{name}.ppm')
        finally:
            process.terminate()
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()

run_case('run')
run_case('ud', 6)
run_case('pf', 14)
run_case('ud', 6, no_uart=True)
