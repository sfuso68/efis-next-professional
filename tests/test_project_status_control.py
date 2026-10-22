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
        project_control = (
            ROOT / ".github/workflows/efis-project-control.yml").read_text(
                encoding="utf-8")
        core_ci = (ROOT / ".github/workflows/efis-core-ci.yml").read_text(
            encoding="utf-8")
        self.assertIn("'safety/**'", project_control)
        self.assertIn("tests.test_project_status_control", project_control)
        self.assertIn("'ci/**'", project_control)
        self.assertIn("'ci/**'", core_ci)

    def test_project_control_cannot_exempt_itself_or_workflows(self):
        for path in ("tools/project_status/check_status_update.py",
                     "tools/project_status/activate_dashboard.py",
                     "tools/project_status/generate_dashboard.py",
                     ".github/workflows/efis-project-control.yml"):
            with self.subTest(path=path):
                result = self.run_python(str(CHECKER), path)
                self.assertEqual(result.returncode, 2, result.stdout)
                accepted = self.run_python(str(CHECKER), path,
                    "docs/status/EFIS_NEXT_MASTER_MATRIX.md")
                self.assertEqual(accepted.returncode, 0, accepted.stdout)

    def test_normalized_relative_and_windows_paths_are_watched(self):
        for path in ("./tools/project_status/example.py",
                     r"tools\project_status\example.py",
                     "docs/../tools/project_status/example.py"):
            with self.subTest(path=path):
                self.assertEqual(self.run_python(str(CHECKER), path).returncode, 2)

    def test_generated_document_only_changes_do_not_recurse(self):
        result = self.run_python(str(CHECKER),
            "docs/status/PROJECT_DASHBOARD.html", "docs/status/efis-next-status.json")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_generated_docs_cannot_cover_executable_tool_change(self):
        result = self.run_python(str(CHECKER),
            "tools/project_status/dashboard_server.py",
            "docs/status/PROJECT_DASHBOARD.html")
        self.assertEqual(result.returncode, 2, result.stdout)

    def test_core_sanitizer_job_is_blocking(self):
        workflow = (ROOT / ".github/workflows/efis-core-ci.yml").read_text(
            encoding="utf-8")
        marker = "  linux-sanitizers:\n"
        self.assertIn(marker, workflow)
        sanitizer_job = workflow.split(marker, 1)[1]
        self.assertNotIn("continue-on-error:", sanitizer_job)
        self.assertIn("-fsanitize=address,undefined", sanitizer_job)
        self.assertIn("ctest --test-dir build/sanitizers", sanitizer_job)


if __name__ == "__main__":
    unittest.main()
