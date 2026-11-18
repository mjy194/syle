"""Make a fresh ext2 L2 root and empty newc archive from the BusyBox fixture."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def member(archive, name):
    with archive.open('rb') as stream:
        data = subprocess.check_output(['cpio', '-i', '--to-stdout', name], stdin=stream)
    if not data:
        raise ValueError(f'missing archive member {name}')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('initramfs', type=Path)
    parser.add_argument('new_directory', type=Path)
    args = parser.parse_args()
    busybox = member(args.initramfs, 'bin/busybox')
    init = member(args.initramfs, 'init')
    out = args.new_directory
    out.mkdir()
    disk = out / 'l2.ext2'
    with tempfile.TemporaryDirectory() as tmp:
        tree = Path(tmp)
        for directory in ('bin', 'dev', 'etc', 'proc', 'root', 'sbin', 'sys', 'tmp'):
            (tree / directory).mkdir()
        for path, data in [('bin/busybox', busybox), ('sbin/init', init)]:
            (tree / path).write_bytes(data)
            (tree / path).chmod(0o755)
        for applet in ('sh ash mount cat echo ls mkdir printf sleep test uname ps halt '
                       'poweroff reboot setsid cttyhack').split():
            (tree / 'bin' / applet).symlink_to('busybox')
        with disk.open('xb') as stream:
            stream.truncate(8 * 1024 * 1024)
        subprocess.run(['mke2fs', '-q', '-t', 'ext2', '-F', '-b', '1024', '-d',
                        str(tree), str(disk)], check=True)
        subprocess.run(['debugfs', '-w', '-f', '/dev/stdin', str(disk)], check=True,
                       input=b'cd /dev\nmknod console c 5 1\nmknod ttyS0 c 4 64\n')
    with (out / 'empty.cpio').open('xb') as stream:
        subprocess.run(['cpio', '-o', '-H', 'newc'], input=b'', stdout=stream, check=True)
    subprocess.run(['e2fsck', '-fn', str(disk)], check=True)


if __name__ == '__main__':
    main()
