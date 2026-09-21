import copy
import json
import tempfile
import unittest
from pathlib import Path

from tools.terrain.manage_synthetic_vision_session import (
    CONFIRMATION, ROLES, SessionError, finalize_session, init_session,
    load_session, verify_session,
)
from tools.terrain.validate_synthetic_vision_benchmark import (
    ARTIFACT_TYPES, CONTRACT, digest,
)


ROOT = Path(__file__).resolve().parents[1]
SOURCE_COMMIT = "b" * 40
OPERATOR = "Bench Operator"


class SyntheticVisionSessionTests(unittest.TestCase):
    def _complete_role(self, bundle: Path, role: str) -> None:
        results_path = bundle / role.lower() / "results.json"
        results = json.loads(results_path.read_text(encoding="utf-8"))
        artifact_root = bundle / role.lower() / "artifacts"
        artifacts = {}
        for kind in sorted(ARTIFACT_TYPES):
            path = artifact_root / f"{kind.lower()}.txt"
            path.write_text(f"{role} observed evidence for {kind}\n", encoding="utf-8")
            artifacts[kind] = {"path": path.name, "sha256": digest(path)}
        contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        results.update({
            "release_id": f"{role.lower()}-bench-release-v1",
            "status": "PASS",
            "duration_seconds": 3600,
            "modes": ["DAY", "NIGHT"],
            "measurements": {
                "minimum_fps": 41.0,
                "p95_frame_ms": 24.0,
                "maximum_frame_ms": 99.0,
                "maximum_temperature_c": 61.0,
                "throttled": "0x0",
                "boundary_discontinuity_m": 0.5,
                "missing_samples": 0,
                "unlabelled_fallbacks": 0,
                "finite_projection": True,
                "near_field_visual_pass": True,
                "horizon_continuity_pass": True,
                "tile_boundary_crossed": True,
                "missing_tile_fallback_explicit": True,
            },
            "checkpoints": [
                {"id": item["id"], "day_observed": True, "night_observed": True}
                for item in contract["checkpoints"]
            ],
            "artifacts": artifacts,
            "operator": OPERATOR,
        })
        results_path.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")

    def _prepared(self, parent: Path) -> Path:
        bundle = parent / "session"
        init_session(ROOT, bundle, SOURCE_COMMIT)
        return bundle

    def test_init_creates_closed_not_run_session_for_both_roles(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = self._prepared(Path(temporary))
            session, _, runs = load_session(bundle, ROOT)
            self.assertEqual(session["status"], "PREPARED_SW_NOT_RUN")
            self.assertFalse(session["flight_approval"])
            self.assertEqual([run[0]["role"] for run in runs], list(ROLES))
            self.assertTrue(all(run[0]["status"] == "NOT_RUN" for run in runs))
            self.assertFalse((bundle / "evidence.json").exists())

    def test_init_rejects_bad_commit_and_existing_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            with self.assertRaises(SessionError):
                init_session(ROOT, parent / "bad", "abc")
            bundle = self._prepared(parent)
            with self.assertRaises(SessionError):
                init_session(ROOT, bundle, SOURCE_COMMIT)

    def test_finalize_rejects_not_run_templates(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = self._prepared(Path(temporary))
            with self.assertRaisesRegex(SessionError, "incomplete"):
                finalize_session(ROOT, bundle, OPERATOR, CONFIRMATION)
            self.assertFalse((bundle / "evidence.json").exists())

    def test_finalize_requires_exact_confirmation_and_operator(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = self._prepared(Path(temporary))
            for role in ROLES:
                self._complete_role(bundle, role)
            with self.assertRaisesRegex(SessionError, "confirmation"):
                finalize_session(ROOT, bundle, OPERATOR, "YES")
            with self.assertRaisesRegex(SessionError, "operator"):
                finalize_session(ROOT, bundle, "Different Operator", CONFIRMATION)

    def test_complete_two_role_session_finalizes_once(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = self._prepared(Path(temporary))
            for role in ROLES:
                self._complete_role(bundle, role)
            evidence_path = finalize_session(ROOT, bundle, OPERATOR, CONFIRMATION)
            evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
            self.assertEqual(evidence["status"], "PASS")
            self.assertFalse(evidence["flight_approval"])
            self.assertEqual([item["role"] for item in evidence["roles"]], list(ROLES))
            self.assertTrue(all(len(item["results_sha256"]) == 64 for item in evidence["roles"]))
            self.assertEqual(verify_session(ROOT, bundle), evidence_path)
            with self.assertRaisesRegex(SessionError, "overwrite"):
                finalize_session(ROOT, bundle, OPERATOR, CONFIRMATION)

    def test_verify_rejects_post_finalize_result_tamper(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = self._prepared(Path(temporary))
            for role in ROLES:
                self._complete_role(bundle, role)
            finalize_session(ROOT, bundle, OPERATOR, CONFIRMATION)
            path = bundle / "mfd/results.json"
            results = json.loads(path.read_text(encoding="utf-8"))
            results["release_id"] = "tampered-release"
            path.write_text(json.dumps(results), encoding="utf-8")
            with self.assertRaisesRegex(SessionError, "digest"):
                verify_session(ROOT, bundle)

    def test_source_commit_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = self._prepared(Path(temporary))
            path = bundle / "pfd/results.json"
            results = json.loads(path.read_text(encoding="utf-8"))
            results["source_commit"] = "c" * 40
            path.write_text(json.dumps(results), encoding="utf-8")
            with self.assertRaisesRegex(SessionError, "source commit"):
                load_session(bundle, ROOT)

    def test_session_path_escape_and_contract_tamper_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = self._prepared(Path(temporary))
            session_path = bundle / "session.json"
            original = json.loads(session_path.read_text(encoding="utf-8"))
            escaped = copy.deepcopy(original)
            escaped["roles"][0]["results_file"] = "../escape.json"
            session_path.write_text(json.dumps(escaped), encoding="utf-8")
            with self.assertRaisesRegex(SessionError, "escapes"):
                load_session(bundle, ROOT)

            session_path.write_text(json.dumps(original), encoding="utf-8")
            (bundle / "benchmark-contract.json").write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(SessionError, "digest"):
                load_session(bundle, ROOT)


if __name__ == "__main__":
    unittest.main()
