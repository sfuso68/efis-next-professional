#!/usr/bin/env python3
"""Build deterministic EDP V1 bench scenarios and optionally send them."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, replace
from datetime import datetime, timezone
import hashlib
import ipaddress
import json
import math
from pathlib import Path
import socket
import struct
import sys
import time
from typing import Callable


sys.path.insert(0, str(Path(__file__).resolve().parent))
import capture_edp_observer as edp  # noqa: E402


SCHEMA = "efis-next.edp-replay-session.v1"
DEFAULT_PUBLISHER = 0x454649535245504C
MAX_FRAMES = 6_000
MAX_RATE_HZ = 50.0
MAX_PAUSE_SECONDS = 5.0
SCENARIOS = frozenset((
    "clean", "gap", "duplicate", "reversed", "restart", "bad-crc",
    "wrong-source", "mixed-provenance", "real-only", "loss-recovery",
))
BENCH_NETWORKS = tuple(ipaddress.ip_network(value) for value in (
    "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16",
))


class ReplayError(ValueError):
    pass


@dataclass(frozen=True)
class Frame:
    sequence: int
    publisher_instance: int
    source_node: int
    simulated_samples: int = edp.SAMPLE_COUNT
    corrupt_crc: bool = False
    pause_before_seconds: float = 0.0


def checked_int(value: object, name: str, minimum: int, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise ReplayError(f"{name} must be between {minimum} and {maximum}")
    return value


def checked_float(value: object, name: str, minimum: float, maximum: float) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ReplayError(f"{name} must be numeric")
    number = float(value)
    if not math.isfinite(number) or not minimum <= number <= maximum:
        raise ReplayError(f"{name} must be between {minimum} and {maximum}")
    return number


def validate_target(address: str, *, send: bool, allow_bench_network: bool) -> str:
    try:
        target = ipaddress.ip_address(address)
    except ValueError as error:
        raise ReplayError("target must be a numeric IP address") from error
    if target.version != 4 or target.is_unspecified or target.is_multicast:
        raise ReplayError("target must be a unicast IPv4 address")
    if send and not target.is_loopback:
        if not allow_bench_network:
            raise ReplayError("non-loopback transmission requires --allow-bench-network")
        if not any(target in network for network in BENCH_NETWORKS):
            raise ReplayError("bench transmission is restricted to RFC1918 IPv4 addresses")
        if int(target) & 0xFF in (0, 255):
            raise ReplayError("network and broadcast-like targets are forbidden")
    return str(target)


def sample_payload(simulated_samples: int) -> bytes:
    checked_int(simulated_samples, "simulated_samples", 0, edp.SAMPLE_COUNT)
    signals = sorted(edp.EXPECTED_SIGNALS)
    payload = bytearray((0, 0)) + len(signals).to_bytes(2, "big")
    for index, signal in enumerate(signals):
        simulated = index < simulated_samples
        value = float(signal)
        payload += signal.to_bytes(2, "big")
        payload += struct.pack(">d", value)
        payload += (1).to_bytes(2, "big")
        payload += bytes((0, 2, edp.EXPECTED_UNITS[signal]))
        payload += (0).to_bytes(4, "big")
        payload += bytes((simulated,))
        payload += (47).to_bytes(2, "big")
        payload += (index + 1).to_bytes(2, "big")
        payload += (1).to_bytes(4, "big")
        payload += struct.pack(">d", value)
    if len(payload) != edp.PAYLOAD_SIZE:
        raise AssertionError("EDP payload encoder size mismatch")
    return bytes(payload)


def encode_frame(frame: Frame, monotonic_us: int) -> bytes:
    checked_int(frame.source_node, "source_node", 1, 0xFFFFFFFF)
    checked_int(frame.publisher_instance, "publisher_instance", 1, 0xFFFFFFFFFFFFFFFF)
    checked_int(frame.sequence, "sequence", 1, 0xFFFFFFFFFFFFFFFF)
    checked_int(monotonic_us, "monotonic_us", 0, 0xFFFFFFFFFFFFFFFF)
    payload = sample_payload(frame.simulated_samples)
    crc = edp.crc32c(payload) ^ (1 if frame.corrupt_crc else 0)
    return (b"EDP1" + (1).to_bytes(2, "big") +
            frame.source_node.to_bytes(4, "big") +
            frame.publisher_instance.to_bytes(8, "big") +
            frame.sequence.to_bytes(8, "big") +
            monotonic_us.to_bytes(8, "big") +
            len(payload).to_bytes(4, "big") + crc.to_bytes(4, "big") + payload)


def scenario_frames(name: str, count: int, source_node: int,
                    publisher_instance: int, pause_seconds: float) -> list[Frame]:
    if name not in SCENARIOS:
        raise ReplayError(f"unsupported scenario: {name}")
    checked_int(count, "frames", 4, MAX_FRAMES)
    checked_int(source_node, "source_node", 1, 0xFFFFFFFF)
    checked_int(publisher_instance, "publisher_instance", 1, 0xFFFFFFFFFFFFFFFF)
    checked_float(pause_seconds, "pause_seconds", 0.0, MAX_PAUSE_SECONDS)
    frames = [Frame(i + 1, publisher_instance, source_node) for i in range(count)]
    pivot = count // 2
    if name == "gap":
        for index in range(pivot, count):
            frames[index] = replace(frames[index], sequence=frames[index].sequence + 1)
    elif name == "duplicate":
        frames[pivot] = replace(frames[pivot], sequence=frames[pivot - 1].sequence)
        for index in range(pivot + 1, count):
            frames[index] = replace(frames[index], sequence=frames[index].sequence - 1)
    elif name == "reversed":
        frames[pivot] = replace(frames[pivot], sequence=frames[pivot - 1].sequence - 1)
        for index in range(pivot + 1, count):
            frames[index] = replace(frames[index], sequence=frames[index].sequence - 1)
    elif name == "restart":
        next_publisher = publisher_instance + 1
        if next_publisher > 0xFFFFFFFFFFFFFFFF:
            raise ReplayError("publisher_instance cannot be incremented for restart")
        for index in range(pivot, count):
            frames[index] = replace(frames[index], publisher_instance=next_publisher,
                                    sequence=index - pivot + 1)
    elif name == "bad-crc":
        frames[pivot] = replace(frames[pivot], corrupt_crc=True)
    elif name == "wrong-source":
        replacement = 48 if source_node != 48 else 49
        frames[pivot] = replace(frames[pivot], source_node=replacement)
    elif name == "mixed-provenance":
        frames[pivot] = replace(frames[pivot], simulated_samples=edp.SAMPLE_COUNT // 2)
    elif name == "real-only":
        frames = [replace(frame, simulated_samples=0) for frame in frames]
    elif name == "loss-recovery":
        frames[pivot] = replace(frames[pivot], pause_before_seconds=pause_seconds)
    return frames


def expected_observation(name: str) -> dict[str, object]:
    return {
        "structural_rejection": "CRC_MISMATCH" if name == "bad-crc" else None,
        "identity_rejection": name == "wrong-source",
        "sequence_gap": name == "gap",
        "duplicate": name == "duplicate",
        "reversed": name == "reversed",
        "publisher_restart": name == "restart",
        "stale_lost_recovery": name == "loss-recovery",
        "feed_classification": (
            "REAL_ONLY" if name == "real-only" else
            "MIXED" if name == "mixed-provenance" else "SIM_ONLY"
        ),
    }


def wire_digest(frames: list[Frame], rate_hz: float) -> str:
    digest = hashlib.sha256()
    step_us = max(1, round(1_000_000.0 / rate_hz))
    for index, frame in enumerate(frames):
        digest.update(encode_frame(frame, 1_000_000 + index * step_us))
    return digest.hexdigest()


def schedule_digest(frames: list[Frame], rate_hz: float) -> str:
    schedule = {
        "rate_hz": rate_hz,
        "frames": [{
            "sequence": frame.sequence,
            "publisher_instance": frame.publisher_instance,
            "source_node": frame.source_node,
            "simulated_samples": frame.simulated_samples,
            "corrupt_crc": frame.corrupt_crc,
            "pause_before_seconds": frame.pause_before_seconds,
        } for frame in frames],
    }
    canonical = json.dumps(schedule, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def build_manifest(*, scenario: str, frames: list[Frame], rate_hz: float,
                   target: str, port: int, sent: int, transmission: str) -> dict[str, object]:
    return {
        "schema": SCHEMA,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "scenario": scenario,
        "target": {"address": target, "port": port},
        "frame_count": len(frames),
        "rate_hz": rate_hz,
        "transmission": transmission,
        "frames_sent": sent,
        "wire_sha256": wire_digest(frames, rate_hz),
        "schedule_sha256": schedule_digest(frames, rate_hz),
        "scheduled_duration_seconds": round(
            max(0, len(frames) - 1) / rate_hz +
            sum(frame.pause_before_seconds for frame in frames), 6),
        "maximum_pause_seconds": max(
            (frame.pause_before_seconds for frame in frames), default=0.0),
        "expected_observation": expected_observation(scenario),
        "raw_payload_persisted": False,
        "flight_approval": False,
    }


def send_frames(frames: list[Frame], target: str, port: int, rate_hz: float, *,
                socket_factory: Callable[..., socket.socket] = socket.socket,
                sleep: Callable[[float], None] = time.sleep) -> int:
    sender = socket_factory(socket.AF_INET, socket.SOCK_DGRAM)
    sent = 0
    step_us = max(1, round(1_000_000.0 / rate_hz))
    try:
        for index, frame in enumerate(frames):
            if index:
                sleep(1.0 / rate_hz)
            if frame.pause_before_seconds:
                sleep(frame.pause_before_seconds)
            wire = encode_frame(frame, 1_000_000 + index * step_us)
            if sender.sendto(wire, (target, port)) != len(wire):
                raise OSError("partial UDP datagram transmission")
            sent += 1
    finally:
        sender.close()
    return sent


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=sorted(SCENARIOS), default="clean")
    parser.add_argument("--frames", type=int, default=40)
    parser.add_argument("--rate-hz", type=float, default=20.0)
    parser.add_argument("--source-node", type=int, default=47)
    parser.add_argument("--publisher-instance", type=int, default=DEFAULT_PUBLISHER)
    parser.add_argument("--target", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=49003)
    parser.add_argument("--loss-pause-seconds", type=float, default=1.25)
    parser.add_argument("--send", action="store_true")
    parser.add_argument("--allow-bench-network", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        rate_hz = checked_float(args.rate_hz, "rate_hz", 0.1, MAX_RATE_HZ)
        port = checked_int(args.port, "port", 1, 65535)
        target = validate_target(args.target, send=args.send,
                                 allow_bench_network=args.allow_bench_network)
        frames = scenario_frames(args.scenario, args.frames, args.source_node,
                                 args.publisher_instance, args.loss_pause_seconds)
        sent = send_frames(frames, target, port, rate_hz) if args.send else 0
        manifest = build_manifest(
            scenario=args.scenario, frames=frames, rate_hz=rate_hz,
            target=target, port=port, sent=sent,
            transmission="SENT" if args.send else "DRY_RUN",
        )
        if args.output is not None:
            edp.write_report(args.output, manifest)
        print(json.dumps(manifest, indent=2, sort_keys=True))
        return 0
    except (OSError, ReplayError) as error:
        print(f"replay failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
