import csv
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.terrain.assemble_synthetic_vision_run import (
    AssemblyError, FILES, assemble_role,
)
from tools.terrain.manage_synthetic_vision_session import (
    CONFIRMATION, SessionError, init_session,
)
from tools.terrain.validate_synthetic_vision_benchmark import (
    CONTRACT, digest, validate_results,
)


ROOT = Path(__file__).resolve().parents[1]
SOURCE_COMMIT = "d" * 40
RELEASE_ID = "e" * 40
OPERATOR = "Hardware Observer"


class SyntheticVisionRunAssemblerTests(unittest.TestCase):
    def _write_csv(self, path: Path, header: list[str], rows: list[list[object]]) -> None:
        with path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(header)
            writer.writerows(rows)

    def _prepared(self, parent: Path) -> tuple[Path, Path]:
        bundle = parent / "session"
        init_session(ROOT, bundle, SOURCE_COMMIT)
        artifacts = bundle / "pfd" / "artifacts"
        self._write_csv(
            artifacts / FILES["FRAME_TIMING"],
            ["elapsed_second", "frames", "p95_frame_ms", "maximum_frame_ms"],
            [[second, 41, 20.0, 24.0] for second in range(3601)])
        self._write_csv(
            artifacts / FILES["THERMAL"],
            ["elapsed_second", "temperature_c", "throttled"],
            [[second, 55.0, "0x0"] for second in range(3601)])
        (artifacts / FILES["TILE_TRANSITION"]).write_text(json.dumps({
            "schema": "avionix.synthetic-vision-tile-transition.v1",
            "boundary": "N45E012_TO_N46E012",
            "boundary_discontinuity_m": 0.5,
            "missing_samples": 0,
            "finite_projection": True,
            "horizon_continuity_pass": True,
            "tile_boundary_crossed": True,
        }), encoding="utf-8")
        (artifacts / FILES["MISSING_TILE_FAULT"]).write_text(json.dumps({
            "schema": "avionix.synthetic-vision-missing-tile-fault.v1",
            "unlabelled_fallbacks": 0,
            "missing_tile_fallback_explicit": True,
            "recovery_observed": True,
        }), encoding="utf-8")
        contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        screenshots = []
        for checkpoint in contract["checkpoints"]:
            for mode in ("DAY", "NIGHT"):
                name = f"screenshots/{checkpoint['id'].lower()}-{mode.lower()}.png"
                target = artifacts / name
                target.parent.mkdir(exist_ok=True)
                target.write_bytes(f"fixture {checkpoint['id']} {mode}\n".encode())
                screenshots.append({
                    "checkpoint": checkpoint["id"], "mode": mode,
                    "file": name, "sha256": digest(target),
                })
        (artifacts / FILES["CHECKPOINT_SCREENSHOTS"]).write_text(json.dumps({
            "schema": "avionix.synthetic-vision-screenshots.v1",
            "screenshots": screenshots,
        }), encoding="utf-8")
        (artifacts / FILES["OPERATOR_SIGNOFF"]).write_text(json.dumps({
            "schema": "avionix.synthetic-vision-operator-signoff.v1",
            "operator": OPERATOR,
            "signed_utc": "2026-09-21T12:00:00Z",
            "confirmation": CONFIRMATION,
            "near_field_visual_pass": True,
            "horizon_continuity_pass": True,
            "notes": "Synthetic fixture; no hardware claim.",
        }), encoding="utf-8")
        return bundle, artifacts

    def _assemble(self, bundle: Path) -> Path:
        return assemble_role(
            ROOT, bundle, "PFD", RELEASE_ID, OPERATOR, CONFIRMATION)

    def test_complete_capture_is_derived_validated_and_immutable(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle, artifacts = self._prepared(Path(temporary))
            result_path = self._assemble(bundle)
            result = json.loads(result_path.read_text(encoding="utf-8"))
            contract = json.loads((bundle / "benchmark-contract.json").read_text(encoding="utf-8"))
            self.assertEqual(validate_results(
                result, contract, artifacts, result["contract_sha256"]), [])
            self.assertEqual(result["measurements"]["minimum_fps"], 41)
            self.assertEqual(result["measurements"]["p95_frame_ms"], 20.0)
            self.assertEqual(result["measurements"]["maximum_temperature_c"], 55.0)
            self.assertFalse(result["flight_approval"])
            screenshot_bundle = artifacts / result["artifacts"]["CHECKPOINT_SCREENSHOTS"]["path"]
            with zipfile.ZipFile(screenshot_bundle) as archive:
                self.assertEqual(len(archive.namelist()), 11)
                self.assertIn("checkpoint-screenshots.json", archive.namelist())
            with self.assertRaisesRegex(AssemblyError, "overwrite"):
                self._assemble(bundle)

    def test_identity_and_confirmation_are_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle, _ = self._prepared(Path(temporary))
            for role, release, operator, confirmation in (
                ("EMS", RELEASE_ID, OPERATOR, CONFIRMATION),
                ("PFD", "short", OPERATOR, CONFIRMATION),
                ("PFD", RELEASE_ID, "", CONFIRMATION),
                ("PFD", RELEASE_ID, OPERATOR, "YES"),
            ):
                with self.subTest(role=role, release=release, operator=operator):
                    with self.assertRaises(AssemblyError):
                        assemble_role(ROOT, bundle, role, release, operator, confirmation)

    def test_ten_capture_integrity_and_acceptance_controls_fail_closed(self):
        def mutate(control: int, bundle: Path, artifacts: Path) -> None:
            if control == 1:
                path = artifacts / FILES["FRAME_TIMING"]
                text = path.read_text(encoding="utf-8").replace("elapsed_second", "second", 1)
                path.write_text(text, encoding="utf-8")
            elif control == 2:
                path = artifacts / FILES["FRAME_TIMING"]
                lines = path.read_text(encoding="utf-8").splitlines()
                path.write_text("\n".join(lines[:100] + lines[101:]) + "\n", encoding="utf-8")
            elif control == 3:
                path = artifacts / FILES["FRAME_TIMING"]
                lines = path.read_text(encoding="utf-8").splitlines()
                lines[2] = lines[2].replace(",41,", ",39,")
                path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            elif control == 4:
                path = artifacts / FILES["THERMAL"]
                lines = path.read_text(encoding="utf-8").splitlines()
                lines[2] = lines[2].replace("0x0", "0x50000")
                path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            elif control == 5:
                path = artifacts / FILES["TILE_TRANSITION"]
                data = json.loads(path.read_text(encoding="utf-8"))
                data["boundary_discontinuity_m"] = 1.1
                path.write_text(json.dumps(data), encoding="utf-8")
            elif control == 6:
                path = artifacts / FILES["MISSING_TILE_FAULT"]
                data = json.loads(path.read_text(encoding="utf-8"))
                data["recovery_observed"] = False
                path.write_text(json.dumps(data), encoding="utf-8")
            elif control == 7:
                path = artifacts / FILES["CHECKPOINT_SCREENSHOTS"]
                data = json.loads(path.read_text(encoding="utf-8"))
                data["screenshots"].pop()
                path.write_text(json.dumps(data), encoding="utf-8")
            elif control == 8:
                path = artifacts / FILES["CHECKPOINT_SCREENSHOTS"]
                data = json.loads(path.read_text(encoding="utf-8"))
                data["screenshots"][0]["file"] = "../escape.png"
                path.write_text(json.dumps(data), encoding="utf-8")
            elif control == 9:
                path = artifacts / FILES["OPERATOR_SIGNOFF"]
                data = json.loads(path.read_text(encoding="utf-8"))
                data["near_field_visual_pass"] = False
                path.write_text(json.dumps(data), encoding="utf-8")
            elif control == 10:
                path = bundle / "session.json"
                data = json.loads(path.read_text(encoding="utf-8"))
                data["source_commit"] = "f" * 40
                path.write_text(json.dumps(data), encoding="utf-8")

        for control in range(1, 11):
            with self.subTest(control=control), tempfile.TemporaryDirectory() as temporary:
                bundle, artifacts = self._prepared(Path(temporary))
                mutate(control, bundle, artifacts)
                with self.assertRaises((AssemblyError, SessionError)):
                    self._assemble(bundle)


if __name__ == "__main__":
    unittest.main()
