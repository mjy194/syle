#!/usr/bin/env python3
"""Mutation checks against an actual lifecycle serial capture (argument required)."""
from pathlib import Path
import sys
from analyze_managed_firmware import lifecycle
raw=Path(sys.argv[1]).read_bytes()
proof,_=lifecycle(raw)
assert proof['releases'][0]['pages_after']==proof['releases'][1]['pages_after']
def reject(name,data):
    assert data!=raw,name
    try:lifecycle(data)
    except AssertionError:return
    raise AssertionError('accepted corrupt evidence: '+name)
reject('typed echo only',raw.replace(b'SYLE_LIFE_RESUMED\r\r\n',b'echo SYLE_LIFE_RESUMED\r\r\n'))
reject('changed paused state',raw.replace(b'FMG.RESUME same_guest=1 generation=0x0000000000000001',b'FMG.RESUME same_guest=1 generation=0x0000000000000002'))
reject('page leak',raw.replace(b'pages_after=0x0000000000000106',b'pages_after=0x0000000000000107'))
reject('VMX still active',raw.replace(b'vmx_active=0x0000000000000000',b'vmx_active=0x0000000000000001'))
reject('adapter still exposed',raw.replace(b'adapter_revoked=0x0000000000000001',b'adapter_revoked=0x0000000000000000'))
reject('trace dropped',raw.replace(b'trace_dropped=0x0000000000000000',b'trace_dropped=0x0000000000000001'))
reject('missing recreated guest',raw.replace(b'SYLE_LIFE_RECREATED\r\r\n',b'RECREATED_MISSING\r\r\n'))
reject('missing disk proof',raw.replace(b'SYLE_PERSIST_VERIFIED',b'SYLE_PERSIST_MISSING'))
reject('missing final exit',raw.replace(b'MGMT.EXIT status=0x0000000000000000',b'MGMT.EXIT status=0x0000000000000001'))
reject('missing release',raw.replace(b'FMG.RELEASE generation=0x0000000000000002',b'RELEASE_MISSING generation=0x0000000000000002'))
print('managed firmware analyzer: real capture PASS and ten corrupt-evidence cases rejected')
