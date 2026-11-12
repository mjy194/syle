#!/usr/bin/env python3
"""Compile unchanged production gating functions and reject serial-turn/gate mutants."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / 'krnl/nested_typed_dynamic_maintenance.inc').read_text()
def function(name):
    # Balanced braces preserve complete production bodies; declarations unique.
    start = source.index('static ' + name)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'
entry = function('bool typed_context_before_entry(')
parts = function('struct tr_overlap_counts typed_overlap_counts(') + function('bool typed_overlap_entry(')
checkpoint = function('bool typed_checkpoint(')
# Restore the exact serial B1 wait loop in the B2 entry path. The same good
# test must reject it, rather than count a timeout as successful overlap.
old = '    if(!typed_overlap_entry(r)){typed_fatal(r);return false;}\n'
assert entry.count(old) == 1
serial = entry.replace(old, '''    uint64_t start=typed_clock();
    while(__atomic_load_n(&typed_runtime.turn,__ATOMIC_ACQUIRE)!=cpu){
        if(typed_failed()||typed_clock()-start>UINT64_C(30000000000)){typed_fatal(r);return false;}
    }
    if(!typed_overlap_entry(r)){typed_fatal(r);return false;}
''')
gate = '        if(phase==3&&!typed_overlap_wait(&typed_runtime.teardown_turn))goto fail;'
assert checkpoint.count(gate) == 1
recipe = '.PHONY: overlap-flags\noverlap-flags:\n\t@printf "%s\\n" "$(N39B2FLAGS)"\n'
defines = subprocess.check_output(['make','--no-print-directory','-s','-f','Makefile','-f','-','overlap-flags'],input=recipe,text=True,cwd=ROOT/'efi')
with tempfile.TemporaryDirectory(prefix='syle-overlap-gates-') as temp:
    out = Path(temp)
    for name, body in [('production', parts+entry+checkpoint), ('serial_turn', parts+serial+checkpoint), ('late_teardown', parts+entry+checkpoint.replace(gate,''))]:
        (out/'typed-overlap-runtime-gates.inc').write_text(body)
        binary = out/name
        subprocess.run([*shlex.split(os.environ.get('CC','cc')),'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',*shlex.split(defines),'-I',str(out),str(ROOT/'krnl/tests/typed_overlap_runtime_gates.c'),'-o',str(binary),str(ROOT/'krnl/typed_root_overlap.c')],check=True)
        result = subprocess.run([str(binary)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
        if name=='production':
            assert result.returncode==0, result.stdout+result.stderr
            print(result.stdout.strip())
        else:
            assert result.returncode!=0 and 'Assertion' in result.stderr, result.stdout+result.stderr
            print('PASS rejected production runtime mutant: '+name)
