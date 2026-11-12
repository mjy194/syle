#!/usr/bin/env python3
"""Actual preprocessed N43 wiring, not a VMX execution claim."""
from pathlib import Path
import os,re,subprocess
ROOT=Path(__file__).resolve().parents[2]
K=ROOT/'krnl'
def pp(name):
 inc=os.environ.get('EFI_INC',os.environ.get('EFI_PREFIX','/usr')+'/include/efi')
 flags=['-DVMX_EPT_TEST','-DVMX_NESTED_TEST','-DVMX_NESTED_VCPU_TEST','-DVMX_NESTED_PARALLEL_TEST',
  '-DVMX_NESTED_DOMAIN_TEST','-DVMX_NESTED_OWNER_SPARSE_TEST','-DVMX_NESTED_OWNER_CONTEXT_TEST',
  '-DVMX_NESTED_ATOMIC_CATALOG_TEST','-DVMX_NESTED_ATOMIC_RETRACK_TEST','-DVMX_NESTED_ATOMIC_MULTIROUND_TEST','-DSMP_NESTED_TEST','-DSMP_VMX_TEST','-DSYLE_KERNEL']
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
order(G,'retrack_ref->local_launched01=&launched01','retrack_ref->local_launched02=&launched02','nested_owner_sparse_start(')
order(G,'nested_domain_dispatch_physical_leave(', 'nested_vcpu_complete(', 'atomic_checkpoint(vcpu->parallel,l2_exits)', 'nvmx_return_from_l2(&model)')
for i in range(13):assert 'nested_atomic_multiround_call'+str(i) in G
assert 'nested_atomic_retrack_call' not in G and 'l2_exits>=13' in G and 'l2_exits!=13' in G
assert 'config_gpa+0x820)!=13' in G and 'flags!=2' in G
fault=G[G.index('if(in_l2&&reason==48)'):G.index('internal_resume=1;continue;')]
assert 'rip+' not in fault and 'put(0x681e' not in fault and 'nested_owner_atomic_fault(' in fault
order(G,'done:','N42B FAILURE RETAIN BEFORE CLEANUP','if(vmcs02&&owned02&&!clear(vmcs02))','nested_owner_sparse_stop(')
S=pp('smp.c');run=function(S,'smp_nested_run');checkpoint=function(S,'atomic_checkpoint')
assert 'SMP.N43_PASS' in run and 'SMP.N42B_PASS' not in run and 'nested_domain_memory_release(' not in run
configure=function(S,'atomic_configure')
assert 'a->empty_ready=namr_empty_ready;a->empty_context=&multiround' in configure
order(checkpoint,'__atomic_store_n(&atomic_runtime.arrived[phase][cpu]', 'namr_round(&multiround,cpu,phase==2?1:2)',
 'atomic_wait(&atomic_runtime.arrived[phase][1])','source_before[0]','namr_round(&multiround,0,round+1)',
 'source_after[0]','atomic_runtime.rounds[round]', '&multiround.completed[0]', '&multiround.completed[1]',
 'retrack.refs[c].phase!=NOAM_COMMITTED','__atomic_store_n(&atomic_runtime.released[phase],1')
assert 'phase==2||phase==7' in checkpoint and 'phase==4||phase==9' in checkpoint
assert 'nested_owner_atomic_item_leaf(' in checkpoint and '(target&0x302)!=0x100' in checkpoint
for value in ('receipt->hpa!=root->hpa','receipt->history!=root->history','noam_ticket_equal(receipt->ticket,retrack.ticket)',
 'multiround_identity(receipt->identity,root->identity)','root->old_generation!=old->new_generation',
 'multiround_source(root->source,old->source)','atomic_runtime.memory.catalog.references!=4'):
 assert value in checkpoint,value
final=function(S,'atomic_final')
for value in ('atomic_runtime.reset_receipts!=8','atomic_runtime.stamp.epoch!=3','NOAM_COMMITTED',
 'atomic_runtime.trace_used[cpu]++','N43_TRACE_TOTAL=','atomic_runtime.receipts==5'):
 assert value in final,value
N=pp('nested_atomic_multiround.c');round_=function(N,'namr_round');complete=function(N,'complete')
order(round_,'wait_value(m,&m->arrived[1],g)','tam_close(','tam_drain(','NART_REVOKE','tam_clear(',
 'NART_RESET','NART_PREPARE','tam_finish(','*r->outer_stamp=r->maintenance.next_stamp','release(&m->released,g)')
order(complete,'noam_commit(','release(&m->completed[c],g)','wait_value(m,&m->completed[c^1],g)')
P=pp('ept_payload.S')
for i in range(13):assert 'nested_atomic_multiround_call'+str(i)+':' in P
for value in ('.byte 0,1,0,0,1,0,1,1,0,1,0,1,1','.byte 0,1,2,2,1,1,2,1,2,1,1,2,1',
 'cmp $13,%rdx','cmp $12,%r10','imul $13,%rcx,%rdx','mov %rax,0xf000(%rdx)',
 '0x42b2000000000000','0x240','cmp %rax,0xf140','cmp %rax,0xf148','cmp %rax,0xf188','cmp %rax,0xf190'):
 assert value in P,value
assert 'nested_atomic_retrack_call' not in P
# N42B remains separately selected and has its original bounded payload.
original=(K/'nested_atomic_retrack_runtime.inc').read_text()
assert 'nart_round(&retrack,0)' in original and 'arrived[8][2],released[8]' in original
print('PASS N43 preprocessed 13 checkpoints, two rounds, both-owner committed gate, exact receipts, retained cohorts and unchanged-RIP retry; structural only')
