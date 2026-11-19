"""Build a manual M2 firmware profile in a new, isolated directory."""
import argparse
from pathlib import Path
import shutil
import subprocess

p=argparse.ArgumentParser()
p.add_argument('output',type=Path)
p.add_argument('efi_prefix')
p.add_argument('ovmf_code')
p.add_argument('ovmf_vars')
p.add_argument('--dual-vcpu',action='store_true',help='experimental two-vCPU L1 firmware bring-up')
p.add_argument('--nested',action='store_true',help='enable the guest VMX initialization profile')
p.add_argument('--host-parallel',action='store_true',help='run the two L1 vCPUs on distinct Syle host CPUs')
p.add_argument('--trace-l2-clock',action='store_true',help='trace explicitly tagged L2 RDTSC samples')
p.add_argument('--nested-vmx-timer',action='store_true',help='advertise the L1 VMX preemption timer')
a=p.parse_args()
if a.host_parallel and not a.dual_vcpu:
    p.error('--host-parallel requires --dual-vcpu')
if a.trace_l2_clock and not (a.dual_vcpu and a.nested):
    p.error('--trace-l2-clock requires --dual-vcpu and --nested')
if a.nested_vmx_timer and not (a.host_parallel and a.nested):
    p.error('--nested-vmx-timer requires --host-parallel and --nested')
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
    if a.dual_vcpu:
        f.write('build/fmg-observer.o: KFLAGS += -DSYLE_M2_DUAL_VCPU\n'
                'build/fmg-observer.o: ../krnl/firmware_ap_start.inc\n')
    if a.trace_l2_clock:
        f.write('build/fmg-observer.o: KFLAGS += -DSYLE_TRACE_L2_CLOCK\n')
    if a.nested_vmx_timer:
        f.write('build/fmg-observer.o build/fmg-nested-model.o: KFLAGS += -DSYLE_NESTED_VMX_TIMER\n')
    if a.host_parallel:
        f.write('build/fdisk-loader.o: CFLAGS += -DSYLE_M2_HOST_PARALLEL\n'
                'build/fmg-ring3.o build/fmg-observer.o build/fp-linux_vcpu_backend.o: KFLAGS += -DSYLE_M2_HOST_PARALLEL\n'
                'build/fp-native-asm.o: FP_NATIVE_FLAGS += -DSYLE_M2_HOST_PARALLEL\n'
                'build/fmg-observer.o: ../krnl/firmware_host_parallel.inc ../krnl/smp_boot_platform.inc\n'
                'build/krnl-firmware-disk.elf: build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o\n')
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
