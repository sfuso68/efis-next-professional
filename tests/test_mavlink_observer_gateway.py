from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "validate_mavlink_observer_gateway.py"
SPEC = importlib.util.spec_from_file_location("validate_mavlink_observer_gateway", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class MavlinkObserverGatewayPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.policy = json.loads((
            ROOT / "config" / "network" / "mavlink-observer-gateway-v1.json"
        ).read_text(encoding="utf-8"))

    def test_repository_policy_is_safe(self) -> None:
        report = MODULE.validate(self.policy)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["observers"], 2)
        self.assertFalse(report["return_path"])

    def test_ip_forwarding_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.policy)
        invalid["controls"]["ip_forward_required"] = True
        with self.assertRaises(MODULE.PolicyError):
            MODULE.validate(invalid)

    def test_command_path_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.policy)
        invalid["controls"]["mavlink_command_forwarding_allowed"] = True
        with self.assertRaises(MODULE.PolicyError):
            MODULE.validate(invalid)

    def test_observer_on_flight_data_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.policy)
        invalid["observers"][0]["address"] = "10.77.10.50"
        with self.assertRaises(MODULE.PolicyError):
            MODULE.validate(invalid)

    def test_duplicate_observer_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.policy)
        invalid["observers"][1]["address"] = invalid["observers"][0]["address"]
        with self.assertRaises(MODULE.PolicyError):
            MODULE.validate(invalid)


if __name__ == "__main__":
    unittest.main()
