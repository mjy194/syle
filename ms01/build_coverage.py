#!/usr/bin/env python3
"""Build traceable scenario/test links without modifying confirmed definitions."""
import json
from pathlib import Path
R=Path(__file__).resolve().parents[1]
draft=json.loads((R/'ms01/scenario-draft.json').read_text())
board=json.loads((R/'ms01/board-v22-snapshot.json').read_text())
def sid(i):return f'SC-M1-BOOT-{i:02}' if i<=6 else f'SC-M1-{i:03}'
# Links refer to reproducible commands in run.py, not inferred historical PASS.
groups={
1:['smoke'],2:['smoke','debugger'],3:['boot-exit'],4:['boot-exit'],5:['boot-exit','efi-failure'],6:['boot-abi','cpu-boundary'],
7:['boot-policy','boot-chain'],8:['boot-policy'],9:['boot-policy','boot-chain'],10:['boot-policy','boot-chain'],11:['boot-policy'],12:['boot-policy'],
13:['boot-elf'],14:['boot-read','boot-chain'],15:['boot-elf'],16:['boot-elf'],17:['boot-elf'],18:['boot-elf'],19:['boot-elf','boot-read'],20:['boot-elf'],
21:['smoke','diagnostics','debugger'],22:['cpu-boundary','diagnostics','paging-boundaries'],23:['cpu-boundary','diagnostics','debugger'],24:['pmm'],25:['pmm','high-memory'],26:['pmm','paging-boundaries'],27:['pmm'],28:['pmm'],29:['pmm'],
30:['smoke','memory-faults'],31:['smoke','paging-alias'],32:['memory-faults','wx-alias','diagnostics','paging-boundaries'],33:['memory-faults','diagnostics','paging-boundaries'],34:['memory-faults','diagnostics','paging-boundaries'],35:['paging-alias','wx-alias'],36:['paging-alias','wx-alias'],37:['wx-alias'],38:['console','diagnostics','paging-boundaries'],
39:['high-memory'],40:['high-memory'],41:['pmm','paging-alias'],42:['pmm','paging-alias'],43:['pmm'],44:['smoke','boot-chain'],45:['vector-layout','diagnostics'],46:['diagnostics'],47:['diagnostics','nmi-overlap'],48:['diagnostics','nmi-overlap'],49:['vector-layout','diagnostics','fallback-vectors'],50:['console','diagnostics'],51:['uart','diagnostics'],52:['diagnostics'],53:['console','diagnostics'],54:['uart','diagnostics'],55:['console','diagnostics'],56:['diagnostics','nmi-overlap'],57:['diagnostics','debugger'],58:['debugger'],59:['debugger'],60:['debugger'],61:['debugger'],62:['debugger'],63:['debugger','debugger-abrupt'],64:['debugger','smoke']}
limits={
3:'Mocked firmware callbacks execute actual production transaction, not physical firmware fault injection.',
4:'Three EBS attempts selected as technical policy, not newly user-confirmed.',
5:'Actual EFI adapter failure path is exercised by QEMU firmware-return injection; not a physical firmware failure.',
6:'Loader-owned boot record must itself be readable; metadata validation cannot prove arbitrary pointer accessibility.',
8:'Firmware callback mock for missing keyboard; QEMU no-keyboard hardware matrix not claimed.',
12:'Final poll before 30 seconds wins; at expiry default wins. Technical policy remains draft.',
18:'Actual production allocation/load seam with injected allocator conflict, not physical reservation conflict.',
21:'BSP only. No AP startup, timer or multicore completion claim.',
22:'Current required CPU contract only, not SIMD/XSAVE or VMX support. Supplement paging runner is blocked/unverified; prior component evidence is separate.',
23:'Full diagnostics guarantee starts after exceptions_init installs own IDT; inherited firmware transition window remains explicit.',
26:'Prior component evidence recorded injected allocation failure without partial CR3 publication; final supplement paging runner is blocked/unverified. See SUPPLEMENT_R2.md.',
32:'Final supplement precise frame runner is blocked/unverified; baseline and prior component evidence are separate.',
33:'Final supplement precise frame runner is blocked/unverified; baseline and prior component evidence are separate.',
34:'Noncanonical fetch RIP assertion failed; final paging runner was platform-blocked. No QEMU bug or complete read/write/fetch acceptance is claimed.',
38:'Real PTE/endpoints have prior component evidence; final supplement paging runner is blocked/unverified.',
29:'Implementation metadata limits only, not arbitrary firmware complexity.',
35:'Owned kernel tree and inherited bootstrap user tree. No arbitrary external roots or EPT promise.',
36:'One-way NX writable loading to sealed executable publication; reverse/JIT transition is unsupported and outside the chosen implementation.',
37:'BSP invalidation only, not shootdown across running APs.',
39:'5/8 GiB QEMU TCG access, not bare metal.',
41:'4 GiB exact/cross-boundary inputs are host synthetic tests; QEMU 5/8 GiB runs separately prove real high-frame accesses.',
42:'128 TiB exclusive design/host-test cap; not RAM capacity tested by QEMU.',
43:'Sparse 1/32 TiB synthetic maps; not native hardware RAM capacity.',
45:'Actual DE/BP/UD/GP/PF/DF/NMI representatives plus static all-stub/frame inspection; no claim all 32 exception types individually triggered.',
46:'Actual hardware #DF from failed exception-stack delivery; terminal handling only, not recovery.',
47:'Real QEMU NMI during ordinary exception handling at selected checkpoints; original frame compared byte-for-byte. Not all microtimings.',
48:'Real ordinary-fault/NMI overlap, second NMI requests under architectural blocking, and separate synthetic guard test; no timing-exhaustive NMI proof.',
49:'All 256 gates/stubs checked; every fallback vector 32–255 receives an actual software INT, with explicit debugger rearming between fatal test cases. This is not hardware IRQ/controller coverage or production recovery; first32 exceptions remain representative.',
54:'Production C UART loop has injected stuck-status tests; emergency NMI UART budget is static-checked. An additional runtime status-injection attempt was blocked before guest execution by QMP Unix socket EPERM and is not an added acceptance gate.',
57:'Fatal paths stop with IF=0 and HLT, never IRET; no recoverable-exception or watchdog-restart policy is inferred.',
62:'Exact image hash and live code bytes checked; normal GDB memory error. No universal debugger symbol matching guarantee.',
63:'Explicit detach and actual SIGKILL/TCP-close in stopped/running states tested for this QEMU build; no cross-debugger universal policy is assumed.',
}
records=[]
for i,s in enumerate(draft['scenarios'],1):
    assert s['id']==f'DRAFT-MS1-{i:03}'
    selected=next(v for v in board['scenarioDefinitions'] if v['id']==sid(i))
    records.append({'scenarioId':sid(i),'sourceDraftId':s['id'],'title':selected.get('title',selected.get('name',s['title'])),
      'selectionStatus':selected.get('selectionStatus'),'detailStatus':selected.get('detailStatus'),'requirementIds':s['requirementIds'],
      'automationImplemented':True,'suiteIds':groups[i],
      'evidenceStatus':'execution_results_are_separate','scopeLimit':limits.get(i,'Assertions cover the documented bounded implementation; not exhaustive input/platform coverage.')})
for record in records:
    if record['scenarioId'] in ('SC-M1-054','SC-M1-056'):
        record['additionalTestWork']={'suiteId':'nmi-uart-budget','status':'not_run_blocked','scope':'Optional deeper runtime validation of emergency NMI UART poll budget; not a new confirmed acceptance gate.','reason':'Private QMP Unix socket creation returned EPERM before guest execution; no alternate route attempted.'}
assert len(records)==64 and len({r['scenarioId'] for r in records})==64
result={'boardVersion':22,'boardCommit':'c1218f0bffb513e145ea7b0710cfca0fd42cd652','boardURL':'https://syle-ept-board.azureimpala2.chatgpt.site','scenarioCount':64,'warning':'Automation links do not change candidate confirmation or prove every possible subcase. See component coverage documents and exact final run report.','scenarios':records}
(R/'ms01/scenario-test-map.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print('PASS: 64 exact board scenario IDs linked to current automation; no historical PASS inherited')
