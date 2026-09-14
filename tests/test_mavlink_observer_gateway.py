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

    def test_server_firewall_is_narrow_and_has_no_forward_or_nat_hook(self) -> None:
        firewall = (
            ROOT / "deploy" / "network" / "efis-mavlink-observer-server.nft"
        ).read_text(encoding="utf-8")
        normalized = " ".join(
            line.split("#", 1)[0].strip()
            for line in firewall.splitlines()
            if line.split("#", 1)[0].strip()
        )
        self.assertIn("table inet efis_mavlink_observer", normalized)
        self.assertIn("type filter hook input priority -10; policy accept;", normalized)
        self.assertIn("ip daddr 192.168.1.6 udp dport 14550", normalized)
        self.assertIn("ip saddr != 192.168.1.47 counter drop", normalized)
        self.assertNotIn("hook forward", normalized)
        self.assertNotIn("hook output", normalized)
        self.assertNotIn("type nat", normalized)


if __name__ == "__main__":
    unittest.main()
