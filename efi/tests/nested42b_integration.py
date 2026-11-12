#!/usr/bin/env python3
"""Actual preprocessed N42B wiring, not a VMX execution claim."""
from pathlib import Path
import os,re,subprocess
ROOT=Path(__file__).resolve().parents[2]
K=ROOT/'krnl'
def pp(name):
 inc=os.environ.get('EFI_INC',os.environ.get('EFI_PREFIX','/usr')+'/include/efi')
 flags=['-DVMX_EPT_TEST','-DVMX_NESTED_TEST','-DVMX_NESTED_VCPU_TEST','-DVMX_NESTED_PARALLEL_TEST',
  '-DVMX_NESTED_DOMAIN_TEST','-DVMX_NESTED_OWNER_SPARSE_TEST','-DVMX_NESTED_OWNER_CONTEXT_TEST',
  '-DVMX_NESTED_ATOMIC_CATALOG_TEST','-DVMX_NESTED_ATOMIC_RETRACK_TEST','-DSMP_NESTED_TEST','-DSMP_VMX_TEST','-DSYLE_KERNEL']
 return subprocess.check_output(['cc','-E','-P','-I'+inc,'-I'+inc+'/x86_64',*flags,str(K/name)],text=True)
def function(text,name):
 m=re.search(r'\b'+name+r'\([^;{}]*\)\s*\{',text);assert m,name
 start=m.end()-1;level=1;i=start+1
 while level:
  level+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
def order(text,*parts):
 pos=0
 for part in parts:
  nxt=text.find(part,pos);assert nxt>=0,part;pos=nxt+len(part)
G=function(pp('nested_guest.c'),'nested_guest_run_context')
order(G,'retrack_ref->local_launched01=&launched01','retrack_ref->local_launched02=&launched02','nested_owner_sparse_start(',
 'nested_ept_contexts_active(&vcpu->parallel->sparse.contexts)->eptp')
order(G,'for(unsigned step=0;step<max_steps;step++)','vmptrst','current!=(in_l2?vmcs02:vmcs01)',
 'get(0x201a,&actual_eptp)','nested_owner_atomic_entry(', 'nested_vcpu_authorize(',
 'nested_domain_dispatch_physical_resume(', 'nested_domain_dispatch_physical_enter(', 'nested_vcpu_enter(')
order(G,'nested_domain_dispatch_physical_leave(', 'nested_vcpu_complete(', 'atomic_checkpoint(vcpu->parallel,l2_exits)',
 'nvmx_return_from_l2(&model)')
fault=G[G.index('if(in_l2&&reason==48)'):G.index('internal_resume=1;continue;')]
assert 'rip+' not in fault and 'put(0x681e' not in fault
assert 'nested_owner_atomic_fault(' in fault
order(G,'done:', 'if(!success)nested_parallel_fault_stop', 'N42B FAILURE RETAIN BEFORE CLEANUP',
 'if(vmcs02&&owned02&&!clear(vmcs02))','nested_vcpu_clear(vcpu,context_cpu(),vmcs02)', 'nested_owner_sparse_stop(')
assert G.count('nested_vcpu_enter(')==1 and G.count('nested_vcpu_authorize(')==1
for i in range(8):assert 'nested_atomic_retrack_call'+str(i) in G
assert 'nested_atomic_catalog_call' not in G and 'flags!=2' in G and 'l2_exits>=8' in G
A=pp('nested_owner_atomic_maintenance.c');N=pp('nested_atomic_retrack.c')
remove=function(A,'noam_remove');restore=function(A,'noam_restore');flush=function(A,'noam_flush')
order(remove,'actual(r,r->private_eptp','r->ops.load(r->context,r->vmcs01)', 'r->ops.read_eptp(',
 'eptp!=r->parking_eptp','r->ops.load(r->context,r->vmcs02)', 'r->ops.write_eptp(r->context,r->parking_eptp)',
 'actual(r,r->parking_eptp,NOAM_VERIFY_PARK)', 's->hardware_item=0')
order(restore,'root(r,p,1,1)', 'r->pending_item=p->item','r->ops.write_eptp(r->context,p->full_eptp)',
 'actual(r,p->full_eptp,NOAM_VERIFY_RESTORE)','s->hardware_item=p->item')
order(flush,'root(r,p,0,0)','r->ops.invept(r->context,p->full_eptp)', 'noam_stopped(r,t)', '*out=')
for text in (remove,restore,flush):
 assert 'nested_vcpu_clear' not in text and 'vmclear' not in text.lower()
round_=function(N,'nart_round');opened=function(N,'open_entry')
order(round_,'wait_value(r,&r->arrived[1],1)', 'tam_close(', 'tam_drain(', 'NART_REVOKE','tam_clear(', 'NART_RESET',
 'NART_PREPARE','tam_finish(', '*r->outer_stamp=r->maintenance.next_stamp', 'release(&r->released,1)')
assert 'release(&r->released' not in opened and '*r->outer_stamp=' not in opened
assert 'noam_stopped' not in function(N,'stopped')
S=pp('smp.c');run=function(S,'smp_nested_run');checkpoint=function(S,'atomic_checkpoint')
assert 'SMP.N42B_PASS' in run and 'SMP.N41C_PASS' not in run and 'no_clear_cycle=1' not in run
assert 'nested_domain_memory_release(' not in run
order(checkpoint,'__atomic_store_n(&atomic_runtime.arrived[phase][cpu]', 'nart_round(&retrack,cpu)',
 'atomic_wait(&atomic_runtime.arrived[phase][1])','source_before[0]','nart_round(&retrack,0)',
 'source_after[0]', '__atomic_store_n(&atomic_runtime.released[phase],1')
assert 'nested_owner_atomic_item_leaf(' in checkpoint and '(target&0x302)!=0x100' in checkpoint
for primitive,insn in [('retrack_current','vmptrst'),('retrack_load','vmptrld'),('retrack_read','vmread'),('retrack_write','vmwrite'),('retrack_invept','invept')]:
 assert insn in function(S,primitive)
P=pp('ept_payload.S')
for i in range(8):assert 'nested_atomic_retrack_call'+str(i)+':' in P
assert '.byte 0,1,0,0,1,0,1,1' in P and '.byte 0,1,2,2,1,1,2,1' in P
assert 'mov %rax,0xf000(%rdx)' in P and '0x42b1000000000000' in P and '0x140' in P
assert 'nested_atomic_catalog_call' not in P
print('PASS N42B preprocessed physical checkpoint/VMCS parking/INVEPT/launch retention/release/payload wiring; structural only')
F=function(pp('nested_parallel.c'),'nested_parallel_mark_fault')
order(F,'r->retrack_fault(r)', 'r->fault_code=code', '__atomic_store_n(&r->faulted,1')
assert 'nested_parallel_faulted(&nested_runs[0])' in function(S,'atomic_failed')
assert 'nested_parallel_faulted(&nested_runs[1])' in function(S,'atomic_failed')
order(function(S,'atomic_fail'),'__atomic_store_n(&atomic_runtime.fault,1', 'nested_domain_dispatch_markfail(')
print('PASS N42B all local fault sinks publish shared failure before parking; structural only')
