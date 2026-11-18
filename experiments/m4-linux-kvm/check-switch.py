"""Check nested/on, ordinary/off, nested/on on one M2 --socket instance."""
import argparse
from pathlib import Path
import socket
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'm2'))
from check_manual import Console


def check_guest(console, nested, full_l2, long_mode):
    console.send(b'n' if nested else b'c')
    console.expect(b'M2 ready')
    console.send(b's')
    console.expect(b'M2 running')
    console.send(b'a')
    console.expect(b'SYLE_DISK_ROOT_READY')
    if nested:
        console.expect(b'vmx=1')
        if full_l2 or long_mode:
            for marker in (b'KVM_RUN 1 exit=2', b'L2 OUT A', b'KVM_RUN 2 exit=2',
                           b'L2 OUT B', b'KVM_RUN 3 exit=5'):
                console.expect(marker)
            if long_mode:
                console.expect(b'L2_64 RIP=18 R8=1234567800000042 RAX=1234567800000042')
                console.expect(b'KVM_L2_64_RESUME_PASS')
            else:
                console.expect(b'KVM_L2_RESUME_PASS')
        else:
            console.expect(b'KVM_GET_API_VERSION=12')
        console.expect(b'KVM_PROBE_DONE rc=0')
    else:
        console.expect(b'vmx=0')
        console.expect(b'open /dev/kvm: No such file or directory')
        console.expect(b'KVM_PROBE_DONE rc=1')
    console.expect(b'# ')
    console.shell(b"mount -t ext2 -o remount,ro /dev/vda2 / && echo 'M4_'SYNC", b'M4_SYNC')
    console.destroy()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--long-mode', action='store_true', help='require 64-bit L2 register and RIP checks')
    mode.add_argument('--full-l2', action='store_true', help='require two L2 I/O exits and HLT')
    args = parser.parse_args()
    directory = args.run_directory
    with socket.socket(socket.AF_UNIX) as stream, (directory / 'serial.log').open('xb', buffering=0) as log:
        stream.connect(str(directory / 'serial.sock'))
        stream.settimeout(1)
        console = Console(stream, log)
        console.expect(b'[q]uit')
        for nested in (True, False, True):
            check_guest(console, nested, args.full_l2, args.long_mode)
        console.send(b'q')
        console.expect(b'MGMT.EXIT status=0x0000000000000000')
        console.expect(b'KRNL.IDLE')
    print('PASS: nested/ordinary/nested probe and resource release')


if __name__ == '__main__':
    main()
