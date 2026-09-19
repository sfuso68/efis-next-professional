#!/usr/bin/env python3
"""Process-level proof that recorder storage failure is visible and fail-closed."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


RECORDER = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None


class RecorderStorageFailureTests(unittest.TestCase):
    def test_open_failure_publishes_error_and_nonzero_exit(self) -> None:
        self.assertIsNotNone(RECORDER, "recorder executable argument is required")
        self.assertTrue(RECORDER.is_file(), f"recorder executable missing: {RECORDER}")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            blocked_output = root / "records"
            blocked_output.write_text("not-a-directory", encoding="utf-8")
            status = root / "run" / "recorder.status"
            process = subprocess.run(
                [str(RECORDER), str(blocked_output), str(status)],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=5,
                check=False,
            )
            self.assertNotEqual(process.returncode, 0, process.stdout)
            self.assertIn("event=recorder_failed", process.stdout)
            published = status.read_text(encoding="utf-8")
            self.assertIn("EFIS_RECORDER_STATUS_V2", published)
            self.assertIn("state=ERROR", published)
            self.assertIn("records=0", published)
            self.assertIn("write_failed=1", published)
            self.assertIn("last_error=", published)
            self.assertFalse(any(root.rglob("*.csv")))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
