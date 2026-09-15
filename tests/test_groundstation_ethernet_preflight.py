from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "groundstation_ethernet_preflight.py"
SPEC = importlib.util.spec_from_file_location("groundstation_ethernet_preflight", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def snapshot(*, carrier: bool, migrated: bool = False) -> dict[str, object]:
    address = "192.168.1.39"
    return {
        "expected_address": address,
        "ethernet": {
            "name": "eno1", "exists": True, "carrier": carrier,
            "operstate": "up" if carrier else "down",
            "ipv4": [address] if migrated else [],
        },
        "wifi": {
            "name": "wlo1", "exists": True, "carrier": True, "operstate": "up",
            "ipv4": [] if migrated else [address],
        },
        "route": {
            "reachable": True,
            "device": "eno1" if migrated else "wlo1",
            "source": address,
        },
        "qgc": {
            "active": True, "pid": 2660, "restarts": 0,
            "udp_14550_listeners": 1, "listener_is_qgc": True,
        },
    }


class GroundStationEthernetPreflightTests(unittest.TestCase):
    def test_physical_phase_blocks_without_carrier(self) -> None:
        result = MODULE.evaluate(snapshot(carrier=False), "physical")
        self.assertEqual(result["status"], "BLOCKED")
        self.assertEqual(result["failed_checks"], ["ethernet_carrier"])

    def test_physical_phase_passes_before_address_move(self) -> None:
        result = MODULE.evaluate(snapshot(carrier=True), "physical")
        self.assertEqual(result["status"], "PASS")

    def test_physical_phase_rejects_duplicate_service_address(self) -> None:
        state = snapshot(carrier=True)
        state["ethernet"]["ipv4"] = ["192.168.1.39"]
        result = MODULE.evaluate(state, "physical")
        self.assertFalse(result["checks"]["ethernet_does_not_duplicate_address"])

    def test_post_switch_requires_exclusive_address_and_route(self) -> None:
        result = MODULE.evaluate(snapshot(carrier=True, migrated=True), "post-switch")
        self.assertEqual(result["status"], "PASS")

    def test_post_switch_rejects_wifi_route(self) -> None:
        state = snapshot(carrier=True, migrated=True)
        state["route"]["device"] = "wlo1"
        result = MODULE.evaluate(state, "post-switch")
        self.assertFalse(result["checks"]["route_uses_ethernet"])

    def test_qgc_restart_or_listener_collision_blocks_both_phases(self) -> None:
        for field, value in (("restarts", 1), ("udp_14550_listeners", 2)):
            with self.subTest(field=field):
                state = snapshot(carrier=True)
                state["qgc"][field] = value
                self.assertEqual(MODULE.evaluate(state, "physical")["status"], "BLOCKED")


if __name__ == "__main__":
    unittest.main()
