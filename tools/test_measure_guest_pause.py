import importlib.util
from pathlib import Path
import unittest
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import json
spec=importlib.util.spec_from_file_location("pause",Path(__file__).with_name("measure_guest_pause.py"))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class TestPause(unittest.TestCase):
    def good(self):
        return dict(requested_pause_seconds=2,start_ticks=[0,123],stop_ticks=[123,135],handle=42,pause_pid=6,pause_pid_after=6,mono_ns=100,raw_ns=100,tsc_delta=200,paused_received_ns=1000,resume_sent_ns=2000001000)
    def test_ok(self):
        r=self.good();m.validate_report(r,True);self.assertEqual(r["host_pause_observed_ns"],2000000000)
    def test_bad_ticks_pid_or_missing(self):
        for k,v in (("start_ticks",[0,124]),("pause_pid_after",7),("stop_ticks",[123]),("handle",0)):
            r=self.good();r[k]=v
            with self.assertRaises(RuntimeError):m.validate_report(r,True)
        r=self.good();del r["raw_ns"]
        with self.assertRaises(RuntimeError):m.validate_report(r,True)
    def test_actual_multiline_protocol(self):
        r={"start_ticks":[],"stop_ticks":[]}
        text="HOST_CLOCK.START ticks=0x0000000000000000\nHOST_CLOCK.STOP ticks=0x000000000000007b\n pit_edges=0x0000000000000003\n irq0_accepted=0x0000000000000002\n\nHOST_CLOCK.START ticks=0x000000000000007b\n"
        for line in text.splitlines():m.record_line(r,line,1000)
        self.assertEqual(r["start_ticks"],[0,123]);self.assertEqual(r["stop_ticks"],[123])
        m.record_line(r,"SYLE.CLOCK_PAUSED",1000)
        with self.assertRaises(RuntimeError):m.record_line(r,"SYLE.CLOCK_PAUSED",2000)
        self.assertEqual(m.record_line(r,"MGMT.EXIT status=0x0000000000000000",3000),(False,True))
    def test_short_pause_rejected(self):
        r=self.good();r["resume_sent_ns"]=1999999999
        with self.assertRaises(RuntimeError):m.validate_report(r,True)
    @unittest.skipUnless(os.environ.get("SYLE_CLOCK_SOCKET_TEST")=="1","requires local Unix socket permission")
    def test_socket_handshake(self):
        with tempfile.TemporaryDirectory() as d:
            path=str(Path(d)/"serial.sock");server=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);server.bind(path);server.listen(1)
            observed=[]
            def serve():
                with server, server.accept()[0] as conn:
                    conn.sendall(b"HOST_CLOCK.START ticks=0x0\nHOST_CLOCK.STOP ticks=0x7b\nSYLE.CLOCK_HANDLE 0x2a\npause_pid=6\nSYLE.CLOCK_PAUSED\n")
                    start=time.monotonic();observed.append(conn.recv(1));observed.append(time.monotonic()-start)
                    conn.sendall(b"HOST_CLOCK.START ticks=0x7b\nmono_ns=100\nraw_ns=100\ntsc_delta=200\npause_pid_after=6\nHOST_CLOCK.STOP ticks=0x87\nSYLE.CLOCK_PAUSE_VERIFIED same handle and process; cleaned up\nMGMT.EXIT status=0x0\n")
            thread=threading.Thread(target=serve,daemon=True);thread.start()
            result=subprocess.run([sys.executable,str(Path(__file__).with_name("measure_guest_pause.py")),path,"--output-dir",str(Path(d)/"results"),"--pause-seconds","0.5","--timeout","5"],capture_output=True,text=True,timeout=8)
            thread.join(1);self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(observed[0],b"R");self.assertGreaterEqual(observed[1],0.49)
            report=json.loads((Path(d)/"results/report.json").read_text());self.assertTrue(report["completed"])
    def test_no_resume(self):
        with self.assertRaises(RuntimeError):m.validate_report(self.good(),False)
if __name__=="__main__":unittest.main()
