import subprocess
import unittest
from tools.project_status.run_autonomous_checks import execute, NATIVE_REGEX


class AutonomousTests(unittest.TestCase):
    def test_nonzero_exit_cannot_be_pass(self):
        result = execute(['test'], '.', lambda *a, **k: subprocess.CompletedProcess(a, 2, '', 'failure'))
        self.assertEqual('FAIL', result['status'])

    def test_missing_runtime_is_blocked(self):
        def missing(*a, **k):
            raise FileNotFoundError('fixture runtime')
        self.assertEqual('BLOCKED', execute(['missing'], '.', missing)['status'])

    def test_timeout_is_blocked(self):
        def timeout(*a, **k):
            raise subprocess.TimeoutExpired('fixture', 180)
        self.assertEqual('BLOCKED', execute(['timeout'], '.', timeout)['status'])

    def test_native_selection_excludes_live_tools(self):
        import re
        self.assertIsNotNone(re.fullmatch(NATIVE_REGEX, 'efis_freshness_budget_tests'))
        self.assertIsNone(re.fullmatch(NATIVE_REGEX, 'efis_recorder'))
