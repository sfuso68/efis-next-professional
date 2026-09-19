import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools/project_status/generate_status.py"
CHECKER = ROOT / "tools/project_status/check_status_update.py"


class ProjectStatusControlTests(unittest.TestCase):
    def run_python(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, *args], cwd=ROOT, text=True,
            capture_output=True, check=False)

    def test_generator_rejects_illegal_status_instead_of_aliasing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            matrix = root / "matrix.md"
            matrix.write_text(
                "| ID | Area | Node | Stato | Evidenza |\n"
                "|---|---|---|---|---|\n"
                "| TEST-001 | Test | Core | DONE-SW | evidence |\n",
                encoding="utf-8")
            result = self.run_python(
                str(GENERATOR), "--matrix", str(matrix),
                "--markdown", str(root / "status.md"),
                "--json", str(root / "status.json"))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Unsupported project status", result.stderr)

    def test_state_file_alone_does_not_satisfy_matrix_rule(self):
        result = self.run_python(
            str(CHECKER), "src/core/example.cpp",
            "docs/status/EFIS_NEXT_STATE.md")
        self.assertEqual(result.returncode, 2)
        self.assertIn("EFIS_NEXT_MASTER_MATRIX.md", result.stdout)

    def test_matrix_update_satisfies_rule(self):
        result = self.run_python(
            str(CHECKER), "src/apps/example.cpp",
            "docs/status/EFIS_NEXT_MASTER_MATRIX.md")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_all_safety_critical_source_and_tool_paths_are_watched(self):
        paths = (
            "src/apps/example.cpp", "src/protocol/example.cpp",
            "src/display/example.cpp", "src/emergency/example.cpp",
            "src/recording/example.cpp", "src/voice/example.cpp",
            "src/setup/example.cpp", "src/simulation/example.cpp",
            "tools/ui/example.py", "tools/network/example.py",
            "tools/hardware/example.py", "tools/protocol/example.py",
            "tools/groundstation/example.py", "tools/blackbox/example.py",
        )
        for path in paths:
            with self.subTest(path=path):
                result = self.run_python(str(CHECKER), path)
                self.assertEqual(result.returncode, 2, path)

    def test_ci_runs_control_regressions_on_safety_branches(self):
        workflow = (ROOT / ".github/workflows/efis-project-control.yml").read_text(
            encoding="utf-8")
        self.assertIn("'safety/**'", workflow)
        self.assertIn("tests.test_project_status_control", workflow)


if __name__ == "__main__":
    unittest.main()
