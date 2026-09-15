from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "server_observer_reboot_acceptance.py"
SPEC = importlib.util.spec_from_file_location("server_observer_reboot_acceptance", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


SINK = {
    "LoadState": "loaded",
    "ActiveState": "active",
    "SubState": "running",
    "NRestarts": "0",
}
FIREWALL = {
    "LoadState": "loaded",
    "ActiveState": "active",
    "SubState": "exited",
}
HEALTH = {
    "status": "LIVE",
    "mode": "RX_ONLY_NO_RETURN_PATH",
    "allowed_source": "192.168.1.47",
    "updated_unix_ns": 10_000_000_000,
    "malformed": 0,
    "socket_errors": 0,
    "source_rejected": 0,
}


class ServerObserverRebootAcceptanceTests(unittest.TestCase):
    def evaluate(self, **overrides: object) -> dict[str, object]:
        arguments = {
            "phase": "check",
            "baseline_boot_id": "a",
            "current_boot_id": "b",
            "sink": SINK,
            "sink_enabled": True,
            "firewall": FIREWALL,
            "firewall_enabled": True,
            "health": HEALTH,
            "has_listener": True,
            "has_linger": True,
            "containers": MODULE.REQUIRED_CONTAINERS,
            "caddy_http_status": 401,
            "now_unix_ns": 11_000_000_000,
        }
        arguments.update(overrides)
        return MODULE.evaluate(**arguments)

    def test_complete_post_reboot_state_passes(self) -> None:
        self.assertEqual(self.evaluate()["status"], "PASS")

    def test_same_boot_or_stale_health_fails(self) -> None:
        self.assertEqual(self.evaluate(current_boot_id="a")["status"], "FAIL")
        self.assertEqual(self.evaluate(now_unix_ns=30_000_000_000)["status"], "FAIL")

    def test_sink_firewall_and_source_are_fail_closed(self) -> None:
        sink = dict(SINK)
        sink["NRestarts"] = "1"
        self.assertEqual(self.evaluate(sink=sink)["status"], "FAIL")
        firewall = dict(FIREWALL)
        firewall["ActiveState"] = "inactive"
        self.assertEqual(self.evaluate(firewall=firewall)["status"], "FAIL")
        health = dict(HEALTH)
        health["allowed_source"] = "192.168.1.99"
        self.assertEqual(self.evaluate(health=health)["status"], "FAIL")

    def test_container_and_caddy_gates(self) -> None:
        self.assertEqual(self.evaluate(containers={"caddy"})["status"], "FAIL")
        self.assertEqual(self.evaluate(caddy_http_status=200)["status"], "FAIL")


if __name__ == "__main__":
    unittest.main()
