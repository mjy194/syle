#!/usr/bin/env python3
"""Build a NEW pinned-source kernel in a never-reused directory; no download/install."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
from recipe import SOURCE_SHA256,SOURCE_URL,FORMAT,digest,check_config,apply_compat

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-tar',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--jobs',type=int,default=2);a=p.parse_args()
 if not 1<=a.jobs<=32:p.error('jobs must be 1..32')
 if digest(a.source_tar)!=SOURCE_SHA256:p.error('source archive does not match Linux6.12 official pin')
 required=['make','gcc','ld','as','objcopy','nm','strip','readelf','gzip','xz','bc','bison','flex','perl','tar']
 missing=[t for t in required if not shutil.which(t)]
 if missing:p.error('missing build tools: '+','.join(missing))
 tools={}
 for t in required:
  path=Path(shutil.which(t)).resolve();v=subprocess.run([str(path),'--version'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True).stdout
  tools[t]={'path':str(path),'sha256':digest(path),'version':v.splitlines()[0] if v else ''}
 # Record the actual GCC compiler backend too, not only its driver.
 cc1=Path(subprocess.check_output(['gcc','-print-prog-name=cc1'],text=True).strip()).resolve();tools['cc1']={'path':str(cc1),'sha256':digest(cc1)}
 root=a.output.resolve();root.mkdir(parents=False,exist_ok=False)
 source=root/'linux-6.12';obj=root/'objects';obj.mkdir()
 fragment=Path(__file__).with_name('config.fragment').resolve();shutil.copyfile(fragment,root/'config.fragment')
 env={'PATH':os.environ.get('PATH','/usr/bin:/bin'),'HOME':str(root)}
 settings={'ARCH':'x86_64','KBUILD_BUILD_TIMESTAMP':'2024-11-17 00:00:00 UTC','KBUILD_BUILD_USER':'syle','KBUILD_BUILD_HOST':'rebuild','KBUILD_BUILD_VERSION':'1','SOURCE_DATE_EPOCH':'1731801600','LC_ALL':'C','TZ':'UTC','LOCALVERSION':'','KCONFIG_ALLCONFIG':str(fragment),'PATH':env['PATH']}
 env.update(settings)
 with (root/'build.log').open('w') as log:
  def run(cmd):
   log.write('+ '+repr(cmd)+'\n');log.flush();subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
  run(['tar','-xJf',str(a.source_tar.resolve()),'-C',str(root),'--no-same-owner','--no-same-permissions'])
  compat=apply_compat(source)
  shutil.copyfile(Path(__file__).with_name('compressed-gnu11.patch'),root/'compressed-gnu11.patch')
  log.write('compatibility: '+json.dumps(compat,sort_keys=True)+'\n');log.flush()
  args=['make','-C',str(source),'O='+str(obj),'CC=gcc','HOSTCC=gcc','LD=ld']
  run(args+['allnoconfig']);check_config(fragment,obj/'.config')
  shutil.copyfile(obj/'.config',root/'resolved.config')
  run(args+['-j'+str(a.jobs),'bzImage'])
  if digest(obj/'.config')!=digest(root/'resolved.config'):raise ValueError('configuration changed during build')
 for t,record in tools.items():
  if digest(record['path'])!=record['sha256']:raise ValueError('tool changed during build: '+t)
 shutil.copyfile(obj/'arch/x86/boot/bzImage',root/'bzImage')
 record={'format':FORMAT,'source_url':SOURCE_URL,'source_sha256':SOURCE_SHA256,'fragment_sha256':digest(root/'config.fragment'),'config_sha256':digest(root/'resolved.config'),'kernel_sha256':digest(root/'bzImage'),'compatibility':compat,'tools':tools,'environment':settings,'status':'built-not-runtime-validated'}
 (root/'build.json').write_text(json.dumps(record,indent=2,sort_keys=True)+'\n')
 print(json.dumps(record,sort_keys=True))
if __name__=='__main__':main()
