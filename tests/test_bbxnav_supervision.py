import tempfile
import unittest
from pathlib import Path

from tools.navigation.validate_bbxnav_supervision import PolicyError, validate_policy


ROOT = Path(__file__).resolve().parents[1]
OVERRIDE = ROOT / "deploy/systemd/bbxnav-fail-closed.override.conf"


class BbxnavSupervisionTests(unittest.TestCase):
    def temporary_policy(self, text: str):
        directory = tempfile.TemporaryDirectory()
        path = Path(directory.name) / "override.conf"
        path.write_text(text, encoding="utf-8")
        return directory, path

    def test_committed_policy_is_fail_closed(self):
        result = validate_policy(OVERRIDE)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["restart_prevent_exit_status"], ["2"])

    def test_rejects_unbounded_restart_policy(self):
        directory, path = self.temporary_policy(
            "[Unit]\nStartLimitIntervalSec=0\nStartLimitBurst=3\n"
            "[Service]\nRestart=always\nRestartPreventExitStatus=2\nRestartSec=5s\n"
        )
        with directory, self.assertRaises(PolicyError):
            validate_policy(path)

    def test_rejects_missing_receiver_exit_guard(self):
        directory, path = self.temporary_policy(
            "[Unit]\nStartLimitIntervalSec=60s\nStartLimitBurst=3\n"
            "[Service]\nRestart=on-failure\nRestartPreventExitStatus=1\nRestartSec=5s\n"
        )
        with directory, self.assertRaisesRegex(PolicyError, "exit status 2"):
            validate_policy(path)

    def test_rejects_command_or_source_changes(self):
        directory, path = self.temporary_policy(
            "[Unit]\nStartLimitIntervalSec=60s\nStartLimitBurst=3\n"
            "[Service]\nRestart=on-failure\nRestartPreventExitStatus=2\n"
            "RestartSec=5s\nExecStart=/usr/local/bin/bbxnav --source simulata\n"
        )
        with directory, self.assertRaises(PolicyError):
            validate_policy(path)


if __name__ == "__main__":
    unittest.main()
