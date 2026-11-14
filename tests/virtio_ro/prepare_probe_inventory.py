import pathlib,subprocess,json,hashlib,sys,tempfile
kernel=pathlib.Path(sys.argv[1]);manager=pathlib.Path(sys.argv[2]);out=pathlib.Path(sys.argv[3]);repo=pathlib.Path(__file__).resolve().parents[2]
rows=[x.split() for x in subprocess.check_output(['nm','-S',str(kernel)],text=True).splitlines()]
def symbol(name,size=None):
 matches=[x for x in rows if x[-1]==name and (size is None or len(x)==4 and int(x[1],16)==size)]
 assert len(matches)==1,(name,matches);return int(matches[0][0],16)
expr={'session.size':'sizeof(struct lgs_session)','managed_guest.size':'sizeof(struct guest_control)'}
for f in ['handle','state','busy']:expr['managed_guest.'+f]='offsetof(struct guest_control,'+f+')'
for f in ['prepared','enabled','configured']:expr['session.'+f]='offsetof(struct lgs_session,'+f+')'
for parent,typ,fields in [('host','lvh_context',['active','vmxon_page','vmcs_page']),('memory','linux_guest_memory',['prepared','eptp','pages','tables']),('state','lvs_state',['ready'])]:
 for f in fields:expr['session.'+parent+'.'+f]='offsetof(struct lgs_session,'+parent+')+offsetof(struct '+typ+','+f+')'
with tempfile.TemporaryDirectory() as t:
 c=pathlib.Path(t)/'l.c';e=pathlib.Path(t)/'l';s='#include "linux_guest_session.h"\n#include <stddef.h>\n#include <stdio.h>\nint main(void){'
 for k,v in expr.items():s+='printf("'+k+'=%zu\\n",(size_t)('+v+'));'
 c.write_text(s+'return 0;}');subprocess.run(['cc','-I'+str(repo/'krnl'),str(c),'-o',str(e)],check=True)
 layout={k:int(v) for k,v in (x.split('=') for x in subprocess.check_output([str(e)],text=True).splitlines())}
snap={n:{'address':symbol(n,z),'size':z} for n,z in [('session',layout['session.size']),('managed_guest',layout['managed_guest.size']),('stats',24),('available_count',4),('available',4096*16)]}
start=symbol('source_prepare_window_start');end=symbol('source_prepare_window_end');assert end-start==6
# Exact decoded loop instruction starts: PAUSE(2), DEC ecx(2), JNZ rel8(2).
dis=subprocess.check_output(['objdump','-d','--disassemble=source_prepare_pause',str(kernel)],text=True)
assert 'pause' in dis and 'dec' in dis and 'jne' in dis
out.write_text(json.dumps({'kernel_sha256':hashlib.sha256(kernel.read_bytes()).hexdigest(),'snapshots':snap,'layout':layout,'managers':{'0':{'sha256':hashlib.sha256(manager.read_bytes()).hexdigest()}},'prepare_window':{'instruction_rips':[start,start+2,start+4],'end_rejected':end,'paging_root_address':symbol('root',8),'pages_count':32768,'tables_count':67}},indent=2)+'\n')
