#!/usr/bin/env python3
"""Attach this terminal to an explicitly supplied local QEMU Unix serial socket.
No TCP listener, credentials, system configuration or external packages.
"""
import argparse
import os
import select
import socket
import sys
import termios
import tty


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("socket", help="path to the existing QEMU serial Unix socket")
    args = parser.parse_args()
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        parser.error("requires an interactive terminal on stdin and stdout")
    fd = sys.stdin.fileno()
    previous = termios.tcgetattr(fd)
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as stream:
        stream.connect(args.socket)
        print("Connected. Ctrl-] stops the Syle guest; Ctrl-\\ exits this client.", flush=True)
        try:
            tty.setraw(fd)
            while True:
                ready, _, _ = select.select([fd, stream], [], [])
                if stream in ready:
                    data = stream.recv(4096)
                    if not data:
                        break
                    sys.stdout.buffer.write(data)
                    sys.stdout.buffer.flush()
                if fd in ready:
                    data = os.read(fd, 128)
                    if not data or b"\x1c" in data:
                        break
                    stream.sendall(data)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, previous)
    print("\nDisconnected.")


if __name__ == "__main__":
    main()
