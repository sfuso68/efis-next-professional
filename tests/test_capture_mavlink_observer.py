from __future__ import annotations

import argparse
import importlib.util
import ipaddress
from pathlib import Path
import socket
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "tools" / "network"))
SCRIPT = ROOT / "tools" / "network" / "capture_mavlink_observer.py"
SPEC = importlib.util.spec_from_file_location("capture_mavlink_observer", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def mavlink_frame(sequence: int, *, system: int = 1, component: int = 1) -> bytes:
    return bytes((0xFD, 0, 0, 0, sequence, system, component, 0, 0, 0, 0, 0))


def ethernet_udp(
    payload: bytes,
    *,
    source: str = "192.168.1.47",
    destination: str = "192.168.1.39",
    destination_port: int = 14550,
    vlan: bool = False,
) -> bytes:
    ethernet = bytes(12)
    if vlan:
        ethernet += bytes.fromhex("810000010800")
    else:
        ethernet += bytes.fromhex("0800")
    udp_length = 8 + len(payload)
    total_length = 20 + udp_length
    ip_header = bytearray(20)
    ip_header[0] = 0x45
    ip_header[2:4] = total_length.to_bytes(2, "big")
    ip_header[8] = 64
    ip_header[9] = 17
    ip_header[12:16] = ipaddress.ip_address(source).packed
    ip_header[16:20] = ipaddress.ip_address(destination).packed
    udp = bytearray(8)
    udp[0:2] = (40000).to_bytes(2, "big")
    udp[2:4] = destination_port.to_bytes(2, "big")
    udp[4:6] = udp_length.to_bytes(2, "big")
    return ethernet + bytes(ip_header) + bytes(udp) + payload


def arguments(**overrides: object) -> argparse.Namespace:
    values = {
        "duration_seconds": 10.0,
        "minimum_hz": 5.0,
        "max_gap_percent": 0.1,
        "max_malformed": 0,
        "max_duplicates": 0,
        "max_out_of_order": 0,
        "max_interarrival_ms": 1000.0,
        "max_p95_interarrival_ms": 100.0,
        "max_heartbeat_interarrival_ms": 3000.0,
    }
    values.update(overrides)
    return argparse.Namespace(**values)


class CaptureMavlinkObserverTests(unittest.TestCase):
    def test_decodes_untagged_and_vlan_udp(self) -> None:
        for vlan in (False, True):
            decoded = MODULE.decode_udp(ethernet_udp(b"payload", vlan=vlan))
            self.assertIsNotNone(decoded)
            assert decoded is not None
            self.assertEqual(decoded[0:2], ("192.168.1.47", "192.168.1.39"))
            self.assertEqual(decoded[3], 14550)
            self.assertEqual(decoded[4], b"payload")

    def test_ignores_non_ipv4_and_unrelated_endpoint(self) -> None:
        self.assertIsNone(MODULE.decode_udp(bytes(12) + bytes.fromhex("86dd") + bytes(40)))
        endpoint = MODULE.Endpoint("192.168.1.47", "192.168.1.39", 14550)
        self.assertIsNone(
            MODULE.observer_payload(
                ethernet_udp(b"payload", destination="192.168.1.6"), endpoint
            )
        )

    def test_rejects_truncation_fragment_and_invalid_udp_length(self) -> None:
        packet = ethernet_udp(b"payload")
        with self.assertRaises(MODULE.PacketError):
            MODULE.decode_udp(packet[:25])
        fragmented = bytearray(packet)
        fragmented[20:22] = (0x2000).to_bytes(2, "big")
        with self.assertRaises(MODULE.PacketError):
            MODULE.decode_udp(bytes(fragmented))
        invalid_udp = bytearray(packet)
        invalid_udp[38:40] = (7).to_bytes(2, "big")
        with self.assertRaises(MODULE.PacketError):
            MODULE.decode_udp(bytes(invalid_udp))

    def test_sequence_tracker_handles_wrap_gap_duplicate_and_reorder(self) -> None:
        stream = MODULE.StreamState(1, 1)
        for sequence in (254, 255, 0, 2, 2, 1):
            stream.observe(sequence)
        self.assertEqual(stream.frames, 6)
        self.assertEqual(stream.gaps, 1)
        self.assertEqual(stream.duplicates, 1)
        self.assertEqual(stream.out_of_order, 1)
        self.assertEqual(stream.last_sequence, 2)

    def test_streams_are_independent(self) -> None:
        first = MODULE.StreamState(1, 1)
        second = MODULE.StreamState(1, 50)
        first.observe(10)
        first.observe(12)
        second.observe(200)
        second.observe(201)
        self.assertEqual(first.gaps, 1)
        self.assertEqual(second.gaps, 0)

    def test_timing_report_is_bounded_aggregate(self) -> None:
        report = MODULE.timing_report([10.0, 20.0, 30.0, 40.0])
        self.assertEqual(report["samples"], 4)
        self.assertEqual(report["average_ms"], 25.0)
        self.assertEqual(report["maximum_ms"], 40.0)
        self.assertEqual(report["p95_ms"], 40.0)
        self.assertIsNotNone(report["mdev_ms"])
        boundary = MODULE.timing_report([1000.0001])
        self.assertEqual(boundary["maximum_ms"], 1000.001)
        self.assertEqual(boundary["p95_ms"], 1000.001)

    def test_evaluation_passes_clean_capture(self) -> None:
        report: dict[str, object] = {
            "actual_duration_seconds": 10.0,
            "datagram_hz": 26.0,
            "frames": 260,
            "sequence_gaps": 0,
            "duplicates": 0,
            "out_of_order": 0,
            "malformed": 0,
            "socket_errors": 0,
            "interarrival": {
                "samples": 259,
                "average_ms": 38.0,
                "maximum_ms": 45.0,
                "p95_ms": 42.0,
                "mdev_ms": 2.0,
            },
            "heartbeat": {
                "frames": 10,
                "interarrival": {
                    "samples": 9,
                    "average_ms": 1000.0,
                    "maximum_ms": 1000.0,
                    "p95_ms": 1000.0,
                    "mdev_ms": 0.0,
                },
            },
        }
        checks = MODULE.evaluate(report, arguments())
        self.assertTrue(all(checks.values()))
        self.assertEqual(report["sequence_gap_percent"], 0.0)

    def test_evaluation_fails_each_safety_gate(self) -> None:
        baseline = {
            "actual_duration_seconds": 10.0,
            "datagram_hz": 26.0,
            "frames": 260,
            "sequence_gaps": 0,
            "duplicates": 0,
            "out_of_order": 0,
            "malformed": 0,
            "socket_errors": 0,
            "interarrival": {
                "samples": 259,
                "average_ms": 38.0,
                "maximum_ms": 45.0,
                "p95_ms": 42.0,
                "mdev_ms": 2.0,
            },
            "heartbeat": {
                "frames": 10,
                "interarrival": {
                    "samples": 9,
                    "average_ms": 1000.0,
                    "maximum_ms": 1000.0,
                    "p95_ms": 1000.0,
                    "mdev_ms": 0.0,
                },
            },
        }
        for key, value in (
            ("actual_duration_seconds", 9.0),
            ("datagram_hz", 4.0),
            ("frames", 0),
            ("sequence_gaps", 2),
            ("duplicates", 1),
            ("out_of_order", 1),
            ("malformed", 1),
            ("socket_errors", 1),
        ):
            with self.subTest(key=key):
                report = dict(baseline)
                report[key] = value
                self.assertFalse(all(MODULE.evaluate(report, arguments()).values()))

    def test_evaluation_fails_timing_gates(self) -> None:
        baseline: dict[str, object] = {
            "actual_duration_seconds": 10.0,
            "datagram_hz": 26.0,
            "frames": 260,
            "sequence_gaps": 0,
            "duplicates": 0,
            "out_of_order": 0,
            "malformed": 0,
            "socket_errors": 0,
            "interarrival": {
                "samples": 259,
                "average_ms": 38.0,
                "maximum_ms": 45.0,
                "p95_ms": 42.0,
                "mdev_ms": 2.0,
            },
            "heartbeat": {
                "frames": 10,
                "interarrival": {
                    "samples": 9,
                    "average_ms": 1000.0,
                    "maximum_ms": 1000.0,
                    "p95_ms": 1000.0,
                    "mdev_ms": 0.0,
                },
            },
        }
        cases = (
            ("samples", 0, "timing_samples_present"),
            ("maximum_ms", 1000.001, "maximum_interarrival_within_limit"),
            ("p95_ms", 100.001, "p95_interarrival_within_limit"),
        )
        for field, value, check in cases:
            with self.subTest(field=field):
                report = dict(baseline)
                report["interarrival"] = dict(baseline["interarrival"])
                report["interarrival"][field] = value
                checks = MODULE.evaluate(report, arguments())
                self.assertFalse(checks[check])

    def test_evaluation_accepts_timing_boundaries(self) -> None:
        report: dict[str, object] = {
            "actual_duration_seconds": 10.0,
            "datagram_hz": 26.0,
            "frames": 260,
            "sequence_gaps": 0,
            "duplicates": 0,
            "out_of_order": 0,
            "malformed": 0,
            "socket_errors": 0,
            "interarrival": {
                "samples": 259,
                "average_ms": 38.0,
                "maximum_ms": 1000.0,
                "p95_ms": 100.0,
                "mdev_ms": 50.0,
            },
            "heartbeat": {
                "frames": 10,
                "interarrival": {
                    "samples": 9,
                    "average_ms": 1000.0,
                    "maximum_ms": 1000.0,
                    "p95_ms": 1000.0,
                    "mdev_ms": 0.0,
                },
            },
        }
        checks = MODULE.evaluate(report, arguments())
        self.assertTrue(checks["maximum_interarrival_within_limit"])
        self.assertTrue(checks["p95_interarrival_within_limit"])

    def test_evaluation_fails_heartbeat_gates(self) -> None:
        baseline: dict[str, object] = {
            "actual_duration_seconds": 10.0,
            "datagram_hz": 26.0,
            "frames": 260,
            "sequence_gaps": 0,
            "duplicates": 0,
            "out_of_order": 0,
            "malformed": 0,
            "socket_errors": 0,
            "interarrival": {
                "samples": 259,
                "average_ms": 38.0,
                "maximum_ms": 45.0,
                "p95_ms": 42.0,
                "mdev_ms": 2.0,
            },
            "heartbeat": {
                "frames": 10,
                "interarrival": {
                    "samples": 9,
                    "average_ms": 1000.0,
                    "maximum_ms": 1000.0,
                    "p95_ms": 1000.0,
                    "mdev_ms": 0.0,
                },
            },
        }
        cases = (
            (1, 1, 1000.0, "heartbeat_frames_present"),
            (2, 0, None, "heartbeat_timing_samples_present"),
            (2, 1, 3000.001, "maximum_heartbeat_interarrival_within_limit"),
        )
        for frames, samples, maximum, expected in cases:
            with self.subTest(expected=expected):
                report = dict(baseline)
                heartbeat = dict(baseline["heartbeat"])
                heartbeat["frames"] = frames
                heartbeat["interarrival"] = dict(heartbeat["interarrival"])
                heartbeat["interarrival"]["samples"] = samples
                heartbeat["interarrival"]["maximum_ms"] = maximum
                report["heartbeat"] = heartbeat
                self.assertFalse(MODULE.evaluate(report, arguments())[expected])

    def test_interval_distribution_and_bursts_are_aggregate(self) -> None:
        intervals = [1.0, 2.0, 200.0, 3.0, 600.0, 4.0, 1000.1]
        distribution = MODULE.interval_distribution(intervals)
        self.assertEqual(distribution["over_100_ms"], 3)
        self.assertEqual(distribution["over_500_ms"], 2)
        self.assertEqual(distribution["over_1000_ms"], 1)
        bursts = MODULE.burst_report(intervals, 100.0, 8)
        self.assertEqual(bursts["bursts"], 4)
        self.assertEqual(bursts["minimum_datagrams_per_burst"], 1)
        self.assertEqual(bursts["maximum_datagrams_per_burst"], 3)

    def test_heartbeat_state_keeps_only_aggregate_timing(self) -> None:
        state = MODULE.HeartbeatState(1, 1)
        state.observe(1_000_000_000)
        state.observe(2_000_000_000)
        report = state.report()
        self.assertEqual(report["frames"], 2)
        self.assertEqual(report["interarrival"]["maximum_ms"], 1000.0)
        self.assertNotIn("last_received_ns", report)

    def test_validators_are_fail_closed(self) -> None:
        self.assertEqual(MODULE.validated_interface("wlo1"), "wlo1")
        self.assertEqual(MODULE.validated_ipv4("192.168.1.47", "source"), "192.168.1.47")
        with self.assertRaises(argparse.ArgumentTypeError):
            MODULE.validated_interface("wlo1;bad")
        with self.assertRaises(argparse.ArgumentTypeError):
            MODULE.validated_ipv4("::1", "source")

    def test_report_does_not_contain_payload(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            report = {"raw_payload_persisted": False, "frames": 10}
            MODULE.write_report(path, report)
            content = path.read_text(encoding="utf-8")
            self.assertIn('"raw_payload_persisted": false', content)
            self.assertNotIn("private-flight-data", content)

    def test_capture_aggregates_without_binding_udp(self) -> None:
        class FakeSocket:
            def __init__(self) -> None:
                self.packets = iter(
                    (
                        ethernet_udp(mavlink_frame(10)),
                        ethernet_udp(mavlink_frame(11)),
                    )
                )
                self.bound: tuple[str, int] | None = None
                self.closed = False

            def bind(self, endpoint: tuple[str, int]) -> None:
                self.bound = endpoint

            def settimeout(self, _seconds: float) -> None:
                pass

            def recv(self, _size: int) -> bytes:
                return next(self.packets)

            def close(self) -> None:
                self.closed = True

        receiver = FakeSocket()
        ticks = iter((0, 0, 1_000_000_000, 1_000_000_000,
                      2_000_000_000, 10_000_000_000, 10_000_000_000))
        args = arguments(
            minimum_hz=0.1,
            max_interarrival_ms=1000.0,
            max_p95_interarrival_ms=1000.0,
        )
        args.interface = "wlo1"
        args.source = "192.168.1.47"
        args.destination = "192.168.1.39"
        args.port = 14550
        with mock.patch.object(socket, "AF_PACKET", 17, create=True):
            report = MODULE.capture(
                args,
                socket_factory=lambda *_args: receiver,
                monotonic_ns=lambda: next(ticks),
            )
        self.assertEqual(receiver.bound, ("wlo1", 0))
        self.assertTrue(receiver.closed)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["datagrams"], 2)
        self.assertEqual(report["frames"], 2)
        self.assertEqual(report["sequence_gaps"], 0)
        self.assertFalse(report["raw_payload_persisted"])
        self.assertEqual(report["interarrival"]["average_ms"], 1000.0)
        self.assertEqual(report["acceptance_limits"]["maximum_interarrival_ms"], 1000.0)
        self.assertEqual(report["heartbeat"]["frames"], 2)
        self.assertEqual(report["heartbeat"]["interarrival"]["maximum_ms"], 1000.0)
        self.assertEqual(report["burst_analysis"]["bursts"], 2)

    def test_source_contains_no_transmit_call(self) -> None:
        source = SCRIPT.read_text(encoding="utf-8")
        for forbidden in (".send(", ".sendto(", ".sendmsg(", "SOCK_DGRAM"):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
