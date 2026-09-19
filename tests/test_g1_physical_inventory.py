from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/hardware/validate_g1_physical_inventory.py"
SPEC = importlib.util.spec_from_file_location("validate_g1_physical_inventory", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class G1PhysicalInventoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = json.loads(
            (ROOT / "config/hardware/avionix-g1-physical-inventory-v1.json")
            .read_text(encoding="utf-8")
        )

    def validate(self, manifest: dict | None = None) -> dict:
        return MODULE.validate(copy.deepcopy(manifest if manifest is not None else self.manifest))

    def complete_manifest(self) -> dict:
        value = copy.deepcopy(self.manifest)
        digest = "a" * 64
        for board in value["boards"]:
            slug = board["slot"].lower()
            board.update(
                model_observed=board["expected_model"],
                revision_observed="TEST-REV",
                serial_observed=None,
                front_photo={"path": f"docs/status/evidence/g1-physical-inventory/{slug}-front.jpg", "sha256": digest},
                back_photo={"path": f"docs/status/evidence/g1-physical-inventory/{slug}-back.jpg", "sha256": digest},
                dip_visual_state="TEST-VISUAL-STATE",
                connector_labels=["TEST-CONNECTOR"],
                verification="VERIFIED_VISUAL",
            )
        value["power"].update(
            supply_model="TEST-SUPPLY", rated_voltage="5 V", rated_current="5 A",
            label_photo={"path": "docs/status/evidence/g1-physical-inventory/power-label.jpg", "sha256": digest},
            single_source_confirmed=True, verification="VERIFIED_VISUAL",
        )
        value["operator_review"].update(
            operator="TEST-OPERATOR", captured_utc="2026-09-18T00:00:00Z", approved=True,
        )
        value["status"] = "CAPTURED_VISUAL_PENDING_ENGINEERING"
        value["decision"] = "NO_GO_PENDING_ENGINEERING_REVIEW"
        return value

    def test_repository_template_passes_without_hardware_claim(self) -> None:
        report = self.validate()
        self.assertEqual(report["controls_passed"], 10)
        self.assertEqual(report["boards_verified_visual"], 0)
        self.assertFalse(report["power_verified_visual"])
        self.assertEqual(report["decision"], "NO_GO_PENDING_PHYSICAL_BOARD_INVENTORY")
        self.assertFalse(report["flight_approval"])

    def test_complete_synthetic_visual_manifest_remains_no_go(self) -> None:
        report = self.validate(self.complete_manifest())
        self.assertEqual(report["boards_verified_visual"], 4)
        self.assertTrue(report["power_verified_visual"])
        self.assertEqual(report["decision"], "NO_GO_PENDING_ENGINEERING_REVIEW")
        self.assertFalse(report["flight_approval"])

    def test_each_control_entry_is_frozen(self) -> None:
        for index, control in enumerate(MODULE.expected_controls()):
            with self.subTest(control=control["id"]):
                invalid = copy.deepcopy(self.manifest)
                invalid["controls"][index]["state"] = "DONE"
                with self.assertRaisesRegex(MODULE.InventoryError, "G1-I10-01"):
                    self.validate(invalid)

    def test_wrong_slot_or_model_count_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.manifest)
        invalid["boards"][2]["expected_model"] = "SM-I-018"
        with self.assertRaisesRegex(MODULE.InventoryError, "G1-I10-03"):
            self.validate(invalid)

    def test_photo_escape_and_bad_digest_are_rejected(self) -> None:
        for photo in (
            {"path": "../secret.jpg", "sha256": "a" * 64},
            {"path": "docs/status/evidence/g1-physical-inventory/front.jpg", "sha256": "bad"},
        ):
            with self.subTest(photo=photo):
                invalid = copy.deepcopy(self.manifest)
                invalid["boards"][0]["front_photo"] = photo
                with self.assertRaisesRegex(MODULE.InventoryError, "G1-I10-04"):
                    self.validate(invalid)

    def test_probe_address_or_electrical_probe_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.manifest)
        invalid["boards"][0]["i2c_address_observed"] = "0x10"
        with self.assertRaisesRegex(MODULE.InventoryError, "G1-I10-09"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.manifest)
        invalid["boards"][0]["electrical_probe_performed"] = True
        with self.assertRaisesRegex(MODULE.InventoryError, "G1-I10-09"):
            self.validate(invalid)

    def test_capture_policy_cannot_be_weakened(self) -> None:
        for key in ("dip_change_allowed", "wiring_change_allowed", "address_probe_allowed", "vendor_read_allowed", "output_activation_allowed"):
            with self.subTest(key=key):
                invalid = copy.deepcopy(self.manifest)
                invalid["capture_policy"][key] = True
                with self.assertRaisesRegex(MODULE.InventoryError, "G1-I10-09"):
                    self.validate(invalid)

    def test_incomplete_inventory_cannot_be_promoted_or_approved(self) -> None:
        for mutation in (
            lambda value: value.update(status="CAPTURED_VISUAL_PENDING_ENGINEERING"),
            lambda value: value.update(decision="NO_GO_PENDING_ENGINEERING_REVIEW"),
            lambda value: value["operator_review"].update(approved=True),
            lambda value: value.update(flight_approval=True),
        ):
            invalid = copy.deepcopy(self.manifest)
            mutation(invalid)
            with self.assertRaisesRegex(MODULE.InventoryError, "G1-I10-10"):
                self.validate(invalid)


if __name__ == "__main__":
    unittest.main()
