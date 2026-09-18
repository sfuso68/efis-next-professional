from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/hardware/validate_g1_g2_readiness.py"
SPEC = importlib.util.spec_from_file_location("validate_g1_g2_readiness", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class G1G2ReadinessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = json.loads(
            (ROOT / "config/hardware/avionix-g1-g2-readiness-v1.json")
            .read_text(encoding="utf-8")
        )

    def validate(self, contract: dict | None = None) -> dict:
        return MODULE.validate(copy.deepcopy(contract if contract is not None else self.contract))

    def test_repository_contract_validates_one_hundred_controls_and_stays_no_go(self) -> None:
        report = self.validate()
        self.assertEqual(report["controls_validated"], 100)
        self.assertEqual(report["verified_physical_boards"], 0)
        self.assertEqual(report["source_to_blackbox_transport"], "UNRESOLVED")
        self.assertFalse(report["execution_authorized"])
        self.assertFalse(report["flight_approval"])
        self.assertTrue(report["decision"].startswith("NO_GO"))

    def test_all_one_hundred_control_mutations_fail_closed(self) -> None:
        for domain_index, domain in enumerate(self.contract["domains"]):
            for control_index, control in enumerate(domain["controls"]):
                with self.subTest(control=control["id"]):
                    invalid = copy.deepcopy(self.contract)
                    invalid["domains"][domain_index]["controls"][control_index]["state"] = "PASS" if control["state"] != "PASS" else "PREPARED"
                    with self.assertRaisesRegex(MODULE.ReadinessError, "G1-R100-001"):
                        self.validate(invalid)

    def test_every_forbidden_capability_fails_closed(self) -> None:
        for key in self.contract["safety"]:
            with self.subTest(capability=key):
                invalid = copy.deepcopy(self.contract)
                invalid["safety"][key] = True
                with self.assertRaisesRegex(MODULE.ReadinessError, "G1-R100-007"):
                    self.validate(invalid)

    def test_hardware_and_architecture_cannot_be_silently_resolved(self) -> None:
        mutations = [
            lambda value: value["unresolved"].update(verified_physical_boards=4),
            lambda value: value["unresolved"].update(board_addresses=["0x10"]),
            lambda value: value["unresolved"].update(physical_channel_map={"oil": 1}),
            lambda value: value["unresolved"].update(calibration_coefficients=[1.0]),
            lambda value: value["unresolved"].update(real_source_id="sequent_real"),
            lambda value: value["unresolved"].update(source_to_blackbox_transport="UDP"),
            lambda value: value["unresolved"].update(live_execution="AUTHORIZED"),
        ]
        for mutation in mutations:
            invalid = copy.deepcopy(self.contract)
            mutation(invalid)
            with self.assertRaisesRegex(MODULE.ReadinessError, "G1-R100-008"):
                self.validate(invalid)

    def test_signal_count_name_unit_and_mapping_are_frozen(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["engine_signals"].pop()
        with self.assertRaisesRegex(MODULE.ReadinessError, "G1-R100-006"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.contract)
        invalid["engine_signals"][0]["unit"] = "Hz"
        with self.assertRaisesRegex(MODULE.ReadinessError, "G1-R100-006"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.contract)
        invalid["engine_signals"][0]["physical_channel"] = "A1"
        with self.assertRaisesRegex(MODULE.ReadinessError, "G1-R100-008"):
            self.validate(invalid)

    def test_transport_cannot_bypass_blackbox_or_become_bidirectional(self) -> None:
        mutations = [
            lambda value: value["transport"].update(normalization_authority="display"),
            lambda value: value["transport"].update(source_to_blackbox="UDP"),
            lambda value: value["transport"]["blackbox_to_ems"].update(direction="BIDIRECTIONAL"),
            lambda value: value["transport"].update(routing_bridge_nat_allowed=True),
        ]
        for mutation in mutations:
            invalid = copy.deepcopy(self.contract)
            mutation(invalid)
            with self.assertRaisesRegex(MODULE.ReadinessError, "G1-R100-005"):
                self.validate(invalid)

    def test_evidence_root_soak_and_replay_policy_are_frozen(self) -> None:
        mutations = [
            lambda value: value["evidence"].update(root="../escape"),
            lambda value: value["evidence"].update(sha256_required=False),
            lambda value: value["evidence"].update(minimum_soak_seconds=60),
            lambda value: value["evidence"].update(same_dataset_replay_required=False),
        ]
        for mutation in mutations:
            invalid = copy.deepcopy(self.contract)
            mutation(invalid)
            with self.assertRaisesRegex(MODULE.ReadinessError, "G1-R100-009"):
                self.validate(invalid)

    def test_execution_flight_and_decision_remain_fail_closed(self) -> None:
        mutations = [
            lambda value: value.update(execution_authorized=True),
            lambda value: value.update(flight_approval=True),
            lambda value: value.update(decision="GO"),
        ]
        for mutation in mutations:
            invalid = copy.deepcopy(self.contract)
            mutation(invalid)
            with self.assertRaises(MODULE.ReadinessError):
                self.validate(invalid)


if __name__ == "__main__":
    unittest.main()
