from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.network.validate_g1_switch_acceptance import (
    AcceptanceError,
    DEFAULT_CONTRACT,
    DEFAULT_RESULTS,
    EVIDENCE,
    UPSTREAM,
    load_json,
    validate,
)


class G1SwitchAcceptanceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.contract = load_json(DEFAULT_CONTRACT)
        self.results = load_json(DEFAULT_RESULTS)
        self.upstream = load_json(UPSTREAM)

    def assert_rejected(self, control: str, mutation) -> None:
        contract = copy.deepcopy(self.contract)
        results = copy.deepcopy(self.results)
        upstream = copy.deepcopy(self.upstream)
        mutation(contract, results, upstream)
        with self.assertRaisesRegex(AcceptanceError, control):
            validate(contract, results, upstream)

    def test_repository_not_run_template_is_valid(self) -> None:
        report = validate(self.contract, self.results, self.upstream)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["result_status"], "NOT_RUN")
        self.assertFalse(report["flight_approval"])

    def test_twenty_controls_fail_closed(self) -> None:
        mutations = [
            ("G1-S20-01", lambda c, r, u: c.__setitem__("revision", "2.0.0")),
            ("G1-S20-02", lambda c, r, u: c.__setitem__("extra", True)),
            ("G1-S20-03", lambda c, r, u: c.__setitem__("execution_authorized", True)),
            ("G1-S20-04", lambda c, r, u: c.__setitem__("authority", "CONFIGURE_NETWORK")),
            ("G1-S20-05", lambda c, r, u: c["candidate"].__setitem__("model", "UNKNOWN")),
            ("G1-S20-06", lambda c, r, u: c["candidate"].__setitem__("installed", True)),
            ("G1-S20-07", lambda c, r, u: c["expected_ports"].__setitem__("rj45_fast_ethernet", 7)),
            ("G1-S20-08", lambda c, r, u: c["expected_ports"].__setitem__("sfp_counts_as_copper", True)),
            ("G1-S20-09", lambda c, r, u: u["candidate_switch"]["capacity_assessment"].__setitem__("status", "QUALIFIED")),
            ("G1-S20-10", lambda c, r, u: c["scope"].__setitem__("segment", "video")),
            ("G1-S20-11", lambda c, r, u: c["scope"].__setitem__("camera_attachment", "ALLOWED")),
            ("G1-S20-12", lambda c, r, u: c["scope"].__setitem__("extend_mode", "ALLOWED")),
            ("G1-S20-13", lambda c, r, u: c["required_evidence"].remove("MODEL_LABEL_PHOTO")),
            ("G1-S20-14", lambda c, r, u: c["required_evidence"].remove("NEGOTIATED_SPEED_PER_PORT")),
            ("G1-S20-15", lambda c, r, u: c["required_evidence"].remove("MIN_MAX_INPUT_AND_BROWNOUT_TEST")),
            ("G1-S20-16", lambda c, r, u: c["required_evidence"].remove("CURRENT_TEMPERATURE_AND_RESTART_REPORT")),
            ("G1-S20-17", lambda c, r, u: c["required_evidence"].remove("FAULT_AND_RECOVERY_TEST")),
            ("G1-S20-18", lambda c, r, u: r.__setitem__("extra", True)),
            ("G1-S20-19", lambda c, r, u: r["measurements"].__setitem__("claimed", True)),
            ("G1-S20-20", lambda c, r, u: r.__setitem__("flight_approval", True)),
        ]
        for control, mutation in mutations:
            with self.subTest(control=control):
                self.assert_rejected(control, mutation)

    def complete_results(self, root: Path) -> dict:
        results = copy.deepcopy(self.results)
        results.update({
            "status": "BENCH_PASS", "operator": "bench-operator",
            "started_utc": "2026-09-19T10:00:00Z", "completed_utc": "2026-09-19T11:00:00Z",
            "operator_confirmed": True, "hardware_verified": True,
            "measurements": {
                "copper_ports_tested": 9, "fe_ports_at_100mbps": 8,
                "ge_ports_at_1000mbps": 1, "power_brownout_pass": True,
                "thermal_restart_pass": True, "fault_recovery_pass": True,
                "video_isolation_pass": True,
            },
        })
        for index, kind in enumerate(EVIDENCE):
            relative = f"artifact-{index}.txt"
            payload = f"synthetic-{kind}\n".encode()
            (root / relative).write_bytes(payload)
            results["artifacts"].append({"kind": kind, "path": relative, "sha256": hashlib.sha256(payload).hexdigest()})
        return results

    def test_synthetic_bench_bundle_validates_but_never_approves_flight(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = validate(self.contract, self.complete_results(root), self.upstream, root)
            self.assertEqual(report["result_status"], "BENCH_PASS")
            self.assertFalse(report["flight_approval"])

    def test_digest_mismatch_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            results = self.complete_results(root)
            results["artifacts"][0]["sha256"] = "0" * 64
            with self.assertRaisesRegex(AcceptanceError, "G1-S20-18"):
                validate(self.contract, results, self.upstream, root)

    def test_path_escape_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            results = self.complete_results(root)
            results["artifacts"][0]["path"] = "../escape.txt"
            with self.assertRaisesRegex(AcceptanceError, "G1-S20-18"):
                validate(self.contract, results, self.upstream, root)


if __name__ == "__main__":
    unittest.main()
