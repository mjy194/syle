"""Generate passive pre-prepare observer; never launches a VM."""
import pathlib,hashlib,sys
p=pathlib.Path(sys.argv[1]);assert hashlib.sha256(p.read_bytes()).hexdigest()=='469fc642790ab9efc52295eec33dacfa2b7cab530dcda1887624de4c22a9b895'
s=p.read_text().replace('run_source_observation.py','run_prepare_observation.py')
a=s.index(" addr=inv['snapshots']['nmi_probe_owner_exit_calls']");b=s.index('def wait_serial(',a);s=s[:a]+s[b:]
s=s.replace("query('cont');wait_serial(b'NMI.PROBE.'+(b'READY_WAIT' if mode==0 else b'RUNNING_WAIT'))", "query('cont');wait_serial("+repr(b'SOURCE.PREPARE_CONTEXT_WINDOW\r\n')+")")
s=s.replace("assert cs&3==3 and rip in (inv['managers'][str(mode)]['wait_rip'],inv['managers'][str(mode)]['wait_rip']+2),(cs,rip)", """assert cs==8 and rip in inv['prepare_window']['instruction_rips'],(cs,rip)
 ss=int(re.search(r'SS\\s*=([0-9a-fA-F]+)',regs)[1],16)
 flags=int(re.search(r'RFL=([0-9a-fA-F]+)',regs)[1],16)
 assert ss==0x10 and not flags&0x600 and 'CPL=0' in regs and 'SMM=0' in regs,(ss,flags)
""")
a=s.index(" checks={'state':");b=s.index(" pathlib.Path('before-decoded.json')",a)
s=s[:a]+''' checks={'state':u(mg,layout['managed_guest.state'],4),'handle':u(mg,layout['managed_guest.handle'],8),'busy':u(mg,layout['managed_guest.busy'],1)}
 # Controller provisionally sets READY before invoking prepare, but it is BUSY
 # and the session has not been prepared. Do not call this a ready VM.
 assert checks=={'state':1,'handle':1,'busy':1},checks
 for key in ['session.prepared','session.enabled','session.host.active','session.memory.prepared','session.state.ready']:
  checks[key]=u(sess,layout[key],1);assert checks[key]==0,(key,checks)
 assert u(sess,layout['session.configured'],1)==1
 for key in ['session.memory.eptp','session.host.vmxon_page','session.host.vmcs_page']:
  checks[key]=u(sess,layout[key],8);assert checks[key]==0,(key,checks)
 for key,count in [('session.memory.pages',inv['prepare_window']['pages_count']),('session.memory.tables',inv['prepare_window']['tables_count'])]:
  offset=layout[key];assert not any(sess[offset:offset+count*8]),key
''' +s[b:]
needle=" bound=int.from_bytes(physical(source_inv['physical_bound_address'],8),'little')"
assert needle in s;s=s.replace(needle," paging_root=int.from_bytes(physical(inv['prepare_window']['paging_root_address'],8),'little')\n assert reg('CR3')==paging_root and paging_root and not paging_root&4095\n"+needle)
s=s.replace("'scope':'current stopped Syle CR3 guest VA->QEMU GPA only; no EPT/other roots/DMA proof'", "'scope':'one actual pre-lgs_prepare kernel-root snapshot only; no memory callback/VMX execution, EPT/other roots/lifetime/DMA proof'")
assert 'inject-nmi' not in s and "query('cont')" in s and "query('cont')" not in s[s.index(" stop()\n"):]
compile(s,sys.argv[2],'exec');pathlib.Path(sys.argv[2]).write_text(s)
