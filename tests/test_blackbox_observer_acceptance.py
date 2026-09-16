from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "tools" / "network"))
SCRIPT = ROOT / "tools" / "network" / "blackbox_observer_acceptance.py"
SPEC = importlib.util.spec_from_file_location("blackbox_observer_acceptance", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class BlackboxObserverAcceptanceTests(unittest.TestCase):
    def report(self, **overrides: object) -> dict[str, object]:
        arguments = {
            "service": {
                "ActiveState": "active",
                "SubState": "running",
                "MainPID": "123",
                "NRestarts": "0",
            },
            "temperature_c": 49.9,
            "throttled_flags": 0,
            "available_memory_bytes": 3_800_000_000,
            "broker_rss_bytes": 4_240_000,
            "max_temperature_c": 80.0,
            "min_available_memory_bytes": 256 * 1024 * 1024,
        }
        arguments.update(overrides)
        return MODULE.resource_report(**arguments)

    def test_healthy_resources_pass(self) -> None:
        self.assertEqual(self.report()["status"], "PASS")

    def test_temperature_or_throttling_fails(self) -> None:
        self.assertEqual(self.report(temperature_c=80.0)["status"], "FAIL")
        self.assertEqual(self.report(throttled_flags=0x50000)["status"], "FAIL")

    def test_restart_or_low_memory_fails(self) -> None:
        service = {
            "ActiveState": "active",
            "SubState": "running",
            "MainPID": "123",
            "NRestarts": "1",
        }
        self.assertEqual(self.report(service=service)["status"], "FAIL")
        self.assertEqual(
            self.report(available_memory_bytes=128 * 1024 * 1024)["status"],
            "FAIL",
        )
        invalid_pid = dict(service)
        invalid_pid["NRestarts"] = "0"
        invalid_pid["MainPID"] = "invalid"
        self.assertEqual(self.report(service=invalid_pid)["status"], "FAIL")

    def test_parsers_are_fail_closed(self) -> None:
        self.assertEqual(MODULE.parse_temperature("temp=45.5'C"), 45.5)
        self.assertIsNone(MODULE.parse_temperature("unavailable"))
        self.assertEqual(MODULE.parse_throttled("throttled=0x50000"), 0x50000)
        self.assertIsNone(MODULE.parse_throttled("unavailable"))

    def test_proc_parsers(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            meminfo = root / "meminfo"
            meminfo.write_text("MemAvailable:       1024 kB\n", encoding="utf-8")
            status = root / "123" / "status"
            status.parent.mkdir()
            status.write_text("Name:\ttest\nVmRSS:\t42 kB\n", encoding="utf-8")
            self.assertEqual(MODULE.memory_available_bytes(meminfo), 1024 * 1024)
            self.assertEqual(MODULE.process_rss_bytes(123, root), 42 * 1024)


if __name__ == "__main__":
    unittest.main()
