from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "network_endpoint_acceptance.py"
SPEC = importlib.util.spec_from_file_location("network_endpoint_acceptance", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


PING = """100 packets transmitted, 100 received, 0% packet loss, time 20213ms
rtt min/avg/max/mdev = 0.127/0.163/0.595/0.051 ms
"""


class NetworkEndpointAcceptanceTests(unittest.TestCase):
    def test_target_is_bounded_to_bench_networks(self) -> None:
        target = MODULE.parse_target("pixhawk,10.77.10.3,0,2,1")
        self.assertEqual(target.address, "10.77.10.3")
        with self.assertRaises(argparse.ArgumentTypeError):
            MODULE.parse_target("internet,8.8.8.8,0,2,1")

    def test_target_rejects_invalid_identity_and_limits(self) -> None:
        for value in (
            "bad id,10.77.10.3,0,2,1",
            "bad,10.77.10.3,-1,2,1",
            "bad,10.77.10.3,101,2,1",
            "incomplete,10.77.10.3,0",
        ):
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                MODULE.parse_target(value)

    def test_linux_ping_summary_is_parsed(self) -> None:
        metrics = MODULE.parse_ping(PING)
        self.assertEqual(metrics["transmitted"], 100)
        self.assertEqual(metrics["received"], 100)
        self.assertEqual(metrics["average_ms"], 0.163)
        self.assertEqual(metrics["rtt_mdev_ms"], 0.051)

    def test_incomplete_summary_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            MODULE.parse_ping("100 packets transmitted")

    def test_thresholds_pass_and_fail_independently(self) -> None:
        target = MODULE.parse_target("pfd,10.77.10.11,0,2,1")
        metrics = MODULE.parse_ping(PING)
        self.assertEqual(MODULE.evaluate(target, metrics)["status"], "PASS")
        strict = MODULE.parse_target("pfd,10.77.10.11,0,0.1,0.01")
        report = MODULE.evaluate(strict, metrics)
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(report["checks"]["average_within_limit"])
        self.assertFalse(report["checks"]["rtt_mdev_within_limit"])

    def test_probe_is_fail_closed_on_command_or_parse_failure(self) -> None:
        target = MODULE.parse_target("pfd,10.77.10.11,0,2,1")

        def failed(*args: object, **kwargs: object) -> subprocess.CompletedProcess[str]:
            return subprocess.CompletedProcess([], 2, stdout="", stderr="missing")

        report = MODULE.probe(
            target,
            count=5,
            interval_seconds=0.2,
            timeout_seconds=1,
            runner=failed,
        )
        self.assertEqual(report["status"], "FAIL")
        self.assertEqual(report["ping_exit_code"], 2)

    def test_report_write_is_atomic_and_parseable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.json"
            MODULE.write_report(output, {"status": "PASS"})
            self.assertEqual(output.read_text(encoding="utf-8"), '{\n  "status": "PASS"\n}\n')
            self.assertFalse(output.with_suffix(".json.tmp").exists())


if __name__ == "__main__":
    unittest.main()
