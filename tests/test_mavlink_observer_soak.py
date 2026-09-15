from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "analyze_mavlink_observer_soak.py"
SPEC = importlib.util.spec_from_file_location("analyze_mavlink_observer_soak", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)

OBSERVERS = ["192.168.1.39:14550", "192.168.1.6:14550"]
DISPLAYS = ["10.77.10.11:49002", "10.77.10.13:49002"]


def fixture(*, error: int = 0, rollback: bool = False) -> list[str]:
    lines: list[str] = []
    for index, value in enumerate((100, 50 if rollback else 300)):
        recorded = (80, 200)[index]
        ddp = (160, 400)[index]
        lines.extend(
            (
                "event=blackbox_broker_health "
                f"mavlink_datagrams={value} mavlink_socket_errors={error} "
                "mavlink_truncated=0 mavlink_source_rejected=0 "
                f"recorded={recorded} ddp_datagrams={ddp} ddp_failures=0",
                "event=blackbox_mavlink_observer_health "
                f"datagrams={value * 2} failures=0",
            )
        )
        for endpoint in OBSERVERS:
            lines.append(
                "event=blackbox_mavlink_observer_target "
                f"destination={endpoint} datagrams={value} failures=0"
            )
        for endpoint in DISPLAYS:
            lines.append(
                "event=blackbox_display_health "
                f"destination={endpoint} datagrams={recorded} failures=0"
            )
    return lines


class ObserverSoakTests(unittest.TestCase):
    def analyze(self, lines: list[str]) -> dict[str, object]:
        return MODULE.analyze(
            MODULE.parse_lines(lines),
            duration_seconds=10,
            observers=OBSERVERS,
            displays=DISPLAYS,
            minimum_mavlink_hz=5,
            expected_sample_period_seconds=5,
            minimum_sample_coverage=1.0,
        )

    def test_complete_progressing_fixture_passes(self) -> None:
        report = self.analyze(fixture())
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["measured_mavlink_hz"], 20.0)

    def test_any_broker_error_fails(self) -> None:
        report = self.analyze(fixture(error=1))
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(report["checks"]["broker_errors_zero"])

    def test_counter_rollback_fails_monotonic_gate(self) -> None:
        report = self.analyze(fixture(rollback=True))
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(report["checks"]["broker_counters_monotonic"])

    def test_missing_observer_fails(self) -> None:
        lines = [line for line in fixture() if OBSERVERS[1] not in line]
        report = self.analyze(lines)
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(
            report["checks"][f"observer_{OBSERVERS[1]}_progress"]
        )

    def test_unrelated_journal_lines_are_ignored(self) -> None:
        records = MODULE.parse_lines(["kernel unrelated", *fixture()])
        self.assertEqual(len(records), 12)

    def test_incomplete_sample_coverage_fails(self) -> None:
        report = MODULE.analyze(
            MODULE.parse_lines(fixture()),
            duration_seconds=10,
            observers=OBSERVERS,
            displays=DISPLAYS,
            minimum_mavlink_hz=5,
            expected_sample_period_seconds=1,
            minimum_sample_coverage=0.9,
        )
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(report["checks"]["broker_sample_coverage"])

    def test_observer_delta_mismatch_fails_parity(self) -> None:
        lines = fixture()
        lines[-4] = lines[-4].replace("datagrams=300", "datagrams=299")
        report = self.analyze(lines)
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(
            report["checks"][f"observer_{OBSERVERS[0]}_parity"]
        )


if __name__ == "__main__":
    unittest.main()
