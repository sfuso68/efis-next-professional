import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.ui.assemble_hmi_hardware_evidence import KINDS
from tools.ui.manage_hmi_hardware_session import (
    CONFIRMATION,
    SessionError,
    finalize_session,
    init_session,
    load_session,
)
from tools.ui.validate_hmi_hardware_acceptance import validate


ROOT = Path(__file__).resolve().parents[1]
SOURCE_COMMIT = "a" * 40


def complete_run(bundle: Path, role: str) -> dict:
    artifacts = {}
    for kind in KINDS:
        path = bundle / role.lower() / "artifacts" / f"{kind.lower()}.txt"
        path.write_text(f"synthetic test only {role}/{kind}\n", encoding="utf-8")
        artifacts[kind] = path.relative_to(bundle).as_posix()
    return {
        "role": role,
        "node": f"synthetic-{role.lower()}",
        "release": "b" * 40,
        "measurements": {
            "duration_minutes": 60,
            "minimum_fps": 40,
            "p95_frame_time_ms": 25.0,
            "maximum_temperature_c": 55.0,
            "throttled": "0x0",
            "cold_boot_pass": True,
            "reboot_pass": True,
            "scenarios_passed": ["LIVE", "STALE", "DEGRADED", "LOST", "FAIL", "SIM"],
            "lighting_modes_passed": ["DAY", "NIGHT"],
            "minimum_touch_px": 44,
            "transport_replay_pass": True,
            "ethernet_fault_recovery_pass": True,
        },
        "artifacts": artifacts,
    }


class ManageHmiHardwareSessionTests(unittest.TestCase):
    def test_init_is_not_run_and_never_overwrites(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary) / "bundle"
            init_session(ROOT, bundle, SOURCE_COMMIT)
            session, runs = load_session(bundle)
            self.assertEqual(session["status"], "PREPARED_SW_NOT_RUN")
            self.assertFalse(session["flight_approval"])
            self.assertEqual([run["role"] for run in runs], ["PFD", "MFD", "EMS"])
            results = json.loads((bundle / "results.json").read_text(encoding="utf-8"))
            self.assertEqual(results["status"], "NOT_RUN")
            self.assertFalse(results["operator_confirmed"])
            with self.assertRaisesRegex(SessionError, "already exists"):
                init_session(ROOT, bundle, SOURCE_COMMIT)

    def test_tampered_plan_and_escaping_run_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary) / "bundle"
            init_session(ROOT, bundle, SOURCE_COMMIT)
            (bundle / "acceptance-plan.json").write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(SessionError, "digest changed"):
                load_session(bundle)

            bundle2 = Path(temporary) / "bundle2"
            init_session(ROOT, bundle2, SOURCE_COMMIT)
            session_path = bundle2 / "session.json"
            session = json.loads(session_path.read_text(encoding="utf-8"))
            session["roles"][0]["run_file"] = "../outside.json"
            session_path.write_text(json.dumps(session), encoding="utf-8")
            with self.assertRaisesRegex(SessionError, "escapes"):
                load_session(bundle2)

    def test_forged_plan_digest_does_not_replace_canonical_plan(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary) / "bundle"
            init_session(ROOT, bundle, SOURCE_COMMIT)
            plan = bundle / "acceptance-plan.json"
            plan.write_text("{}\n", encoding="utf-8")
            session_path = bundle / "session.json"
            session = json.loads(session_path.read_text(encoding="utf-8"))
            session["plan_sha256"] = hashlib.sha256(plan.read_bytes()).hexdigest()
            session_path.write_text(json.dumps(session), encoding="utf-8")
            with self.assertRaisesRegex(SessionError, "canonical plan"):
                finalize_session(ROOT, bundle, "operator", CONFIRMATION)

    def test_incomplete_or_unconfirmed_session_cannot_be_finalized(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary) / "bundle"
            init_session(ROOT, bundle, SOURCE_COMMIT)
            with self.assertRaisesRegex(SessionError, "explicit real-hardware"):
                finalize_session(ROOT, bundle, "operator", "NO")
            with self.assertRaisesRegex(SessionError, "incomplete"):
                finalize_session(ROOT, bundle, "operator", CONFIRMATION)
            self.assertFalse((bundle / "results-ready.json").exists())
            self.assertFalse((bundle / "evidence.json").exists())

    def test_complete_synthetic_session_exercises_pipeline_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary) / "bundle"
            init_session(ROOT, bundle, SOURCE_COMMIT)
            for role in ("PFD", "MFD", "EMS"):
                path = bundle / role.lower() / "run.json"
                path.write_text(
                    json.dumps(complete_run(bundle, role), indent=2) + "\n",
                    encoding="utf-8")
            evidence = finalize_session(
                ROOT, bundle, "synthetic-test-only", CONFIRMATION)
            self.assertTrue(evidence.is_file())
            self.assertEqual(validate(ROOT, evidence), [])
            manifest = json.loads(evidence.read_text(encoding="utf-8"))
            self.assertFalse(manifest["flight_approval"])
            self.assertEqual(manifest["status"], "PASS")
            with self.assertRaisesRegex(SessionError, "overwrite"):
                finalize_session(ROOT, bundle, "synthetic-test-only", CONFIRMATION)


if __name__ == "__main__":
    unittest.main()
