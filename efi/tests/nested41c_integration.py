#!/usr/bin/env python3
"""Check actual preprocessed N41C wiring. Structural checks, not VMX evidence."""
from pathlib import Path
import os,re,subprocess
ROOT=Path(__file__).resolve().parents[2]
K=ROOT/'krnl'
def pp(name):
 inc=os.environ.get('EFI_INC',os.environ.get('EFI_PREFIX','/usr')+'/include/efi')
 flags=['-DVMX_EPT_TEST','-DVMX_NESTED_TEST','-DVMX_NESTED_VCPU_TEST','-DVMX_NESTED_PARALLEL_TEST',
  '-DVMX_NESTED_DOMAIN_TEST','-DVMX_NESTED_OWNER_SPARSE_TEST','-DVMX_NESTED_OWNER_CONTEXT_TEST',
  '-DVMX_NESTED_ATOMIC_CATALOG_TEST','-DSMP_NESTED_TEST','-DSMP_VMX_TEST','-DSYLE_KERNEL']
 return subprocess.check_output(['cc','-E','-P','-I'+inc,'-I'+inc+'/x86_64',*flags,str(K/name)],text=True)
def function(text,name):
 m=re.search(r'\b'+name+r'\([^;{}]*\)\s*\{',text);assert m,name
 start=m.end()-1;level=1;i=start+1
 # Braces in strings are not present in these selected production functions.
 while level:
  level+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
def order(text,*parts):
 pos=0
 for part in parts:
  nxt=text.find(part,pos);assert nxt>=0,part;pos=nxt+len(part)
G=function(pp('nested_guest.c'),'nested_guest_run_context')
order(G,'for(unsigned step=0;step<max_steps;step++)','vmptrst','current!=(in_l2?vmcs02:vmcs01)',
 'get(0x201a,&actual_eptp)','nested_owner_atomic_entry(', 'nested_vcpu_authorize(',
 'nested_domain_dispatch_physical_resume(', 'nested_domain_dispatch_physical_enter(', 'nested_vcpu_enter(')
order(G,'nested_domain_dispatch_physical_suspend_ept(', 'nested_vcpu_complete(',
 'if(in_l2&&reason==48)', 'nested_owner_atomic_fault(', 'internal_resume=1;continue;')
fault=G[G.index('if(in_l2&&reason==48)'):G.index('internal_resume=1;continue;')]
assert 'rip+' not in fault and 'put(0x681e' not in fault
order(G,'(vectoring&(1u<<31)))break;', 'if(in_l2&&reason==48)')
order(fault,'get(0x6400,&qualification)','get(0x2400,&gpa)','get(0x4000,&actual_pin_controls)',
 'nested_owner_atomic_fault(&vcpu->parallel->atomic_catalog,gpa,qualification,actual_pin_controls)',
 '__atomic_load_n(&r->valid,', 'N41C_REJECT_REASON=', 'N41C_REJECT_QUALIFICATION=',
 'N41C_REJECT_GPA=', 'N41C_REJECT_PIN_CONTROLS=', 'N41C_REJECT_STAGE=', 'break;', 'N41C_FAULT_QUALIFICATION=')
# Even malformed access/pagewalk fields reach the production failure latch.
before_fault=fault[:fault.index('nested_owner_atomic_fault(')]
assert 'qualification&7' not in before_fault and 'qualification&3' not in before_fault
V=pp('vmx_guest.c')
assert 'vmx_guest_controls(msr(0x48b),2,2,&secondary)' in V

order(G,'nested_owner_sparse_select(owner_cache,key)','nested_ept_contexts_pin_active(',
 'put(0x201a,new_pin->tree.eptp)','nested_ept_contexts_unpin(', 'owner_cache->hardware_item=new_pin')
order(G,'if(vmcs02&&owned02&&!clear(vmcs02))','nested_vcpu_clear(vcpu,context_cpu(),vmcs02)',
 'nested_owner_sparse_stop(', 'load(vmcs01)')
assert G.count('nested_vcpu_enter(')==1 and G.count('nested_vcpu_authorize(')==1
assert 'if(!success)nested_domain_dispatch_markfail' in G
for i in range(8):assert 'nested_atomic_catalog_call'+str(i) in G
assert 'flags!=2' in G and 'l2_exits>=8' in G and 'l2_exits!=8' in G
S=pp('smp.c');run=function(S,'smp_nested_run');ap=function(S,'smp_nested_ap')
order(ap,'vmx_guest_run_context(', 'nested_local_complete(', 'smp_vmx_local_leave(',
 'nested_parallel_finish(', '__atomic_store_n(&m->parked,1')
order(run,'vmx_guest_run_context(', 'nested_local_complete(', 'smp_vmx_local_leave(',
 'nested_parallel_finish(', 'nested_parallel_done(a)', 'ap_vmx.root_live||bsp_vmx.root_live',
 'nested_domain_dispatch_final(', 'atomic_final(')
assert 'nested_domain_memory_release(' not in run
assert 'backing_retained=1 no_clear_cycle=1' in run
prepare=function(S,'smp_nested_prepare')
order(prepare,'nested_domain_memory_prepare(', 'atomic_prepare(', 'atomic_configure(', 'atomic_private_inventory(')
assert 'nested_domain_memory_prepare_contexts(' not in prepare
assert 'typed_configure(' not in prepare and 'domain_ad_gate' not in prepare
P=pp('ept_payload.S')
assert '0x11007' not in P and '0x12007' not in P and '0x13007' not in P
for i in range(8):assert 'nested_atomic_catalog_call'+str(i)+':' in P
assert '0x1005e' in P and 'vmwrite %rax,%rcx' in P and 'vmresume' in P
print('N41C_INTEGRATION_OK preprocessed_entry_fault_switch_clear_off_retention_payload_order; structural_only')
