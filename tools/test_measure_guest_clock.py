#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import unittest
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
spec=importlib.util.spec_from_file_location("clock",Path(__file__).with_name("measure_guest_clock.py"))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class ClockTest(unittest.TestCase):
    def test_wait_race_is_nonnegative(self):
        self.assertEqual(m.bounded_wait(-0.001, 10), 0)
        self.assertEqual(m.bounded_wait(0.02, -0.001), 0)
        self.assertEqual(m.bounded_wait(-1, -1), 0)
        self.assertEqual(m.bounded_wait(0.02, 0.01), 0.01)
        self.assertEqual(m.bounded_wait(0.02, 10), 0.02)

    def test_order_and_exact_lines(self):
        c=m.Case("clock10",0)
        c.line(b"echo SYLE_CLOCK_BEGIN clock10",1)
        self.assertNotIn("begin_received_ns",c.data)
        c.line(b"SYLE_CLOCK_BEGIN clock10",2)
        c.line(b"SYLE_CLOCK_END clock10",3)
        for line in (b"mono_ns=123",b"raw_ns=124",b"tsc_delta=20"):
            c.line(line,4)
        c.line(b"SYLE_CLOCK_DONE clock10",5)
        self.assertEqual(c.data["mono_ns"],123)
        self.assertEqual(c.data["done_received_ns"],5)
    def test_reject_incomplete(self):
        with self.assertRaises(ValueError):m.Case("clock0",0).line(b"SYLE_CLOCK_END clock0",1)
        with self.assertRaises(ValueError):m.Case("clock0",0).line(b"SYLE_CLOCK_DONE clock0",1)
    def test_reject_duplicate(self):
        c=m.Case("clock0",0);c.line(b"SYLE_CLOCK_BEGIN clock0",1)
        with self.assertRaises(ValueError):c.line(b"SYLE_CLOCK_BEGIN clock0",2)
    @unittest.skipUnless(os.environ.get("SYLE_CLOCK_SOCKET_TEST")=="1", "explicit local socket mock opt-in")
    def test_socket_sequence_and_clean_close(self):
        # Mock serial transport only, not timing or Linux acceptance.
        with tempfile.TemporaryDirectory(prefix="clock-test-") as tmp:
            root=Path(tmp);path=root/"serial.sock";errors=[]
            with socket.socket(socket.AF_UNIX) as server:
                server.bind(str(path));server.listen(1);server.settimeout(5)
                def peer():
                    try:
                        conn,_=server.accept()
                        with conn:
                            conn.settimeout(5)
                            conn.sendall(b"SYLE_LINUX_USERSPACE_READY\r\n# ")
                            command=b""
                            while not command.endswith(b"\n"):
                                command+=conn.recv(1)
                            assert command==b"/bin/baseline-probe clock0\n"
                            conn.sendall(command+b"\r\nSYLE_CLOCK_BEGIN clock0\nSYLE_CLOCK_END clock0\n"
                                b"mono_ns=42\nraw_ns=43\ntsc_delta=1000\nSYLE_CLOCK_DONE clock0\n# ")
                            assert conn.recv(1)==b"\x1d"
                            conn.sendall(b"\nSYLE.TERMINAL detached; guest cleaned up\nMGMT.EXIT status=0x0000000000000000\n")
                    except Exception as e:errors.append(e)
                thread=threading.Thread(target=peer);thread.start()
                result=subprocess.run([sys.executable,str(Path(__file__).with_name("measure_guest_clock.py")),
                    str(path),"--output-dir",str(root/"out"),"--modes","clock0","--repeats","1",
                    "--pace-ms","20","--timeout","5"],capture_output=True,timeout=10)
                thread.join(6)
                self.assertFalse(thread.is_alive());self.assertEqual(errors,[])
                self.assertEqual(result.returncode,0,result.stderr.decode())
                report=json.loads((root/"out/report.json").read_text())
                self.assertTrue(report["completed"]);self.assertTrue(report["mgmt_zero"])
                self.assertEqual(report["cases"][0]["mono_ns"],42)
if __name__=="__main__":unittest.main()
