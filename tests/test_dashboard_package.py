from pathlib import Path
import tempfile
import unittest
from tools.project_status.package_dashboard import build, verify, install, FILES


class PackageTests(unittest.TestCase):
    def test_reproducible_install_and_tamper(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / 'source'
            for name in FILES:
                p = src / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(b'fixture\r\n')
            first = build(src, root / 'one')
            second = build(src, root / 'two')
            self.assertEqual(first, second)
            installed = install(root / 'one', root / 'releases')
            self.assertEqual(installed, install(root / 'two', root / 'releases'))
            (root / 'two' / FILES[0]).write_text('corrupt')
            with self.assertRaisesRegex(ValueError, 'Digest mismatch'):
                install(root / 'two', root / 'releases')
            self.assertEqual(first, verify(installed))

    def test_extra_file_and_manifest_escape_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / 'source'
            for name in FILES:
                p = src / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text('fixture')
            build(src, root / 'pkg')
            (root / 'pkg' / 'unexpected').write_text('x')
            with self.assertRaises(ValueError):
                verify(root / 'pkg')
