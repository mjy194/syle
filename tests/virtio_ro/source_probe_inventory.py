import pathlib,subprocess,struct,json,hashlib,sys,tempfile
elf=pathlib.Path(sys.argv[1]);out=pathlib.Path(sys.argv[2]);b=elf.read_bytes()
syms={}
for line in subprocess.check_output(['nm','-S',str(elf)],text=True).splitlines():
 p=line.split()
 if len(p) in (3,4):syms.setdefault(p[-1],[]).append(int(p[0],16))
def one(n):
 v=syms[n];assert len(v)==1,(n,v);return v[0]
phoff=struct.unpack_from('<Q',b,32)[0];ents,count=struct.unpack_from('<HH',b,54)
segments=[]
for i in range(count):
 typ,flags,off,va,pa,fs,ms,align=struct.unpack_from('<IIQQQQQQ',b,phoff+i*ents)
 if typ==1:segments.append((va,fs,off,flags))
def read(va,n):
 hits=[(v,z,o,f) for v,z,o,f in segments if v<=va and n<=z-(va-v)]
 assert len(hits)==1;v,z,o,f=hits[0];assert f==4;return b[o+va-v:o+va-v+n]
ro=one('__rodata_start');end=one('__rodata_end')
# PT_LOAD file bytes can end before the page-aligned rodata_end.
segment=[x for x in segments if x[0]==ro and x[3]==4];assert len(segment)==1
rb=b[segment[0][2]:segment[0][2]+segment[0][1]]
command=b'console=ttyS0,115200 earlyprintk=serial,ttyS0,115200 nokaslr rdinit=/init\0'
assert rb.count(command)==1
sources={}
for name in ('kernel','initrd'):
 start=one('linux_'+name+'_start');stop=one('linux_'+name+'_end');assert ro<=start<stop<=end
 sources[name]={'va':start,'length':stop-start,'sha256':hashlib.sha256(read(start,stop-start)).hexdigest()}
cmdva=ro+rb.index(command);sources['command']={'va':cmdva,'length':len(command),'sha256':hashlib.sha256(command).hexdigest()}
repo=pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as t:
 c=pathlib.Path(t)/'layout.c';exe=pathlib.Path(t)/'layout'
 fields=['kernel','kernel_size','initrd','initrd_size','command_line']
 text='#include "linux_guest_session.h"\n#include <stdio.h>\n#include <stddef.h>\nint main(void){printf("%zu",sizeof(struct lgs_session));'
 for f in fields:text+='printf(" %zu",offsetof(struct lgs_session,'+f+'));'
 c.write_text(text+'return 0;}');subprocess.run(['cc','-I'+str(repo/'krnl'),str(c),'-o',str(exe)],check=True)
 nums=list(map(int,subprocess.check_output([str(exe)],text=True).split()))
assert nums[0]==266688,'frozen kernel session layout changed; investigate'
result={'kernel_sha256':hashlib.sha256(b).hexdigest(),'sources':sources,'rodata':[ro,end],'physical_bound_address':one('address_limit'),'layout':dict(zip(fields,nums[1:])),'session_size':nums[0],'physical_bound_basis':'frozen actual PMM cpu_limit CPUID.80000008 bits, clamped down by PMM_DIRECT_MAP_LIMIT; conservative not exact MAXPHYADDR'}
out.write_text(json.dumps(result,indent=2)+'\n')
