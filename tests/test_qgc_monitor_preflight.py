from __future__ import annotations

import importlib.util
from pathlib import Path
import socket
import sys
import tempfile
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "groundstation" / "qgc_monitor_preflight.py"
SPEC = importlib.util.spec_from_file_location("qgc_monitor_preflight", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class QgcMonitorPreflightTests(unittest.TestCase):
    def test_blocked_exit_is_non_transient_configuration_failure(self) -> None:
        self.assertEqual(MODULE.BLOCKED_EXIT, 78)

    def test_pass_requires_all_three_guards(self) -> None:
        report = MODULE.evaluate(
            serial_devices=[], bridge_active=False, port_available=True
        )
        self.assertEqual(report["mode"], "MONITOR_ONLY_NO_PIXHAWK_RETURN")
        self.assertEqual(report["status"], "PASS")

    def test_any_acm_device_blocks_start(self) -> None:
        report = MODULE.evaluate(
            serial_devices=["/dev/ttyACM0"],
            bridge_active=False,
            port_available=True,
        )
        self.assertEqual(report["status"], "BLOCKED")
        self.assertFalse(report["checks"]["pixhawk_usb_absent"]["pass"])

    def test_legacy_bridge_blocks_start(self) -> None:
        report = MODULE.evaluate(
            serial_devices=[], bridge_active=True, port_available=True
        )
        self.assertEqual(report["status"], "BLOCKED")

    def test_occupied_udp_port_blocks_start(self) -> None:
        listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        listener.bind(("127.0.0.1", 0))
        try:
            port = listener.getsockname()[1]
            self.assertFalse(MODULE.udp_port_available(port, bind_address="127.0.0.1"))
        finally:
            listener.close()

    def test_free_udp_port_passes_probe(self) -> None:
        listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        listener.bind(("127.0.0.1", 0))
        port = listener.getsockname()[1]
        listener.close()
        self.assertTrue(MODULE.udp_port_available(port, bind_address="127.0.0.1"))

    def test_report_contains_no_telemetry_payload(self) -> None:
        report = MODULE.evaluate(
            serial_devices=[], bridge_active=False, port_available=True
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            MODULE.write_report(path, report)
            text = path.read_text(encoding="utf-8")
        self.assertIn('"status": "PASS"', text)
        self.assertNotIn("mavlink_payload", text)


if __name__ == "__main__":
    unittest.main()
