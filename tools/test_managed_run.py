#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("managed_run", Path(__file__).with_name("managed_run.py"))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class RunnerTest(unittest.TestCase):
    def test_bounded_private_command(self):
        cmd = m.command(Path("/tmp/private-run"), Path("/tmp/code.fd"), "/usr/bin/qemu")
        self.assertIn("q35,accel=kvm", cmd)
        self.assertIn("host", cmd)
        self.assertEqual(cmd[cmd.index("-smp") + 1], "1")
        self.assertEqual(cmd[cmd.index("-net") + 1], "none")
        self.assertIn("format=raw,readonly=on,file=fat:ro:/tmp/private-run/esp", cmd)
        self.assertIn("socket,id=terminal,path=/tmp/private-run/console.sock,server=on,wait=on,logfile=/tmp/private-run/serial.log,logappend=off", cmd)
        self.assertNotIn("-enable-kvm-fallback", cmd)

    def test_input_hash_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in m.FIXED:
                (root / name).write_bytes(b"not the frozen input")
            with self.assertRaisesRegex(ValueError, "hashes differ"):
                m.fixed_inputs(root)

    def test_existing_run_refused_before_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(SystemExit) as error:
                m.main(["--inputs", tmp, "--run-dir", tmp,
                        "--ovmf-code", tmp, "--ovmf-vars", tmp])
            self.assertEqual(error.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
