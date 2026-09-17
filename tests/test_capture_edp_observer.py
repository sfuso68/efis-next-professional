from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import struct
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "capture_edp_observer.py"
SPEC = importlib.util.spec_from_file_location("capture_edp_observer", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def sample_payload(*, simulated: bool = True) -> bytes:
    signals = (*range(600, 610), *range(640, 644), *range(660, 664))
    payload = bytearray((0, 0)) + len(signals).to_bytes(2, "big")
    for signal in signals:
        payload += signal.to_bytes(2, "big")
        payload += struct.pack(">d", float(signal))
        payload += (1).to_bytes(2, "big")
        payload += bytes((0, 2, MODULE.EXPECTED_UNITS[signal],
                          0, 0, 0, 0, simulated))
        payload += (47).to_bytes(2, "big")
        payload += (1).to_bytes(2, "big")
        payload += (1).to_bytes(4, "big")
        payload += struct.pack(">d", float(signal))
    assert len(payload) == MODULE.PAYLOAD_SIZE
    return bytes(payload)


def datagram(*, node: int = 47, publisher: int = 1234, sequence: int = 1,
             simulated: bool = True) -> bytes:
    payload = sample_payload(simulated=simulated)
    return (b"EDP1" + (1).to_bytes(2, "big") + node.to_bytes(4, "big") +
            publisher.to_bytes(8, "big") + sequence.to_bytes(8, "big") +
            (10_000_000).to_bytes(8, "big") + len(payload).to_bytes(4, "big") +
            MODULE.crc32c(payload).to_bytes(4, "big") + payload)


def args(**overrides: object) -> argparse.Namespace:
    values = dict(duration_seconds=10.0, minimum_hz=10.0,
                  max_gap_percent=0.1, max_malformed=0, max_duplicates=0,
                  max_reversed=0, max_publisher_restarts=0,
                  max_interarrival_ms=250.0, max_p95_interarrival_ms=100.0)
    values.update(overrides)
    return argparse.Namespace(**values)


class CaptureEdpObserverTests(unittest.TestCase):
    def test_parses_authoritative_complete_frame(self) -> None:
        parsed = MODULE.parse_edp_v1(datagram())
        self.assertEqual((parsed.source_node, parsed.publisher_instance, parsed.sequence),
                         (47, 1234, 1))
        self.assertEqual(parsed.simulated_samples, 18)

    def test_rejects_header_identity_length_and_crc(self) -> None:
        cases = []
        truncated = datagram()[:20]
        cases.append((truncated, "TRUNCATED"))
        magic = bytearray(datagram()); magic[0] ^= 1
        cases.append((bytes(magic), "BAD_MAGIC"))
        identity = bytearray(datagram()); identity[6:10] = bytes(4)
        cases.append((bytes(identity), "INVALID_IDENTITY"))
        length = bytearray(datagram()); length[34:38] = (1).to_bytes(4, "big")
        cases.append((bytes(length), "LENGTH_MISMATCH"))
        crc = bytearray(datagram()); crc[-1] ^= 1
        cases.append((bytes(crc), "CRC_MISMATCH"))
        for payload, expected in cases:
            with self.subTest(expected=expected), self.assertRaisesRegex(MODULE.EdpError, expected):
                MODULE.parse_edp_v1(payload)

    def test_rejects_duplicate_signal_even_with_valid_crc(self) -> None:
        wire = bytearray(datagram())
        first_signal = wire[MODULE.ENVELOPE_SIZE + 4:MODULE.ENVELOPE_SIZE + 6]
        second = MODULE.ENVELOPE_SIZE + 4 + MODULE.SAMPLE_SIZE
        wire[second:second + 2] = first_signal
        payload = bytes(wire[MODULE.ENVELOPE_SIZE:])
        wire[38:42] = MODULE.crc32c(payload).to_bytes(4, "big")
        with self.assertRaisesRegex(MODULE.EdpError, "MALFORMED_PAYLOAD"):
            MODULE.parse_edp_v1(bytes(wire))

    def test_rejects_wrong_engineering_unit_even_with_valid_crc(self) -> None:
        wire = bytearray(datagram())
        unit = MODULE.ENVELOPE_SIZE + 4 + 14
        wire[unit] = 0
        payload = bytes(wire[MODULE.ENVELOPE_SIZE:])
        wire[38:42] = MODULE.crc32c(payload).to_bytes(4, "big")
        with self.assertRaisesRegex(MODULE.EdpError, "MALFORMED_PAYLOAD"):
            MODULE.parse_edp_v1(bytes(wire))

    def test_tracks_gap_duplicate_reverse_and_restart(self) -> None:
        streams = {}
        restarts = 0
        for sequence in (1, 3, 3, 2):
            restarts += MODULE.observe_sequence(streams, MODULE.parse_edp_v1(
                datagram(sequence=sequence)))
        restarts += MODULE.observe_sequence(streams, MODULE.parse_edp_v1(
            datagram(publisher=5678, sequence=1)))
        old = streams[(47, 1234)]
        self.assertEqual((old.gaps, old.duplicates, old.reversed, restarts),
                         (1, 1, 1, 1))
        MODULE.observe_sequence(streams, MODULE.parse_edp_v1(
            datagram(publisher=1234, sequence=4)))
        self.assertEqual(old.reversed, 2)

    def test_clean_report_passes_all_gates(self) -> None:
        report = dict(actual_duration_seconds=10.0, datagram_hz=20.0,
                      frames=200, malformed=0, identity_rejected=0,
                      sequence_gaps=0, duplicates=0, reversed=0,
                      publisher_restarts=0, socket_errors=0,
                      interarrival=dict(samples=199, average_ms=50.0,
                                        maximum_ms=55.0, p95_ms=52.0,
                                        mdev_ms=1.0))
        checks = MODULE.evaluate(report, args())
        self.assertTrue(all(checks.values()))
        self.assertEqual(report["sequence_gap_percent"], 0.0)

    def test_every_safety_gate_fails_closed(self) -> None:
        baseline = dict(actual_duration_seconds=10.0, datagram_hz=20.0,
                        frames=200, malformed=0, identity_rejected=0,
                        sequence_gaps=0, duplicates=0, reversed=0,
                        publisher_restarts=0, socket_errors=0,
                        interarrival=dict(samples=199, average_ms=50.0,
                                          maximum_ms=55.0, p95_ms=52.0,
                                          mdev_ms=1.0))
        cases = (("actual_duration_seconds", 9.0), ("datagram_hz", 9.0),
                 ("frames", 0), ("malformed", 1), ("identity_rejected", 1),
                 ("sequence_gaps", 1), ("duplicates", 1), ("reversed", 1),
                 ("publisher_restarts", 1), ("socket_errors", 1))
        for field, value in cases:
            with self.subTest(field=field):
                report = dict(baseline); report[field] = value
                self.assertFalse(all(MODULE.evaluate(report, args()).values()))
        for field, value in (("samples", 0), ("maximum_ms", 250.001),
                             ("p95_ms", 100.001)):
            with self.subTest(timing=field):
                report = dict(baseline)
                report["interarrival"] = dict(baseline["interarrival"])
                report["interarrival"][field] = value
                self.assertFalse(all(MODULE.evaluate(report, args()).values()))

    def test_crc32c_known_vector(self) -> None:
        self.assertEqual(MODULE.crc32c(b"123456789"), 0xE3069283)


if __name__ == "__main__":
    unittest.main()
