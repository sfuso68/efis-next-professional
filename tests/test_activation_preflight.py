"""Preflight must never switch a release or turn evidence into authorization."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.deployment import check_rollout_readiness as preflight


class ActivationPreflightTests(unittest.TestCase):
    def good(self, node="MFD"):
        spec = preflight.NODES[node]
        binary = "efis-blackbox-broker" if node == "BLACKBOX" else "efis-ems-qt" if node == "EMS" else "efis-pfd-qt"
        return {"host": spec["host"], "arch": "aarch64", "model": spec["model"] + " Model B Rev 1.0",
                "current": spec["previous"], "throttled": "throttled=0x0", "sudo_available": True,
                "candidate": {"pass": True}, "previous": {"pass": True},
                "services": {s: {"ActiveState": "active", "MainPID": "123", "executable": spec["previous"] + "/bin/" +
                                 ("efis-recorder" if s == "efis-recorder.service" else binary)} for s in spec["services"]}}

    def test_stale_process_executable_is_rejected(self):
        data = self.good()
        data["services"]["efis-next-mfd-direct.service"]["executable"] = "/tmp/old-binary"
        self.assertIn("process_paths_current", preflight.assess("MFD", data)["blocking_checks"])

    def test_previous_manifest_is_pinned_too(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, spec = self.fixture(root)
            spec["previous_digest"] = spec["digest"]
            self.assertTrue(preflight.check_release(root, spec, candidate=False)["pass"])
            spec["previous_digest"] = "0" * 64
            self.assertFalse(preflight.check_release(root, spec, candidate=False)["pass"])

    def test_pass_is_not_activation_or_flight_authorization(self):
        for node in preflight.NODES:
            report = preflight.assess(node, self.good(node))
            self.assertEqual(report["status"], "AUTOMATIC_PREFLIGHT_PASS")
            self.assertIs(report["activation_authorized"], False)
            self.assertIs(report["flight_approval"], False)

    def test_missing_evidence_fails_closed(self):
        self.assertEqual(preflight.assess("MFD", {})["status"], "BLOCKED")

    def test_historical_and_active_power_faults_block(self):
        for value in [None, "throttled=0x50000", "throttled=0x1", "garbage"]:
            data = self.good()
            data["throttled"] = value
            self.assertIn("power_history_clear", preflight.assess("MFD", data)["blocking_checks"])

    def test_wrong_identity_current_and_missing_privilege_block(self):
        for field, value in [("host", "wrong"), ("arch", "x86_64"), ("model", "Raspberry Pi 50 Model B"),
                             ("current", "/tmp/other"), ("sudo_available", False)]:
            data = self.good()
            data[field] = value
            self.assertEqual(preflight.assess("MFD", data)["status"], "BLOCKED")

    def test_pfd_needs_both_services_and_manual_canary(self):
        data = self.good("PFD")
        self.assertIn("MFD canary accepted", preflight.assess("PFD", data)["remaining_manual_gates"])
        del data["services"]["efis-recorder.service"]
        self.assertIn("services_active", preflight.assess("PFD", data)["blocking_checks"])

    def test_bad_service_pid_is_not_a_running_service(self):
        for pid in ["0", "", 123, None]:
            data = self.good()
            data["services"]["efis-next-mfd-direct.service"]["MainPID"] = pid
            self.assertIn("services_active", preflight.assess("MFD", data)["blocking_checks"])

    def fixture(self, root):
        spec = copy.deepcopy(preflight.NODES["PFD"])
        manifest = {"schema": "efis-next.release-manifest.v1", "repository": "sfuso68/efis-next-professional",
                    "release_id": "pfdrec-test-v1", "source_commit": preflight.SOURCE,
                    "built_utc": "2026-10-19T00:00:00Z", "platform": "RPI5_AARCH64",
                    "roles": ["PFD", "RECORDER"], "flight_approval": False, "simulation_mode": True,
                    "artifacts": [{"path": "bin/example", "sha256": hashlib.sha256(b"fixture").hexdigest(),
                                   "executable": False}]}
        (root / "bin").mkdir()
        (root / "bin/example").write_bytes(b"fixture")
        self.save(root, manifest, spec)
        return manifest, spec

    def save(self, root, manifest, spec):
        raw = json.dumps(manifest).encode()
        (root / "release-manifest.json").write_bytes(raw)
        spec["digest"] = hashlib.sha256(raw).hexdigest()

    def test_real_validator_and_artifact_hash_are_used(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, spec = self.fixture(root)
            self.assertTrue(preflight.check_release(root, spec, candidate=True)["pass"])
            (root / "bin/example").write_bytes(b"corrupt")
            self.assertFalse(preflight.check_release(root, spec, candidate=True)["pass"])

    def test_incomplete_dual_role_manifest_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest, spec = self.fixture(root)
            manifest["roles"] = ["RECORDER"]
            self.save(root, manifest, spec)
            self.assertFalse(preflight.check_release(root, spec, candidate=True)["pass"])

    def test_source_or_safety_marker_change_rejected(self):
        for key, value in [("source_commit", "f" * 40), ("flight_approval", True), ("simulation_mode", False)]:
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                manifest, spec = self.fixture(root)
                manifest[key] = value
                self.save(root, manifest, spec)
                self.assertFalse(preflight.check_release(root, spec, candidate=True)["pass"])

    def test_changed_manifest_digest_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, spec = self.fixture(root)
            spec["digest"] = "0" * 64
            self.assertFalse(preflight.check_release(root, spec, candidate=True)["pass"])

    def test_missing_or_malformed_manifest_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertFalse(preflight.check_release(root, preflight.NODES["MFD"], candidate=True)["pass"])
            (root / "release-manifest.json").write_text("[]")
            self.assertFalse(preflight.check_release(root, preflight.NODES["MFD"], candidate=True)["pass"])

    def test_command_timeout_returns_unavailable(self):
        with patch.object(preflight.subprocess, "run", side_effect=OSError("not installed")):
            self.assertIsNone(preflight.command(["vcgencmd", "get_throttled"]))

    def test_process_fallback_only_reads_one_valid_pid_link(self):
        with patch.object(preflight.os, "readlink", side_effect=PermissionError()), \
                patch.object(preflight, "command", return_value="/opt/release/bin/broker") as run:
            self.assertEqual(preflight.process_executable("123"), "/opt/release/bin/broker")
            run.assert_called_once_with(["sudo", "-n", "readlink", "-f", "/proc/123/exe"])
        with patch.object(preflight, "command") as run:
            self.assertIsNone(preflight.process_executable("123;restart"))
            run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
