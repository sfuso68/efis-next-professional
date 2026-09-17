#!/usr/bin/env python3
"""Passively validate Blackbox -> EMS EDP V1 without binding UDP 49003."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import socket
import sys
import time
from typing import Callable


sys.path.insert(0, str(Path(__file__).resolve().parent))
from capture_mavlink_observer import (  # noqa: E402
    Endpoint,
    PacketError,
    observer_payload,
    timing_report,
    validated_interface,
    validated_ipv4,
    write_report,
)


SCHEMA = "efis-next.edp-passive-capture.v1"
MODE = "PASSIVE_AF_PACKET_NO_RETURN_PATH"
ENVELOPE_SIZE = 42
SAMPLE_SIZE = 36
SAMPLE_COUNT = 18
PAYLOAD_SIZE = 4 + SAMPLE_COUNT * SAMPLE_SIZE
EXPECTED_SIGNALS = frozenset((*range(600, 610), *range(640, 644), *range(660, 664)))
EXPECTED_UNITS = {
    600: 8, 601: 9, 602: 10, 603: 11, 604: 10, 605: 13,
    606: 12, 607: 12, 608: 14, 609: 15,
    **{signal: 11 for signal in (*range(640, 644), *range(660, 664))},
}


class EdpError(ValueError):
    """An EDP datagram failed a fail-closed structural or integrity check."""


@dataclass(frozen=True)
class EdpEnvelope:
    source_node: int
    publisher_instance: int
    sequence: int
    monotonic_us: int
    payload_length: int
    health: int
    fault: int
    simulated_samples: int


@dataclass
class PublisherStream:
    source_node: int
    publisher_instance: int
    frames: int = 0
    gaps: int = 0
    duplicates: int = 0
    reversed: int = 0
    first_sequence: int | None = None
    last_sequence: int | None = None
    current: bool = True

    def observe(self, sequence: int) -> None:
        self.frames += 1
        if self.first_sequence is None:
            self.first_sequence = sequence
            self.last_sequence = sequence
            return
        assert self.last_sequence is not None
        if not self.current or sequence < self.last_sequence:
            self.reversed += 1
        elif sequence == self.last_sequence:
            self.duplicates += 1
        else:
            self.gaps += sequence - self.last_sequence - 1
            self.last_sequence = sequence


def crc32c(payload: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in payload:
        crc ^= byte
        for _ in range(8):
            mask = -(crc & 1) & 0xFFFFFFFF
            crc = (crc >> 1) ^ (0x82F63B78 & mask)
    return (~crc) & 0xFFFFFFFF


def parse_edp_v1(datagram: bytes) -> EdpEnvelope:
    if len(datagram) < ENVELOPE_SIZE:
        raise EdpError("TRUNCATED")
    if datagram[:4] != b"EDP1":
        raise EdpError("BAD_MAGIC")
    if int.from_bytes(datagram[4:6], "big") != 1:
        raise EdpError("UNSUPPORTED_SCHEMA")
    source_node = int.from_bytes(datagram[6:10], "big")
    publisher = int.from_bytes(datagram[10:18], "big")
    sequence = int.from_bytes(datagram[18:26], "big")
    monotonic_us = int.from_bytes(datagram[26:34], "big")
    payload_length = int.from_bytes(datagram[34:38], "big")
    expected_crc = int.from_bytes(datagram[38:42], "big")
    if not source_node or not publisher or not sequence:
        raise EdpError("INVALID_IDENTITY")
    if payload_length != PAYLOAD_SIZE or len(datagram) != ENVELOPE_SIZE + payload_length:
        raise EdpError("LENGTH_MISMATCH")
    payload = datagram[ENVELOPE_SIZE:]
    if crc32c(payload) != expected_crc:
        raise EdpError("CRC_MISMATCH")
    health, fault = payload[0], payload[1]
    count = int.from_bytes(payload[2:4], "big")
    if health > 2 or fault > 4 or count != SAMPLE_COUNT:
        raise EdpError("MALFORMED_PAYLOAD")
    signals: set[int] = set()
    simulated = 0
    for index in range(count):
        offset = 4 + index * SAMPLE_SIZE
        signal = int.from_bytes(payload[offset:offset + 2], "big")
        if signal not in EXPECTED_SIGNALS or signal in signals:
            raise EdpError("MALFORMED_PAYLOAD")
        signals.add(signal)
        source = int.from_bytes(payload[offset + 10:offset + 12], "big")
        validity, quality, unit = payload[offset + 12:offset + 15]
        simulated_flag = payload[offset + 19]
        if (source > 8 or validity > 6 or quality > 3 or
                unit != EXPECTED_UNITS[signal] or simulated_flag > 1):
            raise EdpError("MALFORMED_PAYLOAD")
        simulated += simulated_flag
    if signals != EXPECTED_SIGNALS:
        raise EdpError("MALFORMED_PAYLOAD")
    return EdpEnvelope(source_node, publisher, sequence, monotonic_us,
                       payload_length, health, fault, simulated)


def observe_sequence(
    streams: dict[tuple[int, int], PublisherStream], envelope: EdpEnvelope
) -> int:
    key = (envelope.source_node, envelope.publisher_instance)
    stream = streams.get(key)
    restarts = 0
    if stream is None:
        for candidate in streams.values():
            if candidate.source_node == envelope.source_node and candidate.current:
                candidate.current = False
                restarts += 1
        stream = PublisherStream(*key)
        streams[key] = stream
    stream.observe(envelope.sequence)
    return restarts


def evaluate(report: dict[str, object], args: argparse.Namespace) -> dict[str, bool]:
    frames = int(report["frames"])
    gaps = int(report["sequence_gaps"])
    denominator = frames + gaps
    gap_percent = gaps * 100.0 / denominator if denominator else 100.0
    report["sequence_gap_percent"] = round(gap_percent, 6)
    timing = report["interarrival"]
    assert isinstance(timing, dict)
    return {
        "duration_covered": float(report["actual_duration_seconds"]) >= args.duration_seconds * 0.95,
        "minimum_datagram_rate": float(report["datagram_hz"]) >= args.minimum_hz,
        "frames_present": frames > 0,
        "malformed_within_limit": int(report["malformed"]) <= args.max_malformed,
        "identity_rejected_zero": int(report["identity_rejected"]) == 0,
        "sequence_gap_within_limit": gap_percent <= args.max_gap_percent,
        "duplicates_within_limit": int(report["duplicates"]) <= args.max_duplicates,
        "reversed_within_limit": int(report["reversed"]) <= args.max_reversed,
        "publisher_restarts_within_limit": int(report["publisher_restarts"]) <= args.max_publisher_restarts,
        "timing_samples_present": int(timing["samples"]) > 0,
        "maximum_interarrival_within_limit": timing["maximum_ms"] is not None and float(timing["maximum_ms"]) <= args.max_interarrival_ms,
        "p95_interarrival_within_limit": timing["p95_ms"] is not None and float(timing["p95_ms"]) <= args.max_p95_interarrival_ms,
        "socket_errors_zero": int(report["socket_errors"]) == 0,
    }


def capture(
    args: argparse.Namespace,
    *,
    socket_factory: Callable[..., socket.socket] = socket.socket,
    monotonic_ns: Callable[[], int] = time.monotonic_ns,
) -> dict[str, object]:
    af_packet = getattr(socket, "AF_PACKET", None)
    if af_packet is None:
        raise OSError("AF_PACKET is unavailable on this platform")
    receiver = socket_factory(af_packet, socket.SOCK_RAW, socket.htons(0x0003))
    receiver.bind((args.interface, 0))
    receiver.settimeout(0.25)
    endpoint = Endpoint(args.source, args.destination, args.port)
    started_ns = monotonic_ns()
    deadline_ns = started_ns + int(args.duration_seconds * 1e9)
    last_ns: int | None = None
    intervals: list[float] = []
    streams: dict[tuple[int, int], PublisherStream] = {}
    errors: dict[str, int] = {}
    packets_seen = ignored = frames = identity_rejected = socket_errors = 0
    bytes_observed = restarts = simulated_frames = real_frames = 0
    mixed_provenance_frames = simulated_samples = 0
    try:
        while monotonic_ns() < deadline_ns:
            try:
                packet = receiver.recv(65535)
                packets_seen += 1
            except socket.timeout:
                continue
            except OSError:
                socket_errors += 1
                break
            try:
                payload = observer_payload(packet, endpoint)
                if payload is None:
                    ignored += 1
                    continue
                envelope = parse_edp_v1(payload)
            except (PacketError, EdpError) as error:
                name = str(error)
                errors[name] = errors.get(name, 0) + 1
                continue
            now_ns = monotonic_ns()
            if envelope.source_node != args.expected_source_node:
                identity_rejected += 1
                continue
            if last_ns is not None:
                intervals.append((now_ns - last_ns) / 1e6)
            last_ns = now_ns
            frames += 1
            bytes_observed += len(payload)
            simulated_samples += envelope.simulated_samples
            if envelope.simulated_samples == SAMPLE_COUNT:
                simulated_frames += 1
            elif envelope.simulated_samples == 0:
                real_frames += 1
            else:
                mixed_provenance_frames += 1
            restarts += observe_sequence(streams, envelope)
    finally:
        receiver.close()
    finished_ns = monotonic_ns()
    actual_duration = max(0.0, (finished_ns - started_ns) / 1e9)
    report: dict[str, object] = {
        "schema": SCHEMA,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "mode": MODE,
        "interface": args.interface,
        "endpoint": asdict(endpoint),
        "expected_source_node": args.expected_source_node,
        "requested_duration_seconds": args.duration_seconds,
        "actual_duration_seconds": round(actual_duration, 6),
        "packets_seen": packets_seen,
        "packets_ignored": ignored,
        "frames": frames,
        "datagram_hz": round(frames / actual_duration, 6) if actual_duration else 0.0,
        "bytes": bytes_observed,
        "malformed": sum(errors.values()),
        "decode_errors": dict(sorted(errors.items())),
        "identity_rejected": identity_rejected,
        "sequence_gaps": sum(item.gaps for item in streams.values()),
        "duplicates": sum(item.duplicates for item in streams.values()),
        "reversed": sum(item.reversed for item in streams.values()),
        "publisher_restarts": restarts,
        "simulated_frames": simulated_frames,
        "real_frames": real_frames,
        "mixed_provenance_frames": mixed_provenance_frames,
        "simulated_samples": simulated_samples,
        "total_samples": frames * SAMPLE_COUNT,
        "interarrival": timing_report(intervals),
        "acceptance_limits": {
            "minimum_hz": args.minimum_hz,
            "maximum_gap_percent": args.max_gap_percent,
            "maximum_malformed": args.max_malformed,
            "maximum_duplicates": args.max_duplicates,
            "maximum_reversed": args.max_reversed,
            "maximum_publisher_restarts": args.max_publisher_restarts,
            "maximum_interarrival_ms": args.max_interarrival_ms,
            "maximum_p95_interarrival_ms": args.max_p95_interarrival_ms,
        },
        "streams": [asdict(streams[key]) for key in sorted(streams)],
        "socket_errors": socket_errors,
        "raw_payload_persisted": False,
        "flight_approval": False,
    }
    report["checks"] = evaluate(report, args)
    report["status"] = "PASS" if all(report["checks"].values()) else "FAIL"
    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interface", type=validated_interface, default="eth0")
    parser.add_argument("--source", default="10.77.10.2")
    parser.add_argument("--destination", default="10.77.10.12")
    parser.add_argument("--port", type=int, default=49003)
    parser.add_argument("--expected-source-node", type=int, default=47)
    parser.add_argument("--duration-seconds", type=float, default=60.0)
    parser.add_argument("--minimum-hz", type=float, default=10.0)
    parser.add_argument("--max-gap-percent", type=float, default=0.1)
    parser.add_argument("--max-malformed", type=int, default=0)
    parser.add_argument("--max-duplicates", type=int, default=0)
    parser.add_argument("--max-reversed", type=int, default=0)
    parser.add_argument("--max-publisher-restarts", type=int, default=0)
    parser.add_argument("--max-interarrival-ms", type=float, default=250.0)
    parser.add_argument("--max-p95-interarrival-ms", type=float, default=100.0)
    parser.add_argument("--output", default="-")
    args = parser.parse_args()
    args.source = validated_ipv4(args.source, "source")
    args.destination = validated_ipv4(args.destination, "destination")
    if not 1 <= args.port <= 65535 or not 1 <= args.expected_source_node <= 65535:
        parser.error("port and expected source node must be non-zero and in range")
    if not 10 <= args.duration_seconds <= 600 or args.minimum_hz <= 0:
        parser.error("duration must be 10..600 seconds and minimum Hz positive")
    if not 0 <= args.max_gap_percent <= 100:
        parser.error("gap percentage must be between 0 and 100")
    if min(args.max_malformed, args.max_duplicates, args.max_reversed,
           args.max_publisher_restarts) < 0:
        parser.error("counter limits must not be negative")
    if args.max_p95_interarrival_ms <= 0 or args.max_interarrival_ms < args.max_p95_interarrival_ms:
        parser.error("timing limits are invalid")
    return args


def main() -> int:
    args = parse_args()
    if os.name != "posix" or not hasattr(socket, "AF_PACKET"):
        print("capture requires Linux AF_PACKET", file=sys.stderr)
        return 69
    if os.geteuid() != 0:
        print("capture requires root or CAP_NET_RAW", file=sys.stderr)
        return 77
    try:
        report = capture(args)
    except OSError as error:
        print(f"capture failed: {error}", file=sys.stderr)
        return 1
    if args.output != "-":
        write_report(Path(args.output), report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
