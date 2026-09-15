from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "groundstation" / "qgc_appdir_digest.py"
SPEC = importlib.util.spec_from_file_location("qgc_appdir_digest", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class QgcAppdirDigestTests(unittest.TestCase):
    def make_tree(self, root: Path) -> None:
        (root / "usr" / "bin").mkdir(parents=True)
        executable = root / "AppRun"
        executable.write_bytes(b"#!/bin/sh\n")
        executable.chmod(0o755)
        (root / "usr" / "bin" / "QGroundControl").write_bytes(b"binary")

    def test_digest_is_deterministic_and_counts_entries(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            first = MODULE.digest_tree(root)
            second = MODULE.digest_tree(root)
        self.assertEqual(first, second)
        self.assertEqual(first["file_count"], 2)
        self.assertEqual(first["symlink_count"], 0)
        self.assertEqual(first["directory_count"], 2)
        self.assertEqual(first["file_bytes"], 16)

    def test_file_content_mutation_changes_digest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            before = MODULE.digest_tree(root)["tree_sha256"]
            (root / "usr" / "bin" / "QGroundControl").write_bytes(b"changed")
            after = MODULE.digest_tree(root)["tree_sha256"]
        self.assertNotEqual(before, after)

    def test_symlink_target_mutation_changes_digest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            try:
                (root / "current").symlink_to("usr/bin/QGroundControl")
            except OSError as error:
                self.skipTest(f"symlinks unavailable: {error}")
            before = MODULE.digest_tree(root)["tree_sha256"]
            (root / "current").unlink()
            (root / "current").symlink_to("AppRun")
            after = MODULE.digest_tree(root)["tree_sha256"]
        self.assertNotEqual(before, after)

    def test_executable_bit_mutation_changes_digest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            before = MODULE.digest_tree(root)["tree_sha256"]
            before_mode = (root / "AppRun").stat().st_mode & 0o111
            (root / "AppRun").chmod(0o644)
            after_mode = (root / "AppRun").stat().st_mode & 0o111
            if after_mode == before_mode:
                self.skipTest("executable mode mutation unavailable")
            after = MODULE.digest_tree(root)["tree_sha256"]
        self.assertNotEqual(before, after)

    def test_missing_root_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "missing"):
            MODULE.digest_tree(Path("does-not-exist"))


if __name__ == "__main__":
    unittest.main()
