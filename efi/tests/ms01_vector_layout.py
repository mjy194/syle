#!/usr/bin/env python3
"""Static entry/C frame ABI checks, not CPU delivery or saved-RIP validation."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
GPRS = 'rax rbx rcx rdx rbp rsi rdi r8 r9 r10 r11 r12 r13 r14 r15'.split()
FIELDS = list(reversed(GPRS)) + 'vector error rip cs rflags rsp ss'.split()
ERRORS = {8, 10, 11, 12, 13, 14, 17, 21, 29, 30}


def check_layout(source, frame_sources):
    for vector in range(256):
        body = source.split(f'exception_{vector}:\n', 1)[1].split('    jmp exception_common', 1)[0]
        pushes = re.findall(r'pushq \$(\d+)', body)
        assert pushes == ([str(vector)] if vector in ERRORS else ['0', str(vector)]), (vector, pushes)
        assert body.index('exception_entered') < body.index('pushq')
        if vector == 2:
            assert body.index('nmi_entered') < body.index('pushq')
            assert body.index('movq %rax, tss+52') < body.index('pushq')
    assert len(re.findall(r'^    \.quad exception_\d+$', source, re.M)) == 256
    entry = source.split('exception_common:\n', 1)[1]
    call = re.search(r'^\s*call\s+exception_panic\s*$', entry, re.M)
    assert call, 'required exception_panic call'
    common = entry[:call.start()]
    assert re.findall(r'pushq %(\w+)', common) == GPRS, 'saved GPR order'
    # Check the full prologue, not just relative positions: an extra RSP/RDI
    # adjustment would otherwise silently shift the frame delivered to C.
    instructions = [re.sub(r'\s+', '', line.split('#', 1)[0])
                    for line in common.splitlines()]
    instructions = [line for line in instructions if line]
    expected = ['cld'] + ['pushq%' + reg for reg in GPRS]
    expected += ['movq%rsp,%rdi', 'andq$-16,%rsp']
    assert instructions == expected, 'common entry frame delivery'
    # Both consumers must agree with the push order and the CPU frame. Each
    # field is a 64-bit scalar: RIP is slot 17 (136 bytes), total 176 bytes.
    for name, c_source in frame_sources.items():
        body = re.search(r'struct frame\s*\{([^}]*)\};', c_source, re.S)
        assert body, name
        fields = []
        for declaration in body.group(1).split(';'):
            if not declaration.strip():
                continue
            match = re.fullmatch(r'\s*(?:UINT64|uint64_t)\s+([\w,\s]+)', declaration)
            assert match, (name, declaration)
            fields.extend(field.strip() for field in match.group(1).split(','))
        assert fields == FIELDS, (name, fields)
    stop = source.split('exception_stop:\n', 1)[1].split('.data', 1)[0]
    assert 'cli' in stop and 'hlt' in stop and 'jmp exception_stop' in stop and 'iret' not in stop


def production_sources():
    return ((ROOT / 'efi/src/exceptions.S').read_text(),
            {name: (ROOT / name).read_text()
             for name in ('efi/src/exceptions.c', 'krnl/ring3.c')})


if __name__ == '__main__':
    check_layout(*production_sources())
    print('PASS 256 normalized entries, pre-push guards, GPR/C frame order, RIP offset 136, frame size 176 and terminal halt; static only')
