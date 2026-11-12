#!/usr/bin/env python3
"""Host-only runner/ABI regression. No guest, QMP or privileged execution."""
import ast
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import runpy
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'ms01'))
import runtime_fingerprint

spec = importlib.util.spec_from_file_location('vector_layout', ROOT / 'efi/tests/ms01_vector_layout.py')
layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(layout)


def registry():
    tree = ast.parse((ROOT / 'ms01/run.py').read_text())
    node = next(n.value for n in tree.body if isinstance(n, ast.Assign)
                and any(isinstance(t, ast.Name) and t.id == 'TESTS' for t in n.targets))
    # Evaluate only this known repository's list literal, not runner side effects.
    return eval(compile(ast.Expression(node), 'suite-registry', 'eval'),
                {'__builtins__': {}, 'str': str, 'ROOT': ROOT})


@contextlib.contextmanager
def stale_evidence():
    evidence = ROOT / 'ms01/evidence'
    evidence.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='suite-contract-', dir=evidence) as temp:
        out = Path(temp)
        old = {'status': 'passed', 'passed': 31, 'runId': 'old-invocation',
               'marker': 'previous successful invocation'}
        for filename in ('report.json', 'scenario-results.json'):
            (out / filename).write_text(json.dumps(old))
        yield out


def invoke_mocked(out, selection):
    # Every caller additionally mocks Popen. Never launch guest, compiler or
    # even a version command while testing the runner's bookkeeping.
    with patch.object(sys, 'argv', ['run.py', '--only', selection, '--output', str(out)]), \
            contextlib.redirect_stdout(io.StringIO()):
        runpy.run_path(str(ROOT / 'ms01/run.py'), run_name='__main__')


class SuiteContractTests(unittest.TestCase):
    def assert_current_pair(self, out, status):
        report = json.loads((out / 'report.json').read_text())
        mapping = json.loads((out / 'scenario-results.json').read_text())
        self.assertEqual(report['status'], status)
        self.assertEqual(mapping['status'], status)
        self.assertEqual(report['runId'], mapping['runId'])
        self.assertNotEqual(report['runId'], 'old-invocation')
        self.assertNotIn('marker', report)
        self.assertNotIn('marker', mapping)
        return report, mapping

    def test_missing_executable_replaces_previous_pass(self):
        with stale_evidence() as out:
            def missing(*args, **kwargs):
                # Both old PASS snapshots must already be invalidated before
                # attempting even the first selected command.
                self.assert_current_pair(out, 'running')
                raise FileNotFoundError('injected missing executable')
            with patch.object(runtime_fingerprint, 'collect', return_value={'test': 'mock'}), \
                    patch('subprocess.Popen', side_effect=missing) as process:
                with self.assertRaises(SystemExit) as stopped:
                    invoke_mocked(out, 'boot-policy')
            self.assertEqual(stopped.exception.code, 1)
            self.assertEqual(process.call_count, 1)  # No retry or fallback.
            report, mapping = self.assert_current_pair(out, 'failed-or-source-changed')
            self.assertEqual((report['passed'], report['blocked'], report['failed']), (0, 0, 1))
            self.assertEqual(report['suites'][0]['status'], 'launch_error')
            self.assertIn('FileNotFoundError', report['suites'][0]['error'])
            self.assertTrue(all(row['evidenceStatus'] == 'incomplete_or_failed_in_this_invocation'
                                for row in mapping['scenarios']))

    def test_launch_error_preserves_only_this_runs_completed_suites(self):
        with stale_evidence() as out:
            completed = Mock()
            completed.wait.return_value = 0
            with patch.object(runtime_fingerprint, 'collect', return_value={'test': 'mock'}), \
                    patch('subprocess.Popen', side_effect=[completed, PermissionError('injected denied launch')]) as process:
                with self.assertRaises(SystemExit) as stopped:
                    invoke_mocked(out, 'boot-policy,boot-exit')
            self.assertEqual(stopped.exception.code, 1)
            self.assertEqual(process.call_count, 2)  # Exactly one attempt each.
            report, _ = self.assert_current_pair(out, 'failed-or-source-changed')
            self.assertEqual((report['passed'], report['blocked'], report['failed']), (1, 0, 1))
            self.assertEqual([(row['id'], row['status']) for row in report['suites']],
                             [('boot-policy', 'passed'), ('boot-exit', 'launch_error')])
            self.assertIn('PermissionError', report['suites'][1]['error'])

    def test_setup_failure_cannot_retain_old_acceptance(self):
        with stale_evidence() as out:
            with patch.object(runtime_fingerprint, 'collect', side_effect=RuntimeError('injected setup failure')), \
                    patch('subprocess.Popen', side_effect=AssertionError('must not start')) as process:
                with self.assertRaisesRegex(RuntimeError, 'injected setup failure'):
                    invoke_mocked(out, 'boot-policy')
            process.assert_not_called()
            report, mapping = self.assert_current_pair(out, 'running')
            self.assertEqual(report['suites'], [])
            self.assertEqual(mapping['scenarios'], [])

    def test_mapping_failure_cannot_publish_terminal_pass(self):
        with stale_evidence() as out:
            completed = Mock()
            completed.wait.return_value = 0
            with patch.object(runtime_fingerprint, 'collect', return_value={'test': 'mock'}), \
                    patch('subprocess.Popen', return_value=completed), \
                    patch('json.loads', side_effect=ValueError('injected invalid mapping')):
                with self.assertRaisesRegex(ValueError, 'injected invalid mapping'):
                    invoke_mocked(out, 'boot-policy')
            report, mapping = self.assert_current_pair(out, 'running')
            self.assertEqual(report['suites'][0]['status'], 'passed')
            self.assertEqual(mapping['scenarios'], [])

    def test_successful_reruns_have_distinct_bound_report_ids(self):
        with stale_evidence() as out:
            identifiers = []
            for _ in range(2):
                completed = Mock()
                completed.wait.return_value = 0
                with patch.object(runtime_fingerprint, 'collect', return_value={'test': 'mock'}), \
                        patch('subprocess.Popen', return_value=completed):
                    with self.assertRaises(SystemExit) as stopped:
                        invoke_mocked(out, 'boot-policy')
                self.assertEqual(stopped.exception.code, 0)
                report, _ = self.assert_current_pair(out, 'passed-selected-suites')
                identifiers.append(report['runId'])
                self.assertEqual((report['passed'], report['blocked'], report['failed']), (1, 0, 0))
            self.assertNotEqual(*identifiers)
            self.assertEqual(list(out.glob('*.tmp')), [])

    def test_final_report_write_failure_is_not_committed_acceptance(self):
        with stale_evidence() as out:
            completed = Mock()
            completed.wait.return_value = 0
            replace = os.replace
            def fail_terminal_report(source, destination):
                if Path(destination) == out / 'report.json' and \
                        json.loads(Path(source).read_text())['status'] == 'passed-selected-suites':
                    raise OSError('injected final report write failure')
                return replace(source, destination)
            with patch.object(runtime_fingerprint, 'collect', return_value={'test': 'mock'}), \
                    patch('subprocess.Popen', return_value=completed), \
                    patch('os.replace', side_effect=fail_terminal_report):
                with self.assertRaisesRegex(OSError, 'injected final report write failure'):
                    invoke_mocked(out, 'boot-policy')
            report = json.loads((out / 'report.json').read_text())
            mapping = json.loads((out / 'scenario-results.json').read_text())
            # The scenario snapshot can be ready before the commit marker.
            # Same IDs alone do not authorize treating this pair as complete.
            self.assertEqual(report['runId'], mapping['runId'])
            self.assertNotEqual(report['runId'], 'old-invocation')
            self.assertEqual(report['status'], 'running')
            self.assertEqual(mapping['status'], 'passed-selected-suites')
            self.assertEqual(list(out.glob('*.tmp')), [])

    def test_unique_commands_and_registered_followups(self):
        suites = registry()
        self.assertEqual(len(suites), len({row[0] for row in suites}))
        by_id = {name: (layer, command) for name, layer, command in suites}
        for name, script in [('firmware-diagnostics', 'efi/tests/firmware_diagnostics.sh'),
                             ('paging-rw-wb', 'krnl/tests/paging_rw_wb.sh'),
                             ('pmm-model', 'krnl/tests/memory_model.sh'),
                             ('suite-contract', 'ms01/tests/suite_contract.py')]:
            self.assertEqual(by_id[name][0], 'host')
            self.assertEqual(by_id[name][1][1], script)
            self.assertTrue((ROOT / script).is_file())
        held = {name for name, layer, command in suites if layer == 'held'}
        self.assertEqual(held, {'paging-boundaries', 'nmi-uart-budget'})
        for name in held:
            self.assertIsNone(by_id[name][1])

    def test_scenario_links_are_current_not_execution_results(self):
        mapping = json.loads((ROOT / 'ms01/scenario-test-map.json').read_text())
        rows = mapping['scenarios']
        ids = {name for name, _, _ in registry()}
        self.assertEqual(len(rows), 64)
        self.assertEqual(len({row['scenarioId'] for row in rows}), 64)
        for row in rows:
            self.assertTrue(row['suiteIds'])
            self.assertLessEqual(set(row['suiteIds']), ids)
            self.assertEqual(row['evidenceStatus'], 'execution_results_are_separate')
        noncanonical = next(row for row in rows if row['scenarioId'] == 'SC-M1-034')
        self.assertIn('paging-boundaries', noncanonical['suiteIds'])
        self.assertIn('failed', noncanonical['scopeLimit'])

    def test_explicit_held_selection_never_starts_a_process(self):
        evidence = ROOT / 'ms01/evidence'
        evidence.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='suite-contract-', dir=evidence) as temp:
            argv = ['run.py', '--only', 'paging-boundaries,nmi-uart-budget', '--output', temp]
            # Avoid even version subprocesses. The aggregate itself must not
            # call Popen, including when held IDs are explicitly selected.
            with patch.object(sys, 'argv', argv), patch.object(runtime_fingerprint, 'collect', return_value={'test': 'mock'}), \
                    patch('subprocess.Popen', side_effect=AssertionError('held suite launched')) as process, \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit) as stopped:
                    runpy.run_path(str(ROOT / 'ms01/run.py'), run_name='__main__')
            self.assertEqual(stopped.exception.code, 1)
            process.assert_not_called()
            report = json.loads((Path(temp) / 'report.json').read_text())
            self.assertEqual(report['status'], 'partial-blocked')
            self.assertEqual((report['passed'], report['blocked'], report['failed']), (0, 2, 0))
            self.assertEqual(report['sourceChangedDuringRun'], [])
            self.assertTrue(all(row['command'] is None and row['status'] == 'not_run_blocked'
                                for row in report['suites']))
            mapped = json.loads((Path(temp) / 'scenario-results.json').read_text())
            self.assertTrue(all(row['evidenceStatus'] == 'incomplete_or_failed_in_this_invocation'
                                for row in mapped['scenarios']))

    def test_production_frame_layout(self):
        layout.check_layout(*layout.production_sources())

    def test_swapped_saved_registers_are_rejected(self):
        source, frames = layout.production_sources()
        source = source.replace('pushq %rax\n    pushq %rbx', 'pushq %rbx\n    pushq %rax', 1)
        with self.assertRaisesRegex(AssertionError, 'saved GPR order'):
            layout.check_layout(source, frames)

    def test_each_consumer_rip_shift_is_rejected(self):
        source, frames = layout.production_sources()
        for name in frames:
            with self.subTest(consumer=name):
                mutated = dict(frames)
                mutated[name] = mutated[name].replace('vector,error,rip,cs', 'vector,rip,error,cs', 1)
                with self.assertRaises(AssertionError):
                    layout.check_layout(source, mutated)

    def test_aligned_stack_not_used_as_frame_pointer(self):
        source, frames = layout.production_sources()
        source = source.replace('movq %rsp,%rdi\n    andq $-16,%rsp',
                                'andq $-16,%rsp\n    movq %rsp,%rdi', 1)
        with self.assertRaises(AssertionError):
            layout.check_layout(source, frames)

    def test_extra_stack_adjustment_is_rejected(self):
        source, frames = layout.production_sources()
        source = source.replace('movq %rsp,%rdi', 'subq $8,%rsp\n    movq %rsp,%rdi', 1)
        with self.assertRaisesRegex(AssertionError, 'common entry frame delivery'):
            layout.check_layout(source, frames)

    def test_frame_argument_adjustment_is_rejected(self):
        source, frames = layout.production_sources()
        source = source.replace('movq %rsp,%rdi', 'movq %rsp,%rdi\n    addq $8,%rdi', 1)
        with self.assertRaisesRegex(AssertionError, 'common entry frame delivery'):
            layout.check_layout(source, frames)

    def test_missing_panic_call_is_rejected(self):
        source, frames = layout.production_sources()
        source = source.replace('call exception_panic', '# call removed', 1)
        with self.assertRaisesRegex(AssertionError, 'required exception_panic call'):
            layout.check_layout(source, frames)


if __name__ == '__main__':
    unittest.main(verbosity=2)
