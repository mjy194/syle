"""Build the manual single-vCPU M2 profile in a new, isolated directory."""
import argparse
from pathlib import Path
import shutil
import subprocess

p=argparse.ArgumentParser()
p.add_argument('output',type=Path)
p.add_argument('efi_prefix')
p.add_argument('ovmf_code')
p.add_argument('ovmf_vars')
a=p.parse_args()
repo=Path(__file__).resolve().parents[1]
out=a.output.resolve()
out.mkdir(parents=True,exist_ok=False)
src=out/'source'
for part in ['efi','boot','krnl','include','user']:
    shutil.copytree(repo/part,src/part,ignore=shutil.ignore_patterns('build','tests','__pycache__','.git'))
shutil.copytree(repo/'m2/platform',src,dirs_exist_ok=True)
shutil.copyfile(src/'user/mgmt/firmware_manual.c',src/'user/mgmt/firmware_lifecycle.c')
with (src/'efi/firmware-managed-runtime.mk').open('a') as f:
    f.write('\nbuild/fmg-ring3.o build/fconsole-runtime.o: KFLAGS += -DSYLE_HOST_UART_INPUT\n')
subprocess.run(['sh',str(repo/'experiments/m2-snapshots/graphics-progress/build.sh'),str(out),a.efi_prefix,a.ovmf_code,a.ovmf_vars],check=True)
