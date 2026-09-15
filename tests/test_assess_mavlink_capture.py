from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).parents[1]
NETWORK = ROOT / "tools" / "network"
sys.path.insert(0, str(NETWORK))
SCRIPT = NETWORK / "assess_mavlink_capture.py"
SPEC = importlib.util.spec_from_file_location("assess_mavlink_capture", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def checks() -> dict[str, bool]:
    capture = sys.modules["capture_mavlink_observer"]
    keys = capture.INTEGRITY_CHECKS + capture.LIVENESS_CHECKS + capture.UNIFORMITY_CHECKS
    return {key: True for key in keys}


class AssessMavlinkCaptureTests(unittest.TestCase):
    def write(self, directory: str, document: object) -> Path:
        path = Path(directory) / "report.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def test_classifies_degraded_quality_without_failing_link(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            values = checks()
            values["maximum_interarrival_within_limit"] = False
            path = self.write(directory, {
                "schema": "efis-next.mavlink-passive-capture.v2",
                "status": "FAIL",
                "checks": values,
            })
            document, digest = MODULE.load_report(path)
            result = MODULE.classify(document, digest)
            self.assertEqual(result["assessment"]["link_status"], "PASS")
            self.assertEqual(result["assessment"]["quality_status"], "DEGRADED")
            self.assertEqual(MODULE.exit_code(result), 2)

    def test_link_failure_returns_one(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            values = checks()
            values["heartbeat_frames_present"] = False
            path = self.write(directory, {
                "schema": "efis-next.mavlink-passive-capture.v3",
                "checks": values,
            })
            document, digest = MODULE.load_report(path)
            result = MODULE.classify(document, digest)
            self.assertEqual(result["assessment"]["link_status"], "FAIL")
            self.assertEqual(MODULE.exit_code(result), 1)

    def test_full_pass_returns_zero(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = self.write(directory, {
                "schema": "efis-next.mavlink-passive-capture.v3",
                "checks": checks(),
            })
            document, digest = MODULE.load_report(path)
            result = MODULE.classify(document, digest)
            self.assertEqual(MODULE.exit_code(result), 0)

    def test_rejects_bad_schema_and_non_boolean_checks(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            for document in (
                {"schema": "unknown", "checks": checks()},
                {"schema": "efis-next.mavlink-passive-capture.v2", "checks": {"x": 1}},
            ):
                with self.subTest(document=document):
                    path = self.write(directory, document)
                    with self.assertRaises(MODULE.ReportError):
                        MODULE.load_report(path)


if __name__ == "__main__":
    unittest.main()
