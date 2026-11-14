"""Only execute extracted pure admission statements; never runner top level."""
import pathlib,json,re,unittest,sys,textwrap,ast
s=pathlib.Path(sys.argv.pop(1)).read_text();inv=json.loads(pathlib.Path(sys.argv.pop(1)).read_text());layout=inv['layout']
regsblock=textwrap.dedent(s[s.index(' rip=int('):s.index(" mtree=query(")])
stateblock=textwrap.dedent(s[s.index(" checks={'state':"):s.index(" pathlib.Path('before-decoded.json')")])
class Test(unittest.TestCase):
 def test_marker_bytes(self):
  tree=ast.parse(s);tokens=[n.args[0].value for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Name) and n.func.id=='wait_serial' and n.args and isinstance(n.args[0],ast.Constant)]
  self.assertEqual(tokens,[b'SOURCE.PREPARE_CONTEXT_WINDOW\r\n'])
 def test_register_admission(self):
  rip=inv['prepare_window']['instruction_rips'][0]
  template='RIP=%x CS =0008 SS =0010 RFL=00000002 CPL=0 SMM=0'
  exec(regsblock,{'regs':template%rip,'inv':inv,'re':re})
  for text in [(template%inv['prepare_window']['end_rejected']),(template%rip).replace('CS =0008','CS =002b'),(template%rip).replace('SS =0010','SS =0000'),(template%rip).replace('RFL=00000002','RFL=00000202'),(template%rip).replace('RFL=00000002','RFL=00000402'),(template%rip).replace('CPL=0','CPL=3'),(template%rip).replace('SMM=0','SMM=1')]:
   with self.assertRaises(AssertionError):exec(regsblock,{'regs':text,'inv':inv,'re':re})
 def test_preprepare_snapshot(self):
  mg=bytearray(layout['managed_guest.size']);sess=bytearray(layout['session.size'])
  mg[layout['managed_guest.state']]=1;mg[layout['managed_guest.handle']]=1;mg[layout['managed_guest.busy']]=1;sess[layout['session.configured']]=1
  def run():exec(stateblock,{'mg':mg,'sess':sess,'layout':layout,'inv':inv,'u':lambda b,o,n:int.from_bytes(b[o:o+n],'little')})
  run()
  for key in ['session.prepared','session.enabled','session.host.active','session.memory.prepared','session.state.ready','session.memory.eptp','session.host.vmxon_page','session.host.vmcs_page']:
   o=layout[key];sess[o]=1
   with self.assertRaises(AssertionError):run()
   sess[o]=0
  for key,count in [('session.memory.pages',32768),('session.memory.tables',67)]:
   o=layout[key]+count*8-1;sess[o]=1
   with self.assertRaises(AssertionError):run()
   sess[o]=0
  mg[layout['managed_guest.busy']]=0
  with self.assertRaises(AssertionError):run()
if __name__=='__main__':unittest.main()
