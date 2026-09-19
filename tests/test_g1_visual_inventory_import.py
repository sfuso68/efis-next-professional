from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/hardware/import_g1_visual_inventory.py"
SPEC = importlib.util.spec_from_file_location("import_g1_visual_inventory", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class G1VisualInventoryImportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.template = json.loads((ROOT / "config/hardware/avionix-g1-visual-intake-v1.json").read_text(encoding="utf-8"))
        cls.manifest = json.loads((ROOT / "config/hardware/avionix-g1-physical-inventory-v1.json").read_text(encoding="utf-8"))

    def complete_intake(self) -> dict:
        value = copy.deepcopy(self.template)
        value["operator"] = "TEST-OPERATOR"
        value["captured_utc"] = "2026-09-19T00:00:00Z"
        for board in value["boards"]:
            board["revision_observed"] = "TEST-REV"
            board["dip_visual_state"] = "TEST-VISUAL"
            board["connector_labels"] = ["TEST-CONNECTOR"]
        value["power"].update(
            supply_model="TEST-SUPPLY",
            rated_voltage="5 V",
            rated_current="5 A",
            single_source_confirmed=True,
        )
        return value

    @staticmethod
    def write_images(root: Path, intake: dict) -> None:
        jpeg = b"\xff\xd8\xff\xe0TEST-JPEG"
        for board in intake["boards"]:
            (root / board["front_file"]).write_bytes(jpeg)
            (root / board["back_file"]).write_bytes(jpeg)
        (root / intake["power"]["label_file"]).write_bytes(jpeg)

    def test_complete_preview_builds_no_go_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            intake = self.complete_intake()
            self.write_images(root, intake)
            result, artifacts = MODULE.prepare(intake, self.manifest, root)
        self.assertEqual(len(artifacts), 9)
        self.assertEqual(result["status"], "CAPTURED_VISUAL_PENDING_ENGINEERING")
        self.assertEqual(result["decision"], "NO_GO_PENDING_ENGINEERING_REVIEW")
        self.assertFalse(result["flight_approval"])
        self.assertTrue(all(board["verification"] == "VERIFIED_VISUAL" for board in result["boards"]))

    def test_apply_is_idempotent_for_identical_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            evidence = root / "evidence"
            source.mkdir()
            intake = self.complete_intake()
            self.write_images(source, intake)
            result, artifacts = MODULE.prepare(intake, self.manifest, source)
            manifest_path = root / "manifest.json"
            MODULE.apply_import(result, artifacts, evidence, manifest_path)
            MODULE.apply_import(result, artifacts, evidence, manifest_path)
            written = json.loads(manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(written["decision"], "NO_GO_PENDING_ENGINEERING_REVIEW")

    def test_incomplete_metadata_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            intake = self.complete_intake()
            intake["boards"][0]["revision_observed"] = None
            with self.assertRaisesRegex(MODULE.IntakeError, "revision_observed"):
                MODULE.prepare(intake, self.manifest, Path(directory))

    def test_invalid_utc_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            intake = self.complete_intake()
            intake["captured_utc"] = "2026-09-19 local"
            with self.assertRaisesRegex(MODULE.IntakeError, "valid ISO-8601 UTC"):
                MODULE.prepare(intake, self.manifest, Path(directory))

    def test_completed_manifest_cannot_be_reimported(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            intake = self.complete_intake()
            manifest = copy.deepcopy(self.manifest)
            manifest["status"] = "CAPTURED_VISUAL_PENDING_ENGINEERING"
            with self.assertRaisesRegex(MODULE.IntakeError, "unpopulated NO-GO template"):
                MODULE.prepare(intake, manifest, Path(directory))

    def test_wrong_model_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            intake = self.complete_intake()
            intake["boards"][0]["model_observed"] = "SM-I-011"
            with self.assertRaisesRegex(MODULE.IntakeError, "model mismatch"):
                MODULE.prepare(intake, self.manifest, Path(directory))

    def test_weakened_safety_declaration_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            intake = self.complete_intake()
            intake["capture_declaration"]["dip_changed"] = True
            with self.assertRaisesRegex(MODULE.IntakeError, "safety declaration"):
                MODULE.prepare(intake, self.manifest, Path(directory))

    def test_path_escape_and_non_image_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            intake = self.complete_intake()
            intake["boards"][0]["front_file"] = "../front.jpg"
            with self.assertRaisesRegex(MODULE.IntakeError, "confined"):
                MODULE.prepare(intake, self.manifest, root)
            intake = self.complete_intake()
            self.write_images(root, intake)
            (root / intake["boards"][0]["front_file"]).write_text("not an image", encoding="utf-8")
            with self.assertRaisesRegex(MODULE.IntakeError, "content is not"):
                MODULE.prepare(intake, self.manifest, root)

    def test_overwrite_with_different_artifact_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            evidence = root / "evidence"
            source.mkdir()
            evidence.mkdir()
            intake = self.complete_intake()
            self.write_images(source, intake)
            result, artifacts = MODULE.prepare(intake, self.manifest, source)
            (evidence / artifacts[0][1]).write_bytes(b"different")
            with self.assertRaisesRegex(MODULE.IntakeError, "refusing to overwrite"):
                MODULE.apply_import(result, artifacts, evidence, root / "manifest.json")


if __name__ == "__main__":
    unittest.main()
