import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools.project_status import serve_dashboard as service


class RefreshTests(unittest.TestCase):
    def test_success_and_failure_preserve_snapshot(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            repo = root / 'github.git'
            repo.mkdir()
            with patch.object(service, 'ROOT', root), patch.object(service, 'REPO', repo):
                with patch.object(service, 'git', side_effect=['', 'abc', '2026-10-10', '| TST-001 | Test | N | TODO | E | R | Next | Close |']):
                    service.refresh()
                previous = (root / 'index.html').read_bytes()
                self.assertTrue(json.loads((root / 'status.json').read_text())['github_verified'])
                with patch.object(service, 'git', side_effect=RuntimeError('offline')):
                    with self.assertRaises(RuntimeError):
                        service.refresh()
                self.assertEqual(previous, (root / 'index.html').read_bytes())
                self.assertEqual('ERROR', json.loads((root / 'status.json').read_text())['status'])
