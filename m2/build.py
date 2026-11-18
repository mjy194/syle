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
p.add_argument('--nested',action='store_true',help='enable the guest VMX initialization profile')
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
    if a.nested:
        f.write('build/fmg-observer.o build/k-guest-control.o build/fmg-nested-model.o build/fmg-nested-ept.o: KFLAGS += -DSYLE_M2_NESTED\n'
                'build/fmg-manager.o: FMG_USER_FLAGS += -DSYLE_M2_NESTED\n'
                'build/fmg-observer.o: ../krnl/firmware_kvm_init.inc ../krnl/firmware_nested_entry.inc\n'
                'build/fmg-nested-operand.o: ../krnl/nested_operand.c | build\n'
                '\t$(CC) $(KFLAGS) -c $< -o $@\n'
                'build/fmg-nested-model.o: ../krnl/nested_vmx.c | build\n'
                '\t$(CC) $(KFLAGS) -c $< -o $@\n'
                'build/fmg-invept.o: ../krnl/nested_local_invept.c | build\n'
                '\t$(CC) $(KFLAGS) -c $< -o $@\n'
                'build/fmg-nested-ept.o: ../krnl/nested_ept.c | build\n'
                '\t$(CC) $(KFLAGS) -c $< -o $@\n'
                'build/fmg-sparse.o: ../krnl/nested_ept_sparse.c | build\n'
                '\t$(CC) $(KFLAGS) -c $< -o $@\n'
                'build/krnl-firmware-disk.elf: build/fmg-sparse.o build/fmg-nested-ept.o build/fmg-nested-operand.o build/fmg-nested-model.o build/fmg-invept.o\n')
subprocess.run(['sh',str(repo/'experiments/m2-snapshots/graphics-progress/build.sh'),str(out),a.efi_prefix,a.ovmf_code,a.ovmf_vars],check=True)
