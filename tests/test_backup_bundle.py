import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest
from tools.project_status.verify_backup_bundle import verify


class BundleTests(unittest.TestCase):
    def test_digest_mismatch_fails_before_git(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'bundle'
            p.write_bytes(b'not a bundle')
            with self.assertRaisesRegex(ValueError, 'SHA-256 mismatch'):
                verify(p, '0' * 64)

    def test_invalid_bundle_fails_even_with_matching_digest(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'bundle'
            p.write_bytes(b'not a bundle')
            with self.assertRaises(subprocess.CalledProcessError):
                verify(p, hashlib.sha256(p.read_bytes()).hexdigest(), tmp)

    def test_round_trip_preserves_archived_refs(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp) / 'source'
            repo.mkdir()
            def git(*args):
                return subprocess.check_output(['git', '-C', str(repo), *args], stderr=subprocess.PIPE)
            git('init', '-b', 'main')
            for name in ['docs/status/EFIS_NEXT_MASTER_MATRIX.md', 'docs/status/EFIS_NEXT_STATE.md', 'CMakeLists.txt']:
                path = repo / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('fixture\n')
            git('add', '.')
            git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-m', 'fixture')
            git('update-ref', 'refs/backup/previous/main', 'HEAD')
            bundle = Path(tmp) / 'before.bundle'
            git('bundle', 'create', str(bundle), '--all')
            result = verify(bundle, hashlib.sha256(bundle.read_bytes()).hexdigest(), tmp)
            self.assertEqual('PASS', result['status'])
            self.assertEqual(2, result['refs_checked'])
            self.assertFalse(result['live_repositories_modified'])
