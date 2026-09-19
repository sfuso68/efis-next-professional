from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/hardware/validate_g1_sensor_node_discovery.py"
SPEC = importlib.util.spec_from_file_location("validate_g1_sensor_node_discovery", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class G1SensorNodeDiscoveryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = json.loads(
            (ROOT / "config/hardware/avionix-g1-sensor-node-discovery-v1.json")
            .read_text(encoding="utf-8")
        )

    def validate(self, contract: dict | None = None) -> dict:
        return MODULE.validate(copy.deepcopy(contract if contract is not None else self.contract))

    def test_repository_contract_passes_twenty_controls_without_hardware_claim(self) -> None:
        report = self.validate()
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["execution_status"], "NOT_RUN")
        self.assertFalse(report["execution_authorized"])
        self.assertEqual(report["controls_passed"], 20)
        self.assertEqual(report["expected_boards"], 4)
        self.assertEqual(report["verified_boards"], 0)
        self.assertFalse(report["flight_approval"])

    def test_each_control_entry_is_fail_closed(self) -> None:
        for index, expected in enumerate(MODULE.expected_controls()):
            with self.subTest(control=expected["id"]):
                invalid = copy.deepcopy(self.contract)
                invalid["controls"][index]["state"] = "PASS"
                with self.assertRaisesRegex(MODULE.ContractError, "G1-D20-01"):
                    self.validate(invalid)

    def test_unknown_property_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["silent_override"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "G1-D20-01"):
            self.validate(invalid)

    def test_execution_or_flight_cannot_be_authorized(self) -> None:
        for key in ("execution_authorized", "flight_approval"):
            with self.subTest(key=key):
                invalid = copy.deepcopy(self.contract)
                invalid[key] = True
                with self.assertRaisesRegex(MODULE.ContractError, "G1-D20-02"):
                    self.validate(invalid)

    def test_node_identity_cannot_be_invented(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["node"]["hostname"] = "asso5-sensor"
        invalid["node"]["address"] = "10.41.10.10"
        with self.assertRaisesRegex(MODULE.ContractError, "G1-D20-04"):
            self.validate(invalid)

    def test_i2c_probe_or_vendor_read_cannot_be_enabled(self) -> None:
        for key in ("address_probe_allowed", "generic_scan_allowed", "vendor_read_allowed", "write_allowed"):
            with self.subTest(key=key):
                invalid = copy.deepcopy(self.contract)
                invalid["i2c_policy"][key] = True
                with self.assertRaisesRegex(MODULE.ContractError, "G1-D20-07"):
                    self.validate(invalid)

    def test_confirmed_board_quantities_cannot_drift(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["expected_inventory"]["boards"][0]["quantity"] = 2
        with self.assertRaisesRegex(MODULE.ContractError, "G1-D20-09"):
            self.validate(invalid)

    def test_forbidden_operations_cannot_be_removed(self) -> None:
        for operation in MODULE.FORBIDDEN:
            with self.subTest(operation=operation):
                invalid = copy.deepcopy(self.contract)
                invalid["forbidden_operations"].remove(operation)
                with self.assertRaisesRegex(MODULE.ContractError, "G1-D20-14"):
                    self.validate(invalid)

    def test_adapter_transport_cannot_be_promoted(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["adapter_boundary"]["real_adapter_status"] = "READY"
        invalid["adapter_boundary"]["publisher_transport"] = "UDP_UNICAST"
        with self.assertRaisesRegex(MODULE.ContractError, "G1-D20-16"):
            self.validate(invalid)

    def test_live_publisher_and_soak_remain_no_go(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["go_no_go"]["live_publisher_allowed"] = True
        invalid["go_no_go"]["soak_allowed"] = True
        invalid["go_no_go"]["current_decision"] = "GO"
        with self.assertRaisesRegex(MODULE.ContractError, "G1-D20-20"):
            self.validate(invalid)


if __name__ == "__main__":
    unittest.main()
