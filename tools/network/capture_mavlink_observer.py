#!/usr/bin/env python3
"""Passively measure MAVLink2 delivery without binding QGC's UDP port."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import ipaddress
import json
import math
import os
from pathlib import Path
import re
import socket
import statistics
import sys
import tempfile
import time
from typing import Callable


sys.path.insert(0, str(Path(__file__).resolve().parent))
from mavlink_observer_sink import FrameError, parse_mavlink2_datagram  # noqa: E402


SCHEMA = "efis-next.mavlink-passive-capture.v1"
MODE = "PASSIVE_AF_PACKET_NO_RETURN_PATH"
ETHERNET_HEADER_BYTES = 14
ETHERTYPE_IPV4 = 0x0800
VLAN_ETHERTYPES = {0x8100, 0x88A8}
UDP_PROTOCOL = 17
DEFAULT_MAX_INTERARRIVAL_MS = 1000.0
DEFAULT_MAX_P95_INTERARRIVAL_MS = 100.0


class PacketError(ValueError):
    pass


@dataclass(frozen=True)
class Endpoint:
    source: str
    destination: str
    port: int


@dataclass
class StreamState:
    system_id: int
    component_id: int
    frames: int = 0
    gaps: int = 0
    duplicates: int = 0
    out_of_order: int = 0
    first_sequence: int | None = None
    last_sequence: int | None = None

    def observe(self, sequence: int) -> None:
        self.frames += 1
        if self.first_sequence is None:
            self.first_sequence = sequence
            self.last_sequence = sequence
            return
        assert self.last_sequence is not None
        delta = (sequence - self.last_sequence) & 0xFF
        if delta == 0:
            self.duplicates += 1
            return
        if delta <= 127:
            self.gaps += delta - 1
            self.last_sequence = sequence
            return
        self.out_of_order += 1


def validated_ipv4(value: str, label: str) -> str:
    address = ipaddress.ip_address(value)
    if not isinstance(address, ipaddress.IPv4Address) or address.is_unspecified:
        raise argparse.ArgumentTypeError(f"{label} must be a specific IPv4 address")
    return str(address)


def validated_interface(value: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9_.:-]{1,15}", value):
        raise argparse.ArgumentTypeError("invalid interface name")
    return value


def decode_udp(packet: bytes) -> tuple[str, str, int, int, bytes] | None:
    """Decode one Ethernet/IPv4/UDP frame, ignoring unrelated EtherTypes."""
    if len(packet) < ETHERNET_HEADER_BYTES:
        return None
    offset = ETHERNET_HEADER_BYTES
    ether_type = int.from_bytes(packet[12:14], "big")
    vlan_depth = 0
    while ether_type in VLAN_ETHERTYPES:
        if len(packet) < offset + 4:
            raise PacketError("truncated VLAN header")
        ether_type = int.from_bytes(packet[offset + 2:offset + 4], "big")
        offset += 4
        vlan_depth += 1
        if vlan_depth > 2:
            raise PacketError("excessive VLAN nesting")
    if ether_type != ETHERTYPE_IPV4:
        return None
    if len(packet) < offset + 20:
        raise PacketError("truncated IPv4 header")
    version_ihl = packet[offset]
    if version_ihl >> 4 != 4:
        raise PacketError("invalid IPv4 version")
    ip_header_bytes = (version_ihl & 0x0F) * 4
    if ip_header_bytes < 20 or len(packet) < offset + ip_header_bytes:
        raise PacketError("invalid IPv4 header length")
    total_length = int.from_bytes(packet[offset + 2:offset + 4], "big")
    if total_length < ip_header_bytes + 8 or len(packet) < offset + total_length:
        raise PacketError("invalid IPv4 total length")
    if packet[offset + 9] != UDP_PROTOCOL:
        return None
    source = socket.inet_ntoa(packet[offset + 12:offset + 16])
    destination = socket.inet_ntoa(packet[offset + 16:offset + 20])
    fragment = int.from_bytes(packet[offset + 6:offset + 8], "big")
    if fragment & 0x3FFF:
        raise PacketError("fragmented observer datagram")
    udp_offset = offset + ip_header_bytes
    source_port = int.from_bytes(packet[udp_offset:udp_offset + 2], "big")
    destination_port = int.from_bytes(packet[udp_offset + 2:udp_offset + 4], "big")
    udp_length = int.from_bytes(packet[udp_offset + 4:udp_offset + 6], "big")
    if udp_length < 8 or udp_length > total_length - ip_header_bytes:
        raise PacketError("invalid UDP length")
    payload = packet[udp_offset + 8:udp_offset + udp_length]
    return source, destination, source_port, destination_port, payload


def observer_payload(packet: bytes, endpoint: Endpoint) -> bytes | None:
    decoded = decode_udp(packet)
    if decoded is None:
        return None
    source, destination, _source_port, destination_port, payload = decoded
    if (
        source != endpoint.source
        or destination != endpoint.destination
        or destination_port != endpoint.port
    ):
        return None
    return payload


def percentile(values: list[float], probability: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = max(0, math.ceil(probability * len(ordered)) - 1)
    return ordered[index]


def ceil_milliseconds(value: float) -> float:
    """Keep threshold comparisons fail-closed at report precision."""
    return math.ceil(value * 1000.0) / 1000.0


def timing_report(intervals_ms: list[float]) -> dict[str, float | int | None]:
    if not intervals_ms:
        return {
            "samples": 0,
            "average_ms": None,
            "maximum_ms": None,
            "p95_ms": None,
            "mdev_ms": None,
        }
    return {
        "samples": len(intervals_ms),
        "average_ms": round(statistics.fmean(intervals_ms), 3),
        "maximum_ms": ceil_milliseconds(max(intervals_ms)),
        "p95_ms": ceil_milliseconds(percentile(intervals_ms, 0.95) or 0.0),
        "mdev_ms": round(statistics.pstdev(intervals_ms), 3),
    }


def evaluate(report: dict[str, object], args: argparse.Namespace) -> dict[str, bool]:
    frames = int(report["frames"])
    gaps = int(report["sequence_gaps"])
    denominator = frames + gaps
    gap_percent = (gaps * 100.0 / denominator) if denominator else 100.0
    report["sequence_gap_percent"] = round(gap_percent, 6)
    timing = report["interarrival"]
    assert isinstance(timing, dict)
    timing_samples = int(timing["samples"])
    maximum_ms = timing["maximum_ms"]
    p95_ms = timing["p95_ms"]
    return {
        "duration_covered": float(report["actual_duration_seconds"])
        >= args.duration_seconds * 0.95,
        "minimum_datagram_rate": float(report["datagram_hz"]) >= args.minimum_hz,
        "frames_present": frames > 0,
        "malformed_within_limit": int(report["malformed"]) <= args.max_malformed,
        "sequence_gap_within_limit": gap_percent <= args.max_gap_percent,
        "duplicates_within_limit": int(report["duplicates"]) <= args.max_duplicates,
        "out_of_order_within_limit": int(report["out_of_order"])
        <= args.max_out_of_order,
        "timing_samples_present": timing_samples > 0,
        "maximum_interarrival_within_limit": maximum_ms is not None
        and float(maximum_ms) <= args.max_interarrival_ms,
        "p95_interarrival_within_limit": p95_ms is not None
        and float(p95_ms) <= args.max_p95_interarrival_ms,
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
    last_datagram_ns: int | None = None
    intervals_ms: list[float] = []
    streams: dict[tuple[int, int], StreamState] = {}
    packets_seen = datagrams = frames = malformed = socket_errors = 0
    ignored = bytes_observed = 0
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
                parsed = parse_mavlink2_datagram(payload)
            except (PacketError, FrameError):
                malformed += 1
                continue
            now_ns = monotonic_ns()
            if last_datagram_ns is not None:
                intervals_ms.append((now_ns - last_datagram_ns) / 1e6)
            last_datagram_ns = now_ns
            datagrams += 1
            bytes_observed += len(payload)
            frames += len(parsed)
            for system_id, component_id, _message_id, sequence in parsed:
                key = (system_id, component_id)
                streams.setdefault(
                    key, StreamState(system_id=system_id, component_id=component_id)
                ).observe(sequence)
    finally:
        receiver.close()
    finished_ns = monotonic_ns()
    actual_duration = max(0.0, (finished_ns - started_ns) / 1e9)
    sequence_gaps = sum(stream.gaps for stream in streams.values())
    duplicates = sum(stream.duplicates for stream in streams.values())
    out_of_order = sum(stream.out_of_order for stream in streams.values())
    report: dict[str, object] = {
        "schema": SCHEMA,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "mode": MODE,
        "interface": args.interface,
        "endpoint": asdict(endpoint),
        "requested_duration_seconds": args.duration_seconds,
        "actual_duration_seconds": round(actual_duration, 6),
        "packets_seen": packets_seen,
        "packets_ignored": ignored,
        "datagrams": datagrams,
        "datagram_hz": round(datagrams / actual_duration, 6) if actual_duration else 0.0,
        "bytes": bytes_observed,
        "frames": frames,
        "malformed": malformed,
        "socket_errors": socket_errors,
        "sequence_gaps": sequence_gaps,
        "duplicates": duplicates,
        "out_of_order": out_of_order,
        "interarrival": timing_report(intervals_ms),
        "acceptance_limits": {
            "maximum_interarrival_ms": args.max_interarrival_ms,
            "maximum_p95_interarrival_ms": args.max_p95_interarrival_ms,
        },
        "streams": [asdict(streams[key]) for key in sorted(streams)],
        "raw_payload_persisted": False,
    }
    checks = evaluate(report, args)
    report["checks"] = checks
    report["status"] = "PASS" if all(checks.values()) else "FAIL"
    return report


def write_report(path: Path, report: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    descriptor, temporary_name = tempfile.mkstemp(
        dir=path.parent, prefix=f".{path.name}.", suffix=".tmp"
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interface", type=validated_interface, default="wlo1")
    parser.add_argument("--source", default="192.168.1.47")
    parser.add_argument("--destination", default="192.168.1.39")
    parser.add_argument("--port", type=int, default=14550)
    parser.add_argument("--duration-seconds", type=float, default=60.0)
    parser.add_argument("--minimum-hz", type=float, default=5.0)
    parser.add_argument("--max-gap-percent", type=float, default=0.1)
    parser.add_argument("--max-malformed", type=int, default=0)
    parser.add_argument("--max-duplicates", type=int, default=0)
    parser.add_argument("--max-out-of-order", type=int, default=0)
    parser.add_argument(
        "--max-interarrival-ms",
        type=float,
        default=DEFAULT_MAX_INTERARRIVAL_MS,
        help="fail when any accepted datagram interval exceeds this limit",
    )
    parser.add_argument(
        "--max-p95-interarrival-ms",
        type=float,
        default=DEFAULT_MAX_P95_INTERARRIVAL_MS,
        help="fail when the p95 accepted datagram interval exceeds this limit",
    )
    parser.add_argument("--output", default="-")
    args = parser.parse_args()
    args.source = validated_ipv4(args.source, "source")
    args.destination = validated_ipv4(args.destination, "destination")
    if not 1 <= args.port <= 65535:
        parser.error("port must be between 1 and 65535")
    if not 10 <= args.duration_seconds <= 600:
        parser.error("duration must be between 10 and 600 seconds")
    if args.minimum_hz <= 0:
        parser.error("minimum Hz must be positive")
    if not 0 <= args.max_gap_percent <= 100:
        parser.error("gap percentage must be between 0 and 100")
    if min(args.max_malformed, args.max_duplicates, args.max_out_of_order) < 0:
        parser.error("counter limits must not be negative")
    if args.max_interarrival_ms <= 0 or args.max_p95_interarrival_ms <= 0:
        parser.error("interarrival limits must be positive")
    if args.max_p95_interarrival_ms > args.max_interarrival_ms:
        parser.error("p95 interarrival limit must not exceed the maximum limit")
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
