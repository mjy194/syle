#!/usr/bin/env python3
"""Bounded external-terminal clock measurements; no guest/core clock changes.

Timestamps are host RECEIPT times, not exact guest execution times. UART queues,
manager drain and scheduling contribute delay. Compare clock0 and several sleep
durations/repeats; do not equate send-to-reply latency with nanosleep duration.
"""
import argparse
import json
from pathlib import Path
import re
import select
import socket
import time

MODES = ("clock0", "clock10", "clock100", "clock1000", "clockspin")


def bounded_wait(wait, remaining):
    # Both caller pacing and the overall deadline may elapse between reads.
    return max(0.0, min(wait, remaining))

class Case:
    def __init__(self, mode, repeat):
        self.data = {"mode": mode, "repeat": repeat}

    def line(self, line, timestamp):
        mode = self.data["mode"].encode()
        if line == b"SYLE_CLOCK_BEGIN " + mode:
            if "begin_received_ns" in self.data:
                raise ValueError("duplicate BEGIN")
            self.data["begin_received_ns"] = timestamp
        elif line == b"SYLE_CLOCK_END " + mode:
            if "begin_received_ns" not in self.data or "end_received_ns" in self.data:
                raise ValueError("END ordering")
            self.data["end_received_ns"] = timestamp
        elif line == b"SYLE_CLOCK_DONE " + mode:
            if not all(k in self.data for k in ("end_received_ns", "mono_ns", "raw_ns", "tsc_delta")):
                raise ValueError("incomplete clock result")
            if "done_received_ns" in self.data:
                raise ValueError("duplicate DONE")
            self.data["done_received_ns"] = timestamp
        elif line.startswith(b"SYLE_CLOCK_ERROR"):
            raise ValueError(line.decode(errors="replace"))
        else:
            match = re.fullmatch(rb"(mono_ns|raw_ns|tsc_delta)=(\d+)", line)
            if match and "end_received_ns" in self.data:
                key = match[1].decode()
                if key in self.data:
                    raise ValueError("duplicate metric")
                self.data[key] = int(match[2])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("socket", type=Path)
    p.add_argument("--output-dir", type=Path, required=True, help="must not exist")
    p.add_argument("--modes", nargs="+", choices=MODES, default=list(MODES))
    p.add_argument("--repeats", type=int, default=3)
    p.add_argument("--pace-ms", type=float, default=150)
    p.add_argument("--timeout", type=int, default=900)
    a = p.parse_args()
    if not 1 <= a.repeats <= 10 or not 1 <= a.timeout <= 3600 or not 20 <= a.pace_ms <= 1000:
        p.error("repeats 1..10, timeout 1..3600, pace-ms 20..1000 required")
    a.output_dir.mkdir(mode=0o700)
    report = {"scope": "serial receipt timing; includes transport/scheduling latency",
              "pace_ms": a.pace_ms, "clock": "time.monotonic_ns", "cases": [], "completed": False}
    deadline = time.monotonic() + a.timeout
    line = bytearray()
    tail = bytearray()
    case = None
    ready = False
    prompt = False
    cleaned = False
    mgmt_zero = False
    with open(a.output_dir / "serial.log", "xb") as log, socket.socket(socket.AF_UNIX) as stream:
        stream.connect(str(a.socket))
        stream.setblocking(False)

        def pump(wait):
            nonlocal ready, prompt, cleaned, mgmt_zero
            if time.monotonic() >= deadline:
                raise TimeoutError("bounded measurement deadline")
            readable, _, _ = select.select([stream], [], [], bounded_wait(wait, deadline-time.monotonic()))
            if not readable:
                return
            data = stream.recv(4096)
            received = time.monotonic_ns()
            if not data:
                raise EOFError("serial closed before test completed")
            log.write(data)
            log.flush()
            tail.extend(data)
            del tail[:-4096]
            if b"SYLE_LINUX_USERSPACE_READY" in tail:
                ready = True
            for b in data:
                if b == 13:
                    continue
                if b == 10:
                    value = bytes(line)
                    if case:
                        case.line(value, received)
                    if b"SYLE.TERMINAL detached; guest cleaned up" in value:
                        cleaned = True
                    if re.search(rb"MGMT\.EXIT status=0x0+\b", value):
                        mgmt_zero = True
                    line.clear()
                else:
                    line.append(b)
                    if len(line) > 16384:
                        raise ValueError("serial line exceeds bound")
                if ready and line.endswith(b"# "):
                    prompt = True

        def send(data):
            for b in data:
                # A local single byte avoids sendall's partial-write ambiguity.
                if stream.send(bytes([b])) != 1:
                    raise RuntimeError("incomplete host byte send; no retry")
                stop = time.monotonic() + a.pace_ms / 1000
                while time.monotonic() < stop:
                    pump(min(0.02, stop-time.monotonic()))

        try:
            while not (ready and prompt):
                pump(0.1)
            for repeat in range(a.repeats):
                for mode in a.modes:
                    case = Case(mode, repeat)
                    report["cases"].append(case.data)
                    prompt = False
                    case.data["send_started_ns"] = time.monotonic_ns()
                    send(f"/bin/baseline-probe {mode}\n".encode())
                    case.data["send_finished_ns"] = time.monotonic_ns()
                    while "done_received_ns" not in case.data or not prompt:
                        pump(0.1)
                    case.data["input_elapsed_ns"] = case.data["send_finished_ns"]-case.data["send_started_ns"]
                    case.data["begin_to_end_received_ns"] = case.data["end_received_ns"]-case.data["begin_received_ns"]
                    case.data["send_to_done_ns"] = case.data["done_received_ns"]-case.data["send_started_ns"]
                    print(json.dumps(case.data), flush=True)
            case = None
            if stream.send(b"\x1d") != 1:
                raise RuntimeError("incomplete detach byte; no retry")
            while not (cleaned and mgmt_zero):
                pump(0.1)
            report["completed"] = True
        except Exception as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            report["cleaned"] = cleaned
            report["mgmt_zero"] = mgmt_zero
            (a.output_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
