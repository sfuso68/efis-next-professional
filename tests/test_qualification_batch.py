from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/qualification/validate_qualification_batch.py"
SPEC = importlib.util.spec_from_file_location("validate_qualification_batch", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class QualificationBatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = json.loads(
            (ROOT / "config/qualification/avionix-qualification-batch-v1.json").read_text(encoding="utf-8")
        )

    def validate(self, contract: dict | None = None) -> dict:
        return MODULE.validate(copy.deepcopy(contract if contract is not None else self.contract), ROOT)

    def test_repository_batch_passes_five_packages_and_fifty_controls(self) -> None:
        report = self.validate()
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["execution_status"], "NOT_RUN")
        self.assertEqual(report["packages"], 5)
        self.assertEqual(report["controls_total"], 50)
        self.assertFalse(report["execution_authorized"])
        self.assertFalse(report["flight_approval"])

    def test_live_execution_and_flight_cannot_be_authorized(self) -> None:
        for key in ("execution_authorized", "flight_approval"):
            with self.subTest(key=key):
                invalid = copy.deepcopy(self.contract)
                invalid[key] = True
                with self.assertRaisesRegex(MODULE.ContractError, "QB-ROOT-02"):
                    self.validate(invalid)

    def test_package_order_and_all_control_ids_are_frozen(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][0], invalid["packages"][1] = invalid["packages"][1], invalid["packages"][0]
        with self.assertRaisesRegex(MODULE.ContractError, "QB-G1-00"):
            self.validate(invalid)

        for package_index, package in enumerate(self.contract["packages"]):
            for control_index, control in enumerate(package["controls"]):
                with self.subTest(control=control["id"]):
                    invalid = copy.deepcopy(self.contract)
                    invalid["packages"][package_index]["controls"][control_index]["id"] = "RENAMED"
                    with self.assertRaises(MODULE.ContractError):
                        self.validate(invalid)

    def test_asso5_values_cannot_be_invented(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][0]["invariants"]["physical_channels"] = "ASSIGNED"
        with self.assertRaisesRegex(MODULE.ContractError, "QB-G1-03"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][0]["invariants"]["threshold_invention_allowed"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "QB-G1-03"):
            self.validate(invalid)

    def test_vertical_transport_and_replay_cannot_be_weakened(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][1]["invariants"]["source_to_blackbox"] = "UDP_UNICAST"
        with self.assertRaisesRegex(MODULE.ContractError, "QB-G2-01"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][1]["invariants"]["cross_consumer_outcome_match_required"] = False
        with self.assertRaisesRegex(MODULE.ContractError, "QB-G2-01"):
            self.validate(invalid)

    def test_airdata_cannot_write_or_substitute_ground_speed(self) -> None:
        for key in ("parameter_write_allowed", "output_activation_allowed", "ground_speed_as_ias_allowed"):
            with self.subTest(key=key):
                invalid = copy.deepcopy(self.contract)
                invalid["packages"][2]["invariants"][key] = True
                with self.assertRaisesRegex(MODULE.ContractError, "QB-G3-01"):
                    self.validate(invalid)

    def test_blackbox_duration_and_fault_isolation_cannot_be_weakened(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][3]["invariants"]["minimum_endurance_seconds"] = 60
        with self.assertRaisesRegex(MODULE.ContractError, "QB-G4-02"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][3]["invariants"]["production_destructive_fault_allowed"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "QB-G4-02"):
            self.validate(invalid)

    def test_taws_missing_data_cannot_be_reported_safe(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][4]["invariants"]["missing_terrain_policy"] = "TERRAIN_SAFE"
        with self.assertRaisesRegex(MODULE.ContractError, "QB-G5-03"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][4]["invariants"]["command_or_actuation_authority"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "QB-G5-03"):
            self.validate(invalid)

    def test_authority_paths_are_confined_to_repository(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["authority"]["project_plan"] = "../outside.md"
        with self.assertRaisesRegex(MODULE.ContractError, "QB-ROOT-04"):
            self.validate(invalid)

    def test_each_package_must_finish_with_no_go(self) -> None:
        for index, package in enumerate(self.contract["packages"]):
            with self.subTest(package=package["id"]):
                invalid = copy.deepcopy(self.contract)
                invalid["packages"][index]["controls"][-1]["state"] = "DEFINED"
                with self.assertRaises(MODULE.ContractError):
                    self.validate(invalid)


if __name__ == "__main__":
    unittest.main()
