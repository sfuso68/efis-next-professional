from __future__ import annotations

import argparse
import copy
import contextlib
import io
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from datetime import datetime, timezone
from unittest import mock


ROOT = Path(__file__).parents[1]
NETWORK = ROOT / "tools" / "network"
sys.path.insert(0, str(NETWORK))
SCRIPT = NETWORK / "assess_edp_capture.py"
SPEC = importlib.util.spec_from_file_location("assess_edp_capture", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def report(*, simulated_frames: int = 200, simulated_samples: int | None = None) -> dict[str, object]:
    if simulated_samples is None:
        simulated_samples = simulated_frames * 18
    real_frames = 200 if simulated_samples == 0 else 0
    mixed_frames = 200 - simulated_frames - real_frames
    document: dict[str, object] = {
        "schema": "efis-next.edp-passive-capture.v1",
        "timestamp_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "mode": "PASSIVE_AF_PACKET_NO_RETURN_PATH",
        "interface": "eth0",
        "endpoint": {"source": "10.77.10.2", "destination": "10.77.10.12", "port": 49003},
        "expected_source_node": 47,
        "requested_duration_seconds": 10.0,
        "actual_duration_seconds": 10.0,
        "packets_seen": 220,
        "packets_ignored": 20,
        "frames": 200,
        "datagram_hz": 20.0,
        "bytes": 138800,
        "malformed": 0,
        "decode_errors": {},
        "identity_rejected": 0,
        "sequence_gaps": 0,
        "sequence_gap_percent": 0.0,
        "duplicates": 0,
        "reversed": 0,
        "publisher_restarts": 0,
        "simulated_frames": simulated_frames,
        "real_frames": real_frames,
        "mixed_provenance_frames": mixed_frames,
        "simulated_samples": simulated_samples,
        "total_samples": 3600,
        "interarrival": {"samples": 199, "average_ms": 50.0,
                         "maximum_ms": 55.0, "p95_ms": 52.0, "mdev_ms": 1.0},
        "acceptance_limits": {
            "minimum_hz": 10.0,
            "maximum_gap_percent": 0.1,
            "maximum_malformed": 0,
            "maximum_duplicates": 0,
            "maximum_reversed": 0,
            "maximum_publisher_restarts": 0,
            "maximum_interarrival_ms": 250.0,
            "maximum_p95_interarrival_ms": 100.0,
        },
        "streams": [{"source_node": 47, "publisher_instance": 1234,
                     "frames": 200, "gaps": 0, "duplicates": 0,
                     "reversed": 0, "first_sequence": 1,
                     "last_sequence": 200, "current": True}],
        "socket_errors": 0,
        "raw_payload_persisted": False,
        "flight_approval": False,
    }
    limits = document["acceptance_limits"]
    assert isinstance(limits, dict)
    args = argparse.Namespace(
        duration_seconds=10.0, minimum_hz=limits["minimum_hz"],
        max_gap_percent=limits["maximum_gap_percent"],
        max_malformed=limits["maximum_malformed"],
        max_duplicates=limits["maximum_duplicates"],
        max_reversed=limits["maximum_reversed"],
        max_publisher_restarts=limits["maximum_publisher_restarts"],
        max_interarrival_ms=limits["maximum_interarrival_ms"],
        max_p95_interarrival_ms=limits["maximum_p95_interarrival_ms"],
    )
    document["checks"] = MODULE.capture.evaluate(dict(document), args)
    document["status"] = "PASS"
    return document


class AssessEdpCaptureTests(unittest.TestCase):
    def write(self, directory: str, document: object) -> Path:
        path = Path(directory) / "edp.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def load(self, document: object) -> tuple[dict[str, object], str]:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        return MODULE.load_report(self.write(directory.name, document))

    def test_valid_sim_report_is_bench_only(self) -> None:
        document, digest = self.load(report())
        result = MODULE.classify(document, digest)
        self.assertEqual(result["evidence_status"], "PASS")
        self.assertEqual(result["feed_classification"], "SIM_ONLY")
        self.assertEqual(result["qualification_status"], "BENCH_SIM_ONLY")
        self.assertFalse(result["flight_approval"])
        self.assertTrue(all(group["status"] == "PASS"
                            for group in result["groups"].values()))

    def test_real_and_mixed_feeds_are_explicit(self) -> None:
        cases = ((0, 0, "REAL_ONLY", "BENCH_REAL_ONLY"),
                 (0, 50, "MIXED", "BENCH_MIXED"))
        for count, samples, expected, qualification in cases:
            with self.subTest(count=count, samples=samples):
                document, digest = self.load(report(simulated_frames=count,
                                                    simulated_samples=samples))
                result = MODULE.classify(document, digest)
                self.assertEqual(result["feed_classification"], expected)
                self.assertEqual(result["qualification_status"], qualification)

    def test_recomputes_failure_groups(self) -> None:
        document = report()
        document["duplicates"] = 1
        streams = document["streams"]
        assert isinstance(streams, list) and isinstance(streams[0], dict)
        streams[0]["duplicates"] = 1
        streams[0]["last_sequence"] = 199
        document["checks"]["duplicates_within_limit"] = False
        document["status"] = "FAIL"
        validated, digest = self.load(document)
        result = MODULE.classify(validated, digest)
        self.assertEqual(result["evidence_status"], "FAIL")
        self.assertEqual(result["groups"]["continuity"]["status"], "FAIL")

    def test_rejects_tampered_embedded_checks_and_status(self) -> None:
        for mutation in ("check", "status"):
            document = report()
            if mutation == "check":
                document["checks"]["frames_present"] = False
            else:
                document["status"] = "FAIL"
            with self.subTest(mutation=mutation), self.assertRaises(MODULE.ReportError):
                self.load(document)

    def test_rejects_privacy_and_approval_escalation(self) -> None:
        for field in ("raw_payload_persisted", "flight_approval"):
            document = report(); document[field] = True
            with self.subTest(field=field), self.assertRaises(MODULE.ReportError):
                self.load(document)

    def test_rejects_counter_and_stream_inconsistency(self) -> None:
        mutations = []
        a = report(); a["malformed"] = 1; mutations.append(a)
        b = report(); b["sequence_gap_percent"] = 1.0; mutations.append(b)
        c = report(); c["streams"][0]["frames"] = 199; mutations.append(c)
        d = report(); d["streams"][0]["source_node"] = 99; mutations.append(d)
        e = report(); e["simulated_frames"] = 201; mutations.append(e)
        f = report(); f["packets_seen"] = 219; mutations.append(f)
        g = report(); g["bytes"] = 1; mutations.append(g)
        h = report(); h["streams"][0]["last_sequence"] = 199; mutations.append(h)
        for index, document in enumerate(mutations):
            with self.subTest(index=index), self.assertRaises(MODULE.ReportError):
                self.load(document)

    def test_rejects_unknown_schema_mode_checks_and_limits(self) -> None:
        mutations = []
        a = report(); a["schema"] = "unknown"; mutations.append(a)
        b = report(); b["mode"] = "UDP_BIND"; mutations.append(b)
        c = report(); c["checks"]["unknown"] = True; mutations.append(c)
        d = report(); d["acceptance_limits"]["unknown"] = 1; mutations.append(d)
        e = report(); e["acceptance_limits"]["maximum_duplicates"] = 0.5; mutations.append(e)
        for index, document in enumerate(mutations):
            with self.subTest(index=index), self.assertRaises(MODULE.ReportError):
                self.load(document)

    def test_digest_changes_when_source_changes(self) -> None:
        first, digest_a = self.load(report())
        changed = copy.deepcopy(first); changed["timestamp_utc"] = "2026-01-01T00:00:00+00:00"
        _, digest_b = self.load(changed)
        self.assertNotEqual(digest_a, digest_b)

    def test_cli_writes_atomic_assessment(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = self.write(directory, report())
            output = Path(directory) / "assessment.json"
            with mock.patch.object(sys, "argv", [str(SCRIPT), str(source),
                                                  "--output", str(output)]):
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(MODULE.main(), 0)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(result["schema"], MODULE.SCHEMA)
            self.assertEqual(result["evidence_status"], "PASS")
            self.assertEqual(len(result["source_sha256"]), 64)


if __name__ == "__main__":
    unittest.main()
