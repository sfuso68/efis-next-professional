import tempfile
import unittest
from pathlib import Path
from tools.project_status.generate_dashboard import build


class DashboardTests(unittest.TestCase):
    def test_deterministic_and_safe_embedding(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'matrix.md'
            path.write_text('| TST-001 | </script><img> | N | TODO | E | R | Next | Close |\n', encoding='utf-8')
            first = build(path)
            self.assertEqual(first, build(path))
            self.assertNotIn('</script><img>', first)
            self.assertIn('matrix_sha256', first)
            self.assertIn('acceptance', first)

    def test_rejects_incomplete_columns(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'matrix.md'
            path.write_text('| TST-001 | R | N | TODO | E |\n', encoding='utf-8')
            with self.assertRaises(ValueError):
                build(path)
