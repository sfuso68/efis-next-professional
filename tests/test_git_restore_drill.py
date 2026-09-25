import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

from tools.recovery.git_restore_drill import DrillError, load_config, perform_drill


ROOT = pathlib.Path(__file__).parents[1]
CONFIG = ROOT / "config" / "recovery" / "git-restore-drill-v1.json"


def git(*args: str, cwd: pathlib.Path) -> str:
    result = subprocess.run(
        ["git", *args], cwd=cwd, check=True, capture_output=True, text=True,
        encoding="utf-8",
    )
    return result.stdout.strip()


class GitRestoreDrillTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = pathlib.Path(self.temp.name)
        self.source_work = self.base / "source-work"
        self.source_bare = self.base / "source.git"
        self.source_work.mkdir()
        git("init", "-b", "main", cwd=self.source_work)
        git("config", "user.name", "Restore Test", cwd=self.source_work)
        git("config", "user.email", "restore@example.invalid", cwd=self.source_work)
        for item in load_config(CONFIG)["required_paths"]:
            path = self.source_work / item
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(f"fixture for {item}\n", encoding="utf-8")
        git("add", ".", cwd=self.source_work)
        git("commit", "-m", "fixture", cwd=self.source_work)
        git("tag", "restore-fixture-v1", cwd=self.source_work)
        subprocess.run(
            ["git", "clone", "--bare", str(self.source_work), str(self.source_bare)],
            check=True, capture_output=True, text=True,
        )
        self.expected = git("rev-parse", "HEAD", cwd=self.source_work)

    def tearDown(self):
        self.temp.cleanup()

    def drill(self, expected: str | None = None, tags: list[str] | None = None):
        return perform_drill(
            load_config(CONFIG), str(self.source_bare), self.base / "restored",
            expected or self.expected, self.base / "report.json",
            tags or ["restore-fixture-v1"], False,
        )

    def test_isolated_restore_passes_and_retains_tree(self):
        report = self.drill()
        self.assertEqual(report["status"], "PASS")
        self.assertFalse(report["flight_approval"])
        self.assertEqual(len(report["config_sha256"]), 64)
        self.assertTrue((self.base / "restored" / ".git").is_dir())
        self.assertEqual(report["details"]["restored_main"], self.expected)
        self.assertEqual(
            json.loads((self.base / "report.json").read_text(encoding="utf-8"))["status"],
            "PASS",
        )

    def test_wrong_main_fails_closed(self):
        report = self.drill(expected="0" * 40)
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(report["checks"]["main_matches_expected"])

    def test_missing_required_tag_fails_closed(self):
        report = self.drill(tags=["missing-tag"])
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(report["checks"]["required_tags_present"])

    def test_existing_destination_is_refused_without_mutation(self):
        destination = self.base / "restored"
        destination.mkdir()
        marker = destination / "keep"
        marker.write_text("unchanged", encoding="utf-8")
        with self.assertRaises(DrillError):
            perform_drill(
                load_config(CONFIG), str(self.source_bare), destination,
                self.expected, self.base / "report.json", [], False,
            )
        self.assertEqual(marker.read_text(encoding="utf-8"), "unchanged")

    def test_credentials_in_url_are_rejected(self):
        with self.assertRaises(DrillError):
            perform_drill(
                load_config(CONFIG), "https://user:secret@example.invalid/repo.git",
                self.base / "restored", self.expected,
                self.base / "report.json", [], False,
            )

    def test_cli_rejects_short_commit(self):
        result = subprocess.run(
            [sys.executable, str(ROOT / "tools" / "recovery" / "git_restore_drill.py"),
             "--config", str(CONFIG), "--source", str(self.source_bare),
             "--destination", str(self.base / "restored"), "--expected-main", "abc",
             "--report", str(self.base / "report.json")],
            cwd=ROOT, check=False, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("full 40-character", result.stderr)


if __name__ == "__main__":
    unittest.main()
