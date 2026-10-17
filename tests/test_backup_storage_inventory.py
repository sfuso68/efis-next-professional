from pathlib import Path
import tempfile
import unittest
from tools.project_status.inventory_backup_storage import inventory


class InventoryTests(unittest.TestCase):
    def test_sizing_never_selects_deletion(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            p = root / 'before.bundle'
            p.write_bytes(b'fixture')
            (root / 'latest.json').write_text('{}')
            report = inventory(root)
            self.assertEqual(1, report['count'])
            self.assertEqual(7, report['bundle_bytes'])
            self.assertEqual([], report['deletion_candidates'])
            self.assertFalse(report['modified_backup_files'])
            self.assertEqual(b'fixture', p.read_bytes())

    def test_missing_root_is_not_empty_backup(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(FileNotFoundError):
                inventory(Path(tmp) / 'missing')
