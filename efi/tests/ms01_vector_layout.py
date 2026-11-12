#!/usr/bin/env python3
"""Static entry ABI checks; not hardware delivery of CP/VC/SX/reserved vectors."""
from pathlib import Path
import re
source=(Path(__file__).resolve().parents[1]/'src/exceptions.S').read_text()
errors={8,10,11,12,13,14,17,21,29,30}
for vector in range(256):
    body=source.split(f'exception_{vector}:\n',1)[1].split('    jmp exception_common',1)[0]
    pushes=re.findall(r'pushq \$(\d+)',body)
    assert pushes==([str(vector)] if vector in errors else ['0',str(vector)]),(vector,pushes)
    assert body.index('exception_entered')<body.index('pushq')
    if vector==2:
        assert body.index('nmi_entered')<body.index('pushq')
        assert body.index('movq %rax, tss+52')<body.index('pushq')
assert len(re.findall(r'^    \.quad exception_\d+$',source,re.M))==256
stop=source.split('exception_stop:\n')[1].split('.data')[0]
assert 'cli' in stop and 'hlt' in stop and 'jmp exception_stop' in stop and 'iret' not in stop
print('PASS 256 normalized entry layouts, architectural error-code set, pre-push guards and terminal halt; static only')
