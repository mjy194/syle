#!/usr/bin/env python3
"""Host-only regression for the provenance collector, not a kernel scenario."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
SPEC=importlib.util.spec_from_file_location('fingerprint',Path(__file__).resolve().parents[1]/'runtime_fingerprint.py')
fingerprint=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(fingerprint)

class FingerprintTests(unittest.TestCase):
    def test_file_hash_and_missing_path(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'image';p.write_bytes(b'verified input')
            r=fingerprint.file_record(p)
            self.assertEqual(r['status'],'recorded')
            self.assertEqual(r['bytes'],14)
            self.assertEqual(r['sha256'],hashlib.sha256(b'verified input').hexdigest())
            self.assertEqual(fingerprint.file_record(Path(tmp)/'missing')['status'],'missing-or-not-regular-file')
            self.assertEqual(fingerprint.file_record(tmp)['status'],'missing-or-not-regular-file')
    def test_missing_tools_are_explicit(self):
        with patch.dict('os.environ',{},clear=True),patch('shutil.which',return_value=None),patch('subprocess.run',side_effect=FileNotFoundError('not found')):
            r=fingerprint.collect()
        self.assertTrue(all(v['status']=='unavailable' for v in r['tools'].values()))
        self.assertTrue(all(v['status']=='not-explicitly-set' for v in r['inputs'].values()))
    def test_timeout_and_failure_are_not_success(self):
        with patch.dict('os.environ',{},clear=True),patch('shutil.which',return_value=None),patch('subprocess.run',side_effect=subprocess.TimeoutExpired('tool',10)):
            self.assertTrue(all(v['status']=='unavailable' for v in fingerprint.collect()['tools'].values()))
        with patch.dict('os.environ',{},clear=True),patch('shutil.which',return_value=None),patch('subprocess.run',return_value=subprocess.CompletedProcess(['tool'],7,'failed')):
            r=fingerprint.collect()
        self.assertTrue(all(v['status']=='version-command-failed' and v['exitCode']==7 for v in r['tools'].values()))
    def test_fixed_build_tools_ignore_unused_environment_overrides(self):
        with patch.dict('os.environ',{'CC':'unused-cc','LD':'unused-ld','MAKE':'unused-make','QEMU':'selected-qemu','GDB':'selected-gdb'},clear=True),patch('shutil.which',return_value=None),patch('subprocess.run',return_value=subprocess.CompletedProcess(['tool'],0,'version')):
            r=fingerprint.collect()
        self.assertEqual([r['tools'][k]['command'][0] for k in ('gcc','ld','make','qemu','gdb')],['gcc','ld','make','selected-qemu','selected-gdb'])
    def test_only_allowed_environment_is_reported(self):
        # A harmless sentinel tests that unrelated environment content is never dumped.
        with patch.dict('os.environ',{'UNRELATED_TEST_VALUE':'DO_NOT_COLLECT_SENTINEL'},clear=True),patch('shutil.which',return_value=None),patch('subprocess.run',return_value=subprocess.CompletedProcess(['tool'],0,'v1\na\nb\nc\nd\n')) as run:
            r=fingerprint.collect()
        self.assertNotIn('DO_NOT_COLLECT_SENTINEL',json.dumps(r))
        self.assertTrue(all(len(v['versionLines'])==4 for v in r['tools'].values()))
        self.assertTrue(all(c.kwargs['timeout']==10 and c.kwargs.get('check') is False for c in run.call_args_list))
        self.assertTrue(all(isinstance(c.args[0],list) and c.args[0][-1]=='--version' for c in run.call_args_list))

if __name__=='__main__':unittest.main(verbosity=2)
