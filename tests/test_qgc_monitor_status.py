from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "groundstation" / "qgc_monitor_status.py"
SPEC = importlib.util.spec_from_file_location("qgc_monitor_status", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


PREFLIGHT = {
    "status": "PASS",
    "checks": {"artifact_integrity": {"pass": True}},
}
QGC = {
    "LoadState": "loaded",
    "ActiveState": "active",
    "SubState": "running",
    "NRestarts": "0",
}
FIREWALL = {"LoadState": "loaded", "ActiveState": "active"}
LISTENER = [
    'UNCONN 0 0 0.0.0.0:14550 0.0.0.0:* users:(("QGroundControl",pid=1,fd=1))'
]


class QgcMonitorStatusTests(unittest.TestCase):
    def evaluate(self, **overrides: object) -> dict[str, object]:
        arguments = {
            "preflight": PREFLIGHT,
            "qgc_state": QGC,
            "qgc_enabled": True,
            "listeners": LISTENER,
            "firewall_state": FIREWALL,
            "firewall_enabled": True,
        }
        arguments.update(overrides)
        return MODULE.evaluate(**arguments)

    def test_all_gates_pass(self) -> None:
        self.assertEqual(self.evaluate()["status"], "PASS")

    def test_missing_firewall_is_partial(self) -> None:
        report = self.evaluate(
            firewall_state={"LoadState": "not-found", "ActiveState": "inactive"},
            firewall_enabled=False,
        )
        self.assertEqual(report["status"], "PARTIAL")

    def test_failed_preflight_is_failure(self) -> None:
        report = self.evaluate(preflight={"status": "BLOCKED", "checks": {}})
        self.assertEqual(report["status"], "FAIL")

    def test_restart_or_competing_listener_is_failure(self) -> None:
        restarted = dict(QGC)
        restarted["NRestarts"] = "1"
        self.assertEqual(self.evaluate(qgc_state=restarted)["status"], "FAIL")
        self.assertEqual(
            self.evaluate(listeners=[*LISTENER, "other :14550"])["status"],
            "FAIL",
        )


if __name__ == "__main__":
    unittest.main()
