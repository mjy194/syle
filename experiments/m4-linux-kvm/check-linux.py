"""Verify L2 Linux earlycon, PID 1 and teardown on one M2 --socket instance."""
import argparse
from pathlib import Path
import secrets
import socket
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'm2'))
from check_manual import Console


def check_shell(console):
    console.expect(b'# ',timeout=180)
    token=secrets.token_hex(6).encode()
    console.send(b"read who < /proc/1/comm; printf 'L2-%s-%s\\n' " + token + b' "$who"\n')
    console.expect(b'L2-' + token + b'-init',timeout=180)
    console.expect(b'# ',timeout=180)
    console.send(b'cat /proc/cmdline\n')
    console.expect(b'rdinit=/init', timeout=180)
    console.expect(b'# ', timeout=180)
    console.send(b'poweroff -f\n')
    console.expect(b'System halted',timeout=180)
    console.expect(b'L2_SHUTDOWN_PASS halted=1 IF=0',timeout=180)


def check_disk(console):
    token = secrets.token_hex(12).encode()
    for boot in (1, 2):
        console.expect(f'L2_DISK_BOOT {boot}'.encode(), timeout=180)
        console.expect(b'L2 direct64 rip=')
        console.expect(b'VFS: Mounted root (ext2 filesystem)', timeout=900)
        console.expect(b'L2 | SYLE_LINUX_USERSPACE_READY', timeout=900)
        console.expect(b'# ', timeout=180)
        console.send(b'cat /proc/mounts\n')
        console.expect(b' / ext2 rw,', timeout=180)
        console.expect(b'# ', timeout=180)
        if boot == 1:
            console.send(b"echo " + token + b" > /root/probe && echo 'L2_'WROTE\n")
            console.expect(b'L2_WROTE', timeout=180)
            console.expect(b'# ', timeout=180)
        console.send(b'cat /root/probe\n')
        console.expect(b'\n' + token + b'\r', timeout=180)
        console.expect(b'# ', timeout=180)
        console.send(b"mount -o remount,ro /dev/vda / && echo 'L2_'CLEAN\n")
        console.expect(b'L2_CLEAN', timeout=180)
        console.expect(b'# ', timeout=180)
        console.send(b'poweroff -f\n')
        console.expect(b'L2_SHUTDOWN_PASS halted=1 IF=0', timeout=180)
        console.expect(b'KVM_PROBE_DONE rc=0', timeout=180)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--shell',action='store_true')
    mode.add_argument('--disk',action='store_true')
    args=parser.parse_args()
    directory=args.run_directory
    with socket.socket(socket.AF_UNIX) as stream, (directory / 'serial.log').open('xb', buffering=0) as log:
        stream.connect(str(directory / 'serial.sock'))
        stream.settimeout(1)
        console = Console(stream, log)
        console.expect(b'[q]uit')
        for key, marker in ((b'n', b'M2 ready'), (b's', b'M2 running'), (b'a', b'SYLE_DISK_ROOT_READY')):
            console.send(key)
            console.expect(marker)
        if args.disk:
            check_disk(console)
        else:
            console.expect(b'L2 direct64 rip=')
            for marker in (b'earlycon: uart8250', b'Unpacking initramfs', b'Run /init as init process',
                           b'SYLE_LINUX_USERSPACE_READY'):
                received = console.expect(marker, timeout=900)
                if not received.split(b'\n')[-1].startswith(b'L2 | '):
                    raise RuntimeError(f'missing L2 serial prefix for {marker!r}')
            if args.shell:
                check_shell(console)
            else:
                console.expect(b'L2_LINUX_INIT_PASS')
            console.expect(b'KVM_PROBE_DONE rc=0')
        console.expect(b'# ')
        console.shell(b"mount -t ext2 -o remount,ro /dev/vda2 / && echo 'M4_'SYNC", b'M4_SYNC')
        console.destroy()
        console.send(b'q')
        console.expect(b'MGMT.EXIT status=0x0000000000000000')
        console.expect(b'KRNL.IDLE')
    if args.disk:
        print('PASS: two L2 disk-root boots, persisted data and resource release')
        return
    print('PASS: L2 shell, command round trip, shutdown and resource release' if args.shell else
          'PASS: L2 Linux early console, initramfs /init and resource release')


if __name__ == '__main__':
    main()
