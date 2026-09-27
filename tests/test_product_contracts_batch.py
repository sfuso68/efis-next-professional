from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/product/validate_product_contracts_batch.py"
SPEC = importlib.util.spec_from_file_location("validate_product_contracts_batch", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ProductContractsBatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = json.loads(
            (ROOT / "config/product/avionix-product-contracts-batch-v1.json").read_text(encoding="utf-8")
        )

    def validate(self, contract: dict | None = None) -> dict:
        return MODULE.validate(copy.deepcopy(contract if contract is not None else self.contract), ROOT)

    def test_repository_contract_has_five_packages_and_fifty_controls(self) -> None:
        report = self.validate()
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["execution_status"], "NOT_RUN")
        self.assertEqual(report["packages"], 5)
        self.assertEqual(report["controls_total"], 50)
        self.assertFalse(report["execution_authorized"])
        self.assertFalse(report["flight_approval"])

    def test_batch_cannot_authorize_execution_or_flight(self) -> None:
        for key in ("execution_authorized", "flight_approval"):
            invalid = copy.deepcopy(self.contract)
            invalid[key] = True
            with self.subTest(key=key), self.assertRaisesRegex(MODULE.ContractError, "PB-ROOT-02"):
                self.validate(invalid)

    def test_ems_unverified_limits_cannot_drive_normal_cas(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][0]["invariants"]["unverified_limits_may_drive_normal_cas"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "PB-EMS-01"):
            self.validate(invalid)

    def test_sku_unknown_features_and_inflight_switch_fail_closed(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][1]["invariants"]["unknown_feature_policy"] = "ALLOW"
        with self.assertRaisesRegex(MODULE.ContractError, "PB-SKU-01"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][1]["invariants"]["profile_switch_in_flight_allowed"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "PB-SKU-01"):
            self.validate(invalid)

    def test_fpv_cannot_share_ems_host_or_bridge_networks(self) -> None:
        for key in ("ems_and_fpv_same_host_allowed", "video_and_flight_data_bridge_allowed", "rpi5_primary_decode_allowed"):
            invalid = copy.deepcopy(self.contract)
            invalid["packages"][2]["invariants"][key] = True
            with self.subTest(key=key), self.assertRaisesRegex(MODULE.ContractError, "PB-FPV-02"):
                self.validate(invalid)

    def test_voice_cannot_transmit_command_or_actuate(self) -> None:
        for key in ("free_form_command_execution", "radio_transmission_allowed", "pixhawk_command_allowed", "actuation_allowed"):
            invalid = copy.deepcopy(self.contract)
            invalid["packages"][3]["invariants"][key] = True
            with self.subTest(key=key), self.assertRaisesRegex(MODULE.ContractError, "PB-VOI-01"):
                self.validate(invalid)

    def test_hil_cannot_touch_production_or_actuators(self) -> None:
        for key in ("production_network_injection_allowed", "production_storage_fault_allowed", "actuator_output_allowed", "flight_control_command_allowed"):
            invalid = copy.deepcopy(self.contract)
            invalid["packages"][4]["invariants"][key] = True
            with self.subTest(key=key), self.assertRaisesRegex(MODULE.ContractError, "PB-HIL-01"):
                self.validate(invalid)

    def test_package_order_control_count_and_final_no_go_are_frozen(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["packages"][0], invalid["packages"][1] = invalid["packages"][1], invalid["packages"][0]
        with self.assertRaises(MODULE.ContractError):
            self.validate(invalid)
        for index, package in enumerate(self.contract["packages"]):
            invalid = copy.deepcopy(self.contract)
            invalid["packages"][index]["controls"][-1]["state"] = "DEFINED"
            with self.subTest(package=package["id"]), self.assertRaises(MODULE.ContractError):
                self.validate(invalid)

    def test_authority_paths_are_confined(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["authority"]["master_matrix"] = "../outside.md"
        with self.assertRaisesRegex(MODULE.ContractError, "PB-ROOT-04"):
            self.validate(invalid)


if __name__ == "__main__":
    unittest.main()
