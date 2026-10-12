import unittest
from tools.project_status.sync_backup import check_preserved, check_tag_conflicts


class BackupGuardTests(unittest.TestCase):
    def test_unchanged_tags_allowed(self):
        check_tag_conflicts({'refs/tags/v1': 'a'}, {'refs/tags/v1': 'a'}, {})

    def test_server_tag_conflict(self):
        with self.assertRaisesRegex(RuntimeError, 'server tag conflict'):
            check_tag_conflicts({'refs/tags/v1': 'a'}, {'refs/tags/v1': 'b'}, {})

    def test_nas_tag_conflict(self):
        with self.assertRaisesRegex(RuntimeError, 'NAS tag conflict'):
            check_tag_conflicts({'refs/tags/v1': 'a'}, {}, {'refs/tags/v1': 'b'})

    def test_main_can_advance_without_changing_legacy(self):
        check_preserved({'refs/heads/main': 'a', 'refs/heads/legacy': 'b'},
                        {'refs/heads/main': 'c', 'refs/heads/legacy': 'b'}, 'c')

    def test_deleted_branch_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'ref changed'):
            check_preserved({'refs/heads/legacy': 'b'}, {'refs/heads/main': 'c'}, 'c')

    def test_changed_branch_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'ref changed'):
            check_preserved({'refs/heads/legacy': 'b'},
                            {'refs/heads/main': 'c', 'refs/heads/legacy': 'd'}, 'c')

    def test_wrong_main_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'main mismatch'):
            check_preserved({}, {'refs/heads/main': 'a'}, 'c')
