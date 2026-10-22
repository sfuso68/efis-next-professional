"""Execute each shipped ExecCondition CLI against isolated temporary releases.

No systemd, service, /home node path or ExecStart executable is invoked. Only
interpreter/script/root paths are remapped; the deployed validation flags remain.
This proves CLI rejection, not systemd restart semantics or ARM64 acceptance.
"""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "tools/deployment/validate_release_manifest.py"


class ReleaseExecConditionTests(unittest.TestCase):
    def setUp(self):
        policy = json.loads((ROOT / "config/deployment/node-release-policy-v1.json").read_text())
        self.profiles = {row["role"]: row for row in policy["nodes"]}
        self.units = sorted((ROOT / "deploy/systemd/release-v1").glob("*.conf"))

    def run_condition(self, unit, mutation=None):
        directives = [line.split("=", 1)[1] for line in unit.read_text().splitlines()
                      if line.startswith("ExecCondition=") and line != "ExecCondition="]
        self.assertEqual(len(directives), 1, unit.name)
        argv = shlex.split(directives[0])
        self.assertEqual(argv[:2], ["/usr/bin/python3",
                                  "/usr/local/lib/efis-next/validate_release_manifest.py"])
        role = argv[argv.index("--expected-role") + 1]
        profile = self.profiles[role]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            artifact = root / profile["executable"]
            artifact.parent.mkdir(parents=True)
            artifact.write_bytes(b"fixture only; never executed\n")
            artifact.chmod(0o755)
            manifest = {
                "schema": "efis-next.release-manifest.v1",
                "release_id": "isolated-condition-v1",
                "source_commit": "0" * 40,
                "built_utc": "2026-10-22T07:00:00Z",
                "roles": [role], "platform": profile["platform"],
                "repository": "sfuso68/efis-next-professional",
                "flight_approval": False, "simulation_mode": True,
                "artifacts": [{"path": profile["executable"], "executable": True,
                    "sha256": hashlib.sha256(artifact.read_bytes()).hexdigest()}],
            }
            if mutation == "tampered_artifact":
                artifact.write_bytes(b"tampered after manifest creation\n")
            elif mutation == "wrong_role":
                manifest["roles"] = ["PFD" if role != "PFD" else "EMS"]
            elif mutation == "flight_approval":
                manifest["flight_approval"] = True
            elif mutation == "wrong_platform":
                manifest["platform"] = ("RPI4_AARCH64" if profile["platform"] !=
                                        "RPI4_AARCH64" else "RPI5_AARCH64")
            path = root / "release-manifest.json"
            path.write_text(json.dumps(manifest), encoding="utf-8")
            if mutation == "missing_artifact":
                artifact.unlink()
            argv[:2] = [sys.executable, str(VALIDATOR)]
            argv[argv.index("--manifest") + 1] = str(path)
            argv[argv.index("--release-root") + 1] = str(root)
            result = subprocess.run(argv, text=True, capture_output=True, cwd=ROOT,
                                    timeout=15, check=False)
            return result, json.loads(result.stdout)

    def test_each_shipped_condition_accepts_intact_fixture(self):
        self.assertEqual(len(self.units), 5)
        for unit in self.units:
            with self.subTest(unit=unit.name):
                result, report = self.run_condition(unit)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(report["status"], "PASS")
                self.assertEqual(report["artifacts_checked"], 1)
                self.assertFalse(report["flight_approval"])

    def test_each_shipped_condition_rejects_modified_bytes(self):
        for unit in self.units:
            with self.subTest(unit=unit.name):
                result, report = self.run_condition(unit, "tampered_artifact")
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertEqual(report["status"], "FAIL")
                self.assertIn("sha256 mismatch", report["error"])

    def test_each_shipped_condition_rejects_missing_artifact(self):
        for unit in self.units:
            with self.subTest(unit=unit.name):
                result, report = self.run_condition(unit, "missing_artifact")
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("artifact missing", report["error"])

    def test_each_shipped_condition_enforces_role(self):
        for unit in self.units:
            with self.subTest(unit=unit.name):
                result, report = self.run_condition(unit, "wrong_role")
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("role does not match", report["error"])

    def test_each_shipped_condition_rejects_flight_approval(self):
        for unit in self.units:
            with self.subTest(unit=unit.name):
                result, report = self.run_condition(unit, "flight_approval")
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("flight_approval must remain false", report["error"])

    def test_each_shipped_condition_enforces_platform(self):
        for unit in self.units:
            with self.subTest(unit=unit.name):
                result, report = self.run_condition(unit, "wrong_platform")
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("platform does not match", report["error"])


if __name__ == "__main__":
    unittest.main()
