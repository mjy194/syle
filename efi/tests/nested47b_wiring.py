#!/usr/bin/env python3
"""Preprocessed N47B integration contracts, not a hardware execution test."""
from pathlib import Path
import hashlib,json,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
out=Path(tempfile.mkdtemp(prefix='syle-n47b-wiring-'))
print('N47B_WIRING_EVIDENCE',out,flush=True)
r=subprocess.run(['make','-s','--eval=print-n47b-flags:;@echo $(KFLAGS) $(N47BFLAGS)','print-n47b-flags'],cwd=root/'efi',capture_output=True,text=True)
assert r.returncode==0,r.stderr
flags=[f for f in r.stdout.strip().split() if f not in ('-MMD','-MP')];assert '-DVMX_NESTED_SOURCE_REPLACE_TEST' in flags
assert '-DVMX_NESTED_ATOMIC_RETRACK_TEST' not in flags and '-DVMX_NESTED_ATOMIC_MULTIROUND_TEST' not in flags
sources=['nested_guest.c','smp.c']
for name in sources:
    cmd=['cc','-E','-P',*flags,'-DSMP_NESTED_TEST','-DSMP_VMX_TEST',str(root/'krnl'/name)]
    r=subprocess.run(cmd,cwd=root/'efi',capture_output=True,text=True)
    (out/(name+'.i')).write_text(r.stdout);(out/(name+'.log')).write_text(r.stderr)
    assert r.returncode==0,r.stderr
s=(out/'nested_guest.c.i').read_text()
start=s.index('_Bool nested_guest_run_context(')
s=s[start:]
loop=s.index('for(unsigned step=0;step<max_steps;step++)')
assert s.index('nsrr_before_entry(',loop)<s.index('nested_owner_atomic_entry(',loop)<s.index('nested_domain_dispatch_physical_enter(',loop)
leave=s.index('nested_domain_dispatch_physical_leave(',loop)
checkpoint=s.index('!vcpu->parallel->atomic_checkpoint(vcpu->parallel,l2_exits)',leave)
for required in ['!nvmx_exit_store(&model,0x681e,rip)','!nvmx_return_from_l2(&model)||!load(vmcs01)||!reflect_host(&profile)','in_l2=0;','if(model.running||']:
    assert leave<s.rindex(required,leave,checkpoint)<checkpoint,required
assert 'l2_exits>=8||rip!=label' in s and 'nested_source_replace_call7' in s
assert 'l2_exits!=13' not in s and 'l2_exits>=13' not in s
failure=s.index('if(!success)nested_parallel_fault_stop',checkpoint)
assert failure<s.index('clear(vmcs02)',failure)
s=(out/'smp.c.i').read_text();start=s.index('static void smp_nested_run(void)');s=s[start:]
end=s.index('\n}',s.index('SMP.N47B_PASS'))
run=s[:end]
assert 'SMP.N47B_PASS owners=2 roots=3 WB4K_IPAT1_AD1=1' in run
assert 'DOMAIN SHARED READ FAILED RETAIN' not in run, 'N47 terminal must not inherit the old sentinel predicate'
assert 'SMP.DOMAIN_SHARED_READ=' in run
assert 'pmm_free_page(' not in run and 'nested_owner_pool_release_all(' not in run
assert 'nested_domain_memory_release(' not in run
runtime=(root/'krnl/nested_source_replace_runtime.inc').read_text()
assert 'tsr_consumer_stopped(&a->atomic)' in runtime
assert 'cache_page_audit_wb(root,h,pat,8192' in runtime
assert runtime.index('if(phase==2&&!source_host_audit(cpu))')<runtime.index('__atomic_store_n(&atomic_runtime.arrived[phase][cpu]')
assert 'source_leaf(false)&~UINT64_C(0x300))!=0x1d073' in runtime
assert 'nested_source_replace_fixture_raw_unchanged' in runtime
rounds=(root/'krnl/nested_source_replace_round.c').read_text()
assert 'r->metadata_lock(r->metadata_context);struct tsr_ticket t=r->replacement.ticket;' in rounds
assert 'nosr_root_equal(r->validated[c].root,r->replacement.fresh[c])' in rounds
assert 'tsr_abort(&r->replacement,r->ticket);\n   if(!tsr_released' in rounds
(out/'results.json').write_text(json.dumps({'status':'PASS','flags':flags,'checks':['return-before-checkpoint','outer-gate-first','independent-eight-phase','failure-before-clear','all-backing-retained','exact-provider-route','both-owner-full-host-audit','full-validation-receipts','timeout-serialized-release'],'hardware_claim':False},indent=2)+'\n')
print('PASS N47B preprocessed wiring contracts; no hardware execution')
