#!/usr/bin/env python3
"""Host-only assembled entry and compiled frame ABI checks; never executes guest code."""
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIELDS = 'r15 r14 r13 r12 r11 r10 r9 r8 rdi rsi rbp rdx rcx rbx rax vector error rip cs rflags rsp ss'.split()

errors={8,10,11,12,13,14,17,21,29,30}
def instructions(block):
 return [re.sub(r'\s+', ' ', m.group(1)).split(' #', 1)[0].strip()
         for m in re.finditer(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} )+\s*(\S[^\n]*)$', block, re.M)]


def check_blocks(blocks,rel):
 for vector in range(256):
  b=blocks[f'exception_{vector}']
  seq = instructions(b)
  expected_count = 1 if vector in errors else 2
  assert len([i for i in seq if i.startswith('push ')]) == expected_count, (vector, 'extra push')
  assert all(i.split()[0] in ('cmpb', 'jne', 'movb', 'mov', 'lea', 'push', 'jmp') for i in seq), (vector, 'unexpected instruction')
  assert not re.search(r'%(?:rsp|esp|sp|spl)\b', b), (vector, 'stack modification')
  assert all(i.startswith('push $') for i in seq[-expected_count-1:-1]), vector
  assert re.fullmatch(r'jmp [0-9a-f]+ <exception_common>', seq[-1]), vector
  pushes=[]
  for m in re.finditer(r'^\s*[0-9a-f]+:\s+((?:[0-9a-f]{2} )+)\s*push\s+\$0x([0-9a-f]+)',b,re.M):
   code=bytes.fromhex(m.group(1)); rendered=int(m.group(2),16)
   assert code[0] in (0x6a,0x68),(vector,code)
   value=int.from_bytes(code[1:],'little',signed=True)
   assert value==rendered,(vector,'sign extension',value,rendered)
   pushes.append(value)
  assert pushes==([vector] if vector in errors else [0,vector]),(vector,pushes)
  assert re.search(r'jmp\s+[0-9a-f]+ <exception_common>',b),vector
  assert b.index('exception_entered')<b.index('push'),vector
 table=re.findall(r'^([0-9a-f]+)\s+R_X86_64_64\s+exception_(\d+)\s*$',rel,re.M)
 assert [(int(off,16),int(v)) for off,v in table]==[(i*8,i) for i in range(256)]
 common=blocks['exception_common']
 seq=[]
 for line in common.splitlines():
  m=re.match(r'\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} )+\s*(.*?)\s*$',line)
  if m:seq.append(re.sub(r'\s+',' ',m.group(1)))
 assert seq[:16]==['cld']+['push %'+r for r in 'rax rbx rcx rdx rbp rsi rdi r8 r9 r10 r11 r12 r13 r14 r15'.split()],seq
 assert seq[16]=='mov %rsp,%rdi' and seq[17]=='and $0xfffffffffffffff0,%rsp',seq
 assert 'R_X86_64_PLT32\texception_panic-0x4' in common
 assert len(seq) == 22 and re.fullmatch(r'call [0-9a-f]+ <exception_common\+0x[0-9a-f]+>', seq[18]),seq
 assert seq[19:21]==['cli','hlt'],seq
 assert re.fullmatch(r'jmp [0-9a-f]+ <exception_common\+0x[0-9a-f]+>', seq[21]),seq
 stop=blocks['exception_stop']
 assert 'fa ' in stop and 'f4 ' in stop and re.search(r'jmp\s+[0-9a-f]+ <exception_stop>',stop)

def inspect(source, directory, stem):
    assembly = directory / (stem + '.S')
    obj = directory / (stem + '.o')
    assembly.write_text(source)
    subprocess.run(shlex.split(os.environ.get('AS', 'as')) + ['--64', str(assembly), '-o', str(obj)], check=True)
    tool = shlex.split(os.environ.get('OBJDUMP', 'objdump'))
    dis = subprocess.check_output(tool + ['-dr', str(obj)], text=True)
    rel = subprocess.check_output(tool + ['-r', str(obj)], text=True)
    blocks = {m.group(1): m.group(2) for m in re.finditer(
        r'^[0-9a-f]+ <([^>]+)>:\n(.*?)(?=^[0-9a-f]+ <|\Z)', dis, re.M | re.S)}
    return blocks, rel, hashlib.sha256(obj.read_bytes()).hexdigest()


def compile_frame(source, directory, stem):
    declarations = re.findall(r'struct frame\s*\{[^}]*\};', source, re.S)
    if len(declarations) != 1:
        raise AssertionError('expected exactly one production frame declaration')
    code = '#include <stdint.h>\n#include <stddef.h>\ntypedef uint64_t UINT64;\n' + declarations[0] + '\n'
    for slot, field in enumerate(FIELDS):
        code += f'_Static_assert(offsetof(struct frame, {field}) == {slot * 8}, "{field}");\n'
    code += '_Static_assert(sizeof(struct frame) == 176, "frame size");\n'
    c = directory / (stem + '.c')
    c.write_text(code)
    return subprocess.run(shlex.split(os.environ.get('CC', 'cc')) +
                          ['-m64', '-std=c11', '-Wall', '-Wextra', '-Werror', '-c', str(c),
                           '-o', str(directory / (stem + '.o'))], capture_output=True, text=True)


def main():
    if not __debug__:
        raise SystemExit('Assertions required: do not use python -O or PYTHONOPTIMIZE')
    source = (ROOT / 'efi/src/exceptions.S').read_text()
    hashes = {'efi/src/exceptions.S': hashlib.sha256(source.encode()).hexdigest()}
    with tempfile.TemporaryDirectory(prefix='syle-entry-contract-') as temp:
        directory = Path(temp)
        blocks, rel, object_hash = inspect(source, directory, 'production')
        check_blocks(blocks, rel)
        # Assemble actual malformed fixtures, rather than editing objdump text.
        mutants = {
            '128_sign_extension': source.replace('pushq $128', 'pushq $-128', 1),
            'gp_vector': source.replace('pushq $13\n', 'pushq $14\n', 1),
            'extra_stack_push': source.replace('exception_13:\n', 'exception_13:\n    pushq %rax\n', 1),
            'extra_stack_adjustment': source.replace('exception_13:\n', 'exception_13:\n    subq $8,%rsp\n', 1),
            'table_slot': source.replace('.quad exception_255', '.quad exception_254', 1),
        }
        for name, mutant in mutants.items():
            if mutant == source:
                raise AssertionError('mutation did not apply: ' + name)
            bad, bad_rel, _ = inspect(mutant, directory, name)
            try:
                check_blocks(bad, bad_rel)
            except AssertionError:
                pass
            else:
                raise AssertionError('mutation escaped: ' + name)
        for i, name in enumerate(('efi/src/exceptions.c', 'krnl/ring3.c')):
            frame = (ROOT / name).read_text()
            hashes[name] = hashlib.sha256(frame.encode()).hexdigest()
            result = compile_frame(frame, directory, f'frame-{i}')
            if result.returncode:
                raise AssertionError(name + ': ' + result.stderr)
            mutant = frame.replace('vector,error,rip,cs', 'vector,rip,error,cs', 1)
            if mutant == frame or compile_frame(mutant, directory, f'bad-frame-{i}').returncode == 0:
                raise AssertionError('frame mutation escaped: ' + name)
    print(json.dumps({'status': 'PASS', 'layer': 'assembled object and compiled C layout only',
                      'vectorCount': 256, 'negativeObjectChecks': list(mutants),
                      'frameOffsetAssertions': 44, 'frameSizeAssertions': 2,
                      'negativeFrameChecks': 2, 'sourceSha256': hashes,
                      'objectSha256': object_hash, 'noGuestExecution': True}))


if __name__ == '__main__':
    main()
