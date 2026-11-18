"""Exercise the manual Ring3 profile over a dedicated QEMU serial socket."""
import argparse
import contextlib
from pathlib import Path
import re
import secrets
import socket
import time


class Console:
    def __init__(self, stream, log):
        self.stream = stream
        self.log = log
        self.pending = bytearray()

    def receive(self):
        try:
            data = self.stream.recv(65536)
        except TimeoutError:
            return
        if not data:
            raise EOFError("QEMU serial disconnected")
        self.log.write(data)
        self.pending.extend(data)

    def expect(self, marker, timeout=60):
        deadline = time.monotonic() + timeout
        while marker not in self.pending:
            if time.monotonic() >= deadline:
                raise TimeoutError(marker)
            self.receive()
        end = self.pending.index(marker) + len(marker)
        consumed = bytes(self.pending[:end])
        del self.pending[:end]
        return consumed

    def send(self, data):
        for byte in data:
            self.stream.sendall(bytes([byte]))
            time.sleep(0.02)

    def boot(self):
        self.send(b'c')
        self.expect(b'M2 ready')
        self.send(b's')
        self.expect(b'M2 running')
        self.send(b'a')
        self.expect(b'SYLE_DISK_ROOT_READY')
        self.expect(b'# ')

    def shell(self, command, marker):
        self.send(command + b'\r')
        self.expect(b'\n' + marker + b'\r')
        self.expect(b'# ')

    def detach(self):
        self.send(bytes([29]))
        self.expect(b'[q]uit')

    def destroy(self):
        self.detach()
        self.send(b'd')
        report = self.expect(b'M2 destroyed').split(b'FMG.RELEASE generation=')[-1]
        fields = {k: int(v, 16) for k, v in re.findall(rb'(\w+)=0x([0-9a-f]+)', report)}
        if (fields[b'pages_baseline'] != fields[b'pages_after']
                or any(fields[k] for k in (b'vmx_active', b'vmxon', b'vmcs'))
                or any(fields[k] != 1 for k in (b'ept_released', b'adapter_revoked'))):
            raise RuntimeError("guest resource release verification failed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--idle', type=int, default=130)
    args = parser.parse_args()
    if args.idle < 0:
        parser.error("--idle must be nonnegative")
    token = secrets.token_hex(8).encode()
    with contextlib.ExitStack() as stack:
        stream = stack.enter_context(socket.socket(socket.AF_UNIX))
        stream.settimeout(1)
        for _ in range(200):
            try:
                stream.connect(str(args.directory.resolve() / 'serial.sock'))
                break
            except (FileNotFoundError, ConnectionRefusedError):
                time.sleep(0.1)
        else:
            raise RuntimeError("serial socket unavailable")
        terminal = Console(stream, stack.enter_context((args.directory / 'serial.log').open('xb', buffering=0)))
        terminal.expect(b'[q]uit')
        terminal.boot()
        terminal.shell(b"saved=$$; persistctl write " + token + b" && echo 'M2_'ARMED", b'M2_ARMED')
        terminal.detach()
        terminal.send(b'p')
        terminal.expect(b'M2 paused')
        terminal.send(b's')
        terminal.expect(b'M2 running')
        terminal.send(b'a')
        terminal.expect(b'M2 attached')
        deadline = time.monotonic() + args.idle
        while time.monotonic() < deadline:
            terminal.receive()
        read_disk = b'persistctl read ' + token + b' && mount -t ext2 -o remount,ro /dev/vda2 /'
        terminal.shell(b'[ "$saved" = "$$" ] && ' + read_disk + b" && echo 'M2_'AWAKE", b'M2_AWAKE')
        terminal.destroy()
        terminal.boot()
        terminal.shell(read_disk + b" && echo 'M2_'DISK", b'M2_DISK')
        terminal.destroy()
        terminal.send(b'q')
        terminal.expect(b'KRNL.IDLE')
    print(f'PASS: {args.idle}s idle, pause/resume, cold recreate, resource release and exit')


if __name__ == '__main__':
    main()
