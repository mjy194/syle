#!/usr/bin/env python3
"""Same-handle pause observer. Receipt times, not guest instruction boundaries."""
import argparse
import json
from pathlib import Path
import re
import select
import socket
import time

def validate_report(report, sent):
    if not sent or len(report["start_ticks"])!=2 or len(report["stop_ticks"])<2:
        raise RuntimeError("missing STOP/START evidence")
    if report["start_ticks"][1]!=report["stop_ticks"][0]:raise RuntimeError("paused virtual ticks changed")
    if not report.get("handle") or not report.get("pause_pid") or report.get("pause_pid_after")!=report["pause_pid"]:
        raise RuntimeError("missing handle/same-process identity")
    if not all(k in report for k in ("mono_ns","raw_ns","tsc_delta")):raise RuntimeError("missing clock deltas")
    report["host_pause_observed_ns"]=report["resume_sent_ns"]-report["paused_received_ns"]
    if report["host_pause_observed_ns"]<report["requested_pause_seconds"]*1e9:
        raise RuntimeError("host pause shorter than requested")

def record_line(report, text, received):
    verified=False;mgmt=False
    m=re.fullmatch(r"HOST_CLOCK\.(START|STOP) ticks=(0x[0-9a-fA-F]+)",text)
    if m:report[m[1].lower()+"_ticks"].append(int(m[2],16))
    m=re.fullmatch(r"SYLE\.CLOCK_HANDLE (0x[0-9a-fA-F]+)",text)
    if m:report["handle"]=int(m[1],16)
    m=re.fullmatch(r"(mono_ns|raw_ns|tsc_delta|pause_pid|pause_pid_after)=(\d+)",text)
    if m:
        if m[1] in report:raise RuntimeError("duplicate metric")
        report[m[1]]=int(m[2])
    if text=="SYLE.CLOCK_PAUSED":
        if "paused_received_ns" in report:raise RuntimeError("duplicate paused marker")
        report["paused_received_ns"]=received
    if text=="SYLE.CLOCK_PAUSE_VERIFIED same handle and process; cleaned up":verified=True
    if re.search(r"MGMT\.EXIT status=0x0+\b",text):mgmt=True
    if "SYLE.CLOCK_PAUSE_FAILED" in text or text.startswith("SYLE_CLOCK_ERROR"):
        raise RuntimeError(text)
    return verified,mgmt

def main():
    p=argparse.ArgumentParser()
    p.add_argument("socket")
    p.add_argument("--output-dir",required=True)
    p.add_argument("--pause-seconds",type=float,default=2.0)
    p.add_argument("--timeout",type=float,default=900.0)
    a=p.parse_args()
    if not 0.5<=a.pause_seconds<=10 or not 5<=a.timeout<=1800:
        p.error("bounded pause 0.5..10s and timeout 5..1800s required")
    out=Path(a.output_dir);out.mkdir(mode=0o700,parents=False,exist_ok=False)
    report={"scope":"external STOP/START handshake; receipt boundaries", "requested_pause_seconds":a.pause_seconds,"start_ticks":[],"stop_ticks":[]}
    deadline=time.monotonic()+a.timeout;line=bytearray();sent=False;verified=False;mgmt=False
    try:
        with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as s, (out/"serial.log").open("wb") as log:
            s.connect(a.socket)
            while not (verified and mgmt):
                now=time.monotonic()
                if now>=deadline:raise TimeoutError("pause observation deadline")
                if "paused_received_ns" in report and not sent and time.monotonic_ns()-report["paused_received_ns"]>=a.pause_seconds*1e9:
                    report["resume_sent_ns"]=time.monotonic_ns()
                    if s.send(b"R")!=1:raise RuntimeError("resume byte not fully sent; no retry")
                    sent=True
                wait=max(0.0,min(0.01,deadline-time.monotonic()))
                if not select.select([s],[],[],wait)[0]:continue
                data=s.recv(4096);received=time.monotonic_ns()
                if not data:raise EOFError("closed before checked completion")
                log.write(data);log.flush()
                for b in data:
                    if b==13:continue
                    if b!=10:
                        line.append(b)
                        if len(line)>8192:raise RuntimeError("oversized serial line")
                        continue
                    text=line.decode("ascii",errors="replace");line.clear()
                    v,g=record_line(report,text,received)
                    verified=verified or v;mgmt=mgmt or g
            validate_report(report,sent)
            report["completed"]=True
    except Exception as e:
        report["error"]=str(e)
        raise
    finally:
        (out/"report.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report),flush=True)
if __name__=="__main__":main()
