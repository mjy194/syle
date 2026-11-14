"""Tests extracted functions only. Never executes runner top level or QEMU."""
import ast,pathlib,types,unittest,sys,stat,re,tempfile,hashlib
from source_memory_policy import ram_span,new_file,Refuse
runner_path=pathlib.Path(sys.argv.pop(1));code=runner_path.read_text();tree=ast.parse(code)
def function(name,namespace):
 node=next(n for n in ast.walk(tree) if isinstance(n,ast.FunctionDef) and n.name==name)
 exec(compile(ast.fix_missing_locations(ast.Module(body=[node],type_ignores=[])),'isolated-'+name,'exec'),namespace)
 return namespace[name]
class Test(unittest.TestCase):
 def test_no_injection(self):
  for n in ast.walk(tree):
   if isinstance(n,ast.Call) and isinstance(n.func,ast.Name) and n.func.id=='query' and n.args and isinstance(n.args[0],ast.Constant):self.assertNotIn(n.args[0].value,('inject-nmi','write-memory','human-monitor-command write'))
  self.assertEqual(code.count("query('cont')"),1)
 def test_identity(self):
  current={'birth':'12','exe':'/usr/bin/qemu-system-x86_64','cmd':b'/usr/bin/qemu-system-x86_64\0-S\0','pid_done':None,'inode':3}
  class Path:
   def __init__(self,p):self.p=p
   def read_bytes(self):return current['cmd']
  ns={'proc':types.SimpleNamespace(pid=17,poll=lambda:current['pid_done']),'stamp':lambda:current['birth'],'birth':'12','args':['/usr/bin/qemu-system-x86_64','-S'],'record':lambda x:None,'pathlib':types.SimpleNamespace(Path=Path),'os':types.SimpleNamespace(readlink=lambda p:current['exe'],lstat=lambda p:types.SimpleNamespace(st_mode=stat.S_IFSOCK,st_dev=2,st_ino=current['inode'])),'stat':stat,'sock_identity':(2,3)}
  f=function('identity',ns);f()
  for key,bad in [('birth','13'),('exe','/bin/other'),('cmd',b'wrong\0'),('pid_done',0),('inode',4)]:
   old=current[key];current[key]=bad
   with self.assertRaises(AssertionError):f()
   current[key]=old
 def test_query_denial(self):
  ns={'re':re};q=function('query',ns)
  for cmd,args in [('inject-nmi',None),('device_add',None),('human-monitor-command',{'command-line':'write 0x1000 1'}),('human-monitor-command',{'command-line':'pmemsave 0 1 /tmp/foreign'}),('human-monitor-command',{'command-line':'xp /1gx 0x100000;write 1'}),('human-monitor-command',{'command-line':'xp /1gx 0x100000\nwrite 1'})]:
   with self.assertRaises(AssertionError):q(cmd,args)
 def test_pmemsave_gate(self):
  with tempfile.TemporaryDirectory() as t:
   root=pathlib.Path(t);existing=root/'before-session.bin';existing.write_bytes(b'old')
   ns={'re':re,'ram_span':ram_span,'new_file':new_file,'p':root,'allowed_ram':[(1<<20,128<<20)]};q=function('query',ns)
   for pa,n,path in [(0xa0000,4096,str(root/'hole.bin')),(1<<20,4096,str(existing)),(1<<20,4096,'/tmp/escape.bin'),(128<<20,1,str(root/'outside.bin'))]:
    with self.assertRaises(Refuse):q('pmemsave',{'val':pa,'size':n,'filename':path})
   self.assertEqual(existing.read_bytes(),b'old')
 def test_actual_startup_control(self):
  prefix=[]
  for node in tree.body:
   if isinstance(node,ast.ImportFrom) and node.module=='source_memory_policy':break
   prefix.append(node)
  program=compile(ast.fix_missing_locations(ast.Module(body=prefix,type_ignores=[])),'startup-controls','exec')
  names=[runner_path.name,'source_page_audit.py','source_memory_policy.py','source-inventory.json','inventory-local.json','preflight-baseline.json','frozen-kernel-managers.tar.gz']
  with tempfile.TemporaryDirectory() as t:
   root=pathlib.Path(t)
   for name in names:(root/name).write_bytes(b'fixed')
   (root/'CONTROL.sha256').write_text(''.join(hashlib.sha256(b'fixed').hexdigest()+'  '+name+'\n' for name in names))
   exec(program,{'__file__':str(root/runner_path.name)})
   (root/'source-inventory.json').write_bytes(b'changed')
   with self.assertRaises(AssertionError):exec(program,{'__file__':str(root/runner_path.name)})
if __name__=='__main__':unittest.main()
