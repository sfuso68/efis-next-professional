from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "validate_avionics_network.py"
SPEC = importlib.util.spec_from_file_location("validate_avionics_network", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class AvionicsNetworkPlanTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.plan = json.loads(
            (ROOT / "config" / "network" / "avionics-network-v1.json").read_text(
                encoding="utf-8"
            )
        )

    def test_repository_plan_is_valid(self) -> None:
        report = MODULE.validate(self.plan)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["segments"], 2)
        self.assertEqual(report["nodes"], 11)
        self.assertEqual(report["switches"], 2)

    def test_duplicate_address_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.plan)
        invalid["nodes"][2]["addresses"]["flight-data"] = "10.77.10.2"
        with self.assertRaises(MODULE.PlanError):
            MODULE.validate(invalid)

    def test_camera_on_flight_data_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.plan)
        invalid["nodes"][-1]["addresses"]["flight-data"] = "10.77.10.99"
        with self.assertRaises(MODULE.PlanError):
            MODULE.validate(invalid)

    def test_gateway_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.plan)
        invalid["segments"][0]["default_gateway"] = "10.77.10.1"
        with self.assertRaises(MODULE.PlanError):
            MODULE.validate(invalid)

    def test_undersized_flight_switch_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.plan)
        invalid["switches"][0]["minimum_operational_ports"] = 5
        with self.assertRaises(MODULE.PlanError):
            MODULE.validate(invalid)


if __name__ == "__main__":
    unittest.main()
