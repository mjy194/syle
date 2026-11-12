#!/usr/bin/env python3
"""Sequential reproducible MS-01 suite. No installation, hardware reboot or publish.
Provide EFI_PREFIX/QEMU/GDB/OVMF_* from your toolchain environment. Host mocks,
TCG CPU execution and later-phase regression are intentionally distinct.
"""
import argparse, datetime, hashlib, json, os, pathlib, subprocess, sys, time, signal
from runtime_fingerprint import collect as collect_runtime_fingerprint
ROOT=pathlib.Path(__file__).resolve().parents[1]
TESTS=[
 ('build','build',['make','-C','efi','-j4','all','smoke','faults','ring3']),
 ('provenance','host',['python3','ms01/tests/runtime_fingerprint.py']),
 ('boot-policy','host',['sh','boot/tests/policy.sh']),
 ('boot-elf','host',['sh','boot/tests/kernel_elf.sh',str(ROOT/'efi/build/krnl.elf')]),
 ('boot-exit','host',['sh','boot/tests/exit_services.sh']),
 ('boot-read','host',['sh','boot/tests/read_bounded.sh']),
 ('boot-abi','host',['sh','krnl/tests/boot_info.sh']),
 ('pmm','host',['sh','krnl/tests/memory.sh']),
 ('paging-alias','host',['sh','krnl/tests/paging_alias.sh']),
 ('console','host',['bash','efi/tests/console_memory.sh']),
 ('uart','host',['bash','efi/tests/ms01_uart.sh']),
 ('vector-layout','host',['python3','efi/tests/ms01_vector_layout.py']),
 ('smoke','qemu',['bash','efi/tests/smoke.sh']),
 ('boot-chain','qemu',['python3','efi/tests/boot_chain.py']),
 ('memory-faults','qemu',['python3','efi/tests/kernel_memory.py']),
 ('wx-alias','qemu',['python3','efi/tests/wx_alias.py']),
 ('diagnostics','qemu',['python3','efi/tests/ms01_diagnostics.py']),
 ('cpu-boundary','qemu',['python3','efi/tests/ms01_cpu_boundary.py']),
 ('debugger','qemu',['python3','efi/tests/ms01_debugger.py']),
 ('high-memory','qemu',['python3','efi/tests/high_memory.py']),
 ('efi-failure','qemu',['python3','efi/tests/ms01_efi_failure.py']),
 ('paging-boundaries','held',None),
 ('nmi-overlap','qemu',['python3','efi/tests/ms01_nmi_overlap.py']),
 ('debugger-abrupt','qemu',['python3','efi/tests/ms01_debugger_abrupt.py']),
 ('fallback-vectors','qemu',['python3','efi/tests/ms01_fallback_vectors.py']),
 ('nmi-uart-budget','held',None),
 ('ring3-regression','regression',['bash','efi/tests/ring3.sh']),
]
def source_hashes():
    result={}
    for top in ('boot','efi','include','krnl','user','ms01'):
        for p in sorted((ROOT/top).rglob('*')):
            if not p.is_file() or any(x in ('build','evidence','__pycache__') for x in p.parts):continue
            result[str(p.relative_to(ROOT))]=hashlib.sha256(p.read_bytes()).hexdigest()
    return result
p=argparse.ArgumentParser()
p.add_argument('--group',choices=['all','host','qemu'],default='all')
p.add_argument('--only',help='comma-separated exact suite IDs')
p.add_argument('--output',default='ms01/evidence/aggregate')
p.add_argument('--skip',default='',help='comma-separated suites to record without executing')
p.add_argument('--skip-reason',default='Explicitly skipped by operator')
a=p.parse_args()
for selection in (a.only,a.skip):
    if selection:
        unknown=set(selection.split(','))-{t[0] for t in TESTS}
        if unknown:p.error('Unknown suite IDs: '+','.join(sorted(unknown)))
out=ROOT/a.output;out.mkdir(parents=True,exist_ok=True)
before=source_hashes();(out/'source-before.json').write_text(json.dumps(before,indent=2)+'\n')
selected=[t for t in TESTS if (t[0] in a.only.split(',') if a.only else a.group=='all' or t[1] in (a.group,'build'))]
layers=sorted({group for name,group,cmd in selected if group!='held' and name not in a.skip.split(',')})
report={'started':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Selected test layers: '+(', '.join(layers) or 'none')+'; no bare-metal qualification claimed','selectedSuiteIds':[t[0] for t in selected],'plannedLayers':layers,'suites':[]}
fingerprint=collect_runtime_fingerprint()
(out/'runtime-fingerprint.json').write_text(json.dumps(fingerprint,indent=2)+'\n')
report['runtimeFingerprint']=str((out/'runtime-fingerprint.json').relative_to(ROOT))
for name,group,cmd in selected:
    log=out/(name+'.log');start=time.monotonic()
    if group=='held' or name in a.skip.split(','):
        held_reasons={'paging-boundaries':'Paging supplement held: execution platform blocked worker; exact action unknown; final script unverified and not merged.',
                      'nmi-uart-budget':'Supplemental NMI UART runtime test held: private QMP Unix socket creation returned EPERM before guest execution; no alternate route attempted; script not merged.'}
        reason=held_reasons[name] if group=='held' else a.skip_reason
        item={'id':name,'layer':group,'command':cmd,'status':'not_run_blocked','exitCode':None,'seconds':0,'log':None,'reason':reason}
        report['suites'].append(item)
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print('NOT_RUN_BLOCKED',name,flush=True)
        continue
    print('RUN',name,flush=True)
    try:
        with log.open('w') as stream:
            process=subprocess.Popen(cmd,cwd=ROOT,env=dict(os.environ),stdout=stream,stderr=subprocess.STDOUT,start_new_session=True)
            code=process.wait(timeout=1200)
        status='passed' if code==0 else 'failed'
    except subprocess.TimeoutExpired:
        # Terminate only this suite's own process group, including its QEMU/GDB.
        os.killpg(process.pid,signal.SIGTERM)
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid,signal.SIGKILL);process.wait()
        status='timeout';code=None
    item={'id':name,'layer':group,'command':cmd,'status':status,'exitCode':code,'seconds':round(time.monotonic()-start,3),'log':str(log.relative_to(ROOT))}
    report['suites'].append(item)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(status.upper(),name,flush=True)
after=source_hashes();changed=[k for k in sorted(before.keys()|after.keys()) if before.get(k)!=after.get(k)]
report['sourceChangedDuringRun']=changed
report['finished']=datetime.datetime.now(datetime.timezone.utc).isoformat()
report['passed']=sum(s['status']=='passed' for s in report['suites'])
report['blocked']=sum(s['status']=='not_run_blocked' for s in report['suites'])
report['failed']=sum(s['status'] in ('failed','timeout') for s in report['suites'])
report['notSelected']=[t[0] for t in TESTS if t[0] not in {s['id'] for s in report['suites']}]
report['status']='failed-or-source-changed' if report['failed'] or changed else ('partial-blocked' if report['blocked'] else 'passed-selected-suites' if report['notSelected'] else 'passed')
(out/'source-after.json').write_text(json.dumps(after,indent=2)+'\n')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
# Bind scenario associations to THIS invocation, never infer from old results.
mapping=json.loads((ROOT/'ms01/scenario-test-map.json').read_text())
byid={s['id']:s for s in report['suites']}
for scenario in mapping['scenarios']:
    ids=scenario['suiteIds']
    scenario['suiteResults']=[{'id':i,'status':byid[i]['status'] if i in byid else 'not_run_in_this_invocation','log':byid[i]['log'] if i in byid else None} for i in ids]
    scenario['evidenceStatus']='source_changed_during_run' if changed else ('linked_suites_passed' if all(i in byid and byid[i]['status']=='passed' for i in ids) else 'incomplete_or_failed_in_this_invocation')
mapping['executionReport']=str((out/'report.json').relative_to(ROOT))
mapping['warning']='Linked suite results, not an exhaustive completeness verdict. Candidate details remain unchanged.'
(out/'scenario-results.json').write_text(json.dumps(mapping,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ('status','passed','blocked','failed','sourceChangedDuringRun')},indent=2))
sys.exit(0 if report['status'] in ('passed','passed-selected-suites') else 1)
