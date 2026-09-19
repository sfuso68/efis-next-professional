from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/network/validate_g2_vertical_acceptance.py"
SPEC = importlib.util.spec_from_file_location("validate_g2_vertical_acceptance", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def load(path: str) -> dict:
    return json.loads((ROOT / path).read_text(encoding="utf-8"))


class G2VerticalAcceptanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = load("config/network/avionix-g2-vertical-acceptance-v1.json")
        cls.network = load("config/network/avionics-network-v1.json")
        cls.icd = load("config/protocols/avionix-data-icd-v1.json")

    def validate(self, contract: dict | None = None, network: dict | None = None,
                 icd: dict | None = None) -> dict:
        return MODULE.validate(
            copy.deepcopy(contract if contract is not None else self.contract),
            copy.deepcopy(network if network is not None else self.network),
            copy.deepcopy(icd if icd is not None else self.icd),
        )

    def test_repository_contract_passes_twenty_controls_without_running_hardware(self) -> None:
        report = self.validate()
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["execution_status"], "NOT_RUN")
        self.assertFalse(report["execution_authorized"])
        self.assertEqual(report["controls_passed"], 20)
        self.assertEqual(report["controls_total"], 20)
        self.assertFalse(report["flight_approval"])

    def test_each_control_registry_entry_is_fail_closed(self) -> None:
        for index, expected in enumerate(MODULE.expected_controls()):
            with self.subTest(control=expected["id"]):
                invalid = copy.deepcopy(self.contract)
                invalid["controls"][index]["state"] = "PASS"
                with self.assertRaisesRegex(MODULE.ContractError, expected["id"]):
                    self.validate(contract=invalid)

    def test_unknown_root_property_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["silent_override"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "G2-V20-01"):
            self.validate(contract=invalid)

        invalid = copy.deepcopy(self.contract)
        invalid["topology"]["silent_route"] = "enabled"
        with self.assertRaisesRegex(MODULE.ContractError, "G2-V20-04"):
            self.validate(contract=invalid)

        invalid = copy.deepcopy(self.contract)
        invalid["safety"]["maintenance_override"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "G2-V20-11"):
            self.validate(contract=invalid)

    def test_contract_cannot_authorize_live_execution_or_flight(self) -> None:
        for key in ("execution_authorized", "flight_approval"):
            with self.subTest(key=key):
                invalid = copy.deepcopy(self.contract)
                invalid[key] = True
                with self.assertRaisesRegex(MODULE.ContractError, "G2-V20-02"):
                    self.validate(contract=invalid)

    def test_source_transport_cannot_be_invented_before_g1(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["topology"]["source_to_blackbox"]["transport"] = "UDP_UNICAST"
        invalid["topology"]["source_to_blackbox"]["execution_allowed"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "G2-V20-05"):
            self.validate(contract=invalid)

    def test_network_plan_drift_is_rejected_for_each_display(self) -> None:
        cases = (
            ("blackbox-ddp-pfd", "G2-V20-07"),
            ("blackbox-ddp-mfd", "G2-V20-08"),
            ("blackbox-edp-ems", "G2-V20-09"),
        )
        for flow_id, control_id in cases:
            with self.subTest(flow=flow_id):
                invalid = copy.deepcopy(self.network)
                flow = next(item for item in invalid["flows"] if item["id"] == flow_id)
                flow["port"] += 100
                with self.assertRaisesRegex(MODULE.ContractError, control_id):
                    self.validate(network=invalid)

    def test_command_and_actuation_paths_cannot_be_added(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["safety"]["command_path"] = "MAVLINK"
        with self.assertRaisesRegex(MODULE.ContractError, "G2-V20-11"):
            self.validate(contract=invalid)

    def test_icd_freshness_drift_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.icd)
        invalid["freshness"]["lost_after_ms"] = 5000
        with self.assertRaisesRegex(MODULE.ContractError, "G2-V20-14"):
            self.validate(icd=invalid)

    def test_fault_matrix_cannot_be_reduced(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["mandatory_faults"].remove("storage_full")
        with self.assertRaisesRegex(MODULE.ContractError, "G2-V20-18"):
            self.validate(contract=invalid)

    def test_deployment_and_restart_remain_forbidden(self) -> None:
        invalid = copy.deepcopy(self.contract)
        invalid["rollback"]["deployment_allowed_by_this_contract"] = True
        invalid["rollback"]["service_restart_limit"] = 1
        with self.assertRaisesRegex(MODULE.ContractError, "G2-V20-20"):
            self.validate(contract=invalid)


if __name__ == "__main__":
    unittest.main()
