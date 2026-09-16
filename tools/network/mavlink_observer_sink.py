#!/usr/bin/env python3
"""Receive a monitor-only MAVLink2 copy and publish bounded health metrics.

The sink deliberately has no network transmit operation.  It validates the
configured source address and MAVLink2 frame boundaries, then writes aggregate
health only; raw flight data is never persisted by this process.
"""

from __future__ import annotations

import argparse
from collections.abc import Callable
from dataclasses import asdict, dataclass
import errno
import ipaddress
import json
import os
from pathlib import Path
import signal
import socket
import tempfile
import time


MAVLINK2_MAGIC = 0xFD
MAVLINK2_SIGNED_FLAG = 0x01
MAVLINK2_HEADER_BYTES = 10
MAVLINK2_CHECKSUM_BYTES = 2
MAVLINK2_SIGNATURE_BYTES = 13


class FrameError(ValueError):
    pass


def parse_mavlink2_datagram(datagram: bytes) -> list[tuple[int, int, int, int]]:
    """Return ``(system, component, message_id, sequence)`` for every frame.

    This is a structural boundary check, not dialect-aware CRC validation.
    The Blackbox remains the authoritative parser and recorder.
    """

    if not datagram:
        raise FrameError("empty datagram")

    frames: list[tuple[int, int, int, int]] = []
    offset = 0
    while offset < len(datagram):
        remaining = len(datagram) - offset
        if remaining < MAVLINK2_HEADER_BYTES + MAVLINK2_CHECKSUM_BYTES:
            raise FrameError("truncated MAVLink2 header")
        if datagram[offset] != MAVLINK2_MAGIC:
            raise FrameError("unexpected MAVLink magic")

        payload_length = datagram[offset + 1]
        incompatibility_flags = datagram[offset + 2]
        unsupported_flags = incompatibility_flags & ~MAVLINK2_SIGNED_FLAG
        if unsupported_flags:
            raise FrameError("unsupported MAVLink2 incompatibility flags")

        signature_length = (
            MAVLINK2_SIGNATURE_BYTES
            if incompatibility_flags & MAVLINK2_SIGNED_FLAG
            else 0
        )
        frame_length = (
            MAVLINK2_HEADER_BYTES
            + payload_length
            + MAVLINK2_CHECKSUM_BYTES
            + signature_length
        )
        if frame_length > remaining:
            raise FrameError("truncated MAVLink2 frame")

        sequence = datagram[offset + 4]
        system_id = datagram[offset + 5]
        component_id = datagram[offset + 6]
        message_id = int.from_bytes(datagram[offset + 7:offset + 10], "little")
        if system_id == 0:
            raise FrameError("invalid MAVLink system id")
        frames.append((system_id, component_id, message_id, sequence))
        offset += frame_length

    return frames


@dataclass
class ObserverHealth:
    schema: str = "efis-next.mavlink-observer-health.v1"
    mode: str = "RX_ONLY_NO_RETURN_PATH"
    status: str = "STARTING"
    bind: str = ""
    allowed_source: str = ""
    datagrams: int = 0
    bytes: int = 0
    frames: int = 0
    heartbeats: int = 0
    malformed: int = 0
    source_rejected: int = 0
    socket_errors: int = 0
    bind_retries: int = 0
    started_unix_ns: int = 0
    last_datagram_unix_ns: int = 0
    updated_unix_ns: int = 0

    def accept(self, datagram: bytes, frames: list[tuple[int, int, int, int]]) -> None:
        now = time.time_ns()
        self.datagrams += 1
        self.bytes += len(datagram)
        self.frames += len(frames)
        self.heartbeats += sum(1 for frame in frames if frame[2] == 0)
        self.last_datagram_unix_ns = now
        self.updated_unix_ns = now
        self.status = "LIVE"

    def reject_source(self) -> None:
        self.source_rejected += 1
        self.updated_unix_ns = time.time_ns()

    def reject_frame(self) -> None:
        self.malformed += 1
        self.updated_unix_ns = time.time_ns()

    def refresh(self, stale_after_seconds: float) -> None:
        now = time.time_ns()
        self.updated_unix_ns = now
        if not self.last_datagram_unix_ns:
            self.status = "WAITING"
        elif now - self.last_datagram_unix_ns > int(stale_after_seconds * 1e9):
            self.status = "STALE"
        else:
            self.status = "LIVE"


def write_health(path: Path, health: ObserverHealth) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(asdict(health), indent=2, sort_keys=True) + "\n"
    descriptor, temporary_name = tempfile.mkstemp(
        dir=path.parent, prefix=f".{path.name}.", suffix=".tmp"
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def validated_ipv4(value: str, label: str) -> str:
    address = ipaddress.ip_address(value)
    if not isinstance(address, ipaddress.IPv4Address) or address.is_unspecified:
        raise argparse.ArgumentTypeError(f"{label} must be a specific IPv4 address")
    return str(address)


def open_receiver(
    bind_address: str,
    port: int,
    wait_seconds: float,
    *,
    socket_factory: Callable[[int, int], socket.socket] = socket.socket,
    monotonic: Callable[[], float] = time.monotonic,
    sleeper: Callable[[float], None] = time.sleep,
) -> tuple[socket.socket, int]:
    """Bind once the configured local address exists, or fail at the deadline."""

    deadline = monotonic() + wait_seconds
    retries = 0
    while True:
        receiver = socket_factory(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            receiver.bind((bind_address, port))
            return receiver, retries
        except OSError as exc:
            receiver.close()
            if exc.errno != errno.EADDRNOTAVAIL or monotonic() >= deadline:
                raise
            retries += 1
            sleeper(0.25)


def run(args: argparse.Namespace) -> int:
    bind_address = validated_ipv4(args.bind, "bind")
    allowed_source = validated_ipv4(args.source, "source")
    if bind_address == allowed_source:
        raise ValueError("bind and allowed source must differ")

    health = ObserverHealth(
        bind=f"{bind_address}:{args.port}",
        allowed_source=allowed_source,
        started_unix_ns=time.time_ns(),
        updated_unix_ns=time.time_ns(),
    )
    health_path = Path(args.health)
    stop_requested = False

    def request_stop(_signum: int, _frame: object) -> None:
        nonlocal stop_requested
        stop_requested = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    write_health(health_path, health)
    receiver, health.bind_retries = open_receiver(
        bind_address, args.port, args.bind_wait
    )
    receiver.settimeout(min(1.0, args.health_interval))
    next_health = time.monotonic()
    next_log = next_health

    try:
        while not stop_requested:
            try:
                datagram, source = receiver.recvfrom(65_535)
            except socket.timeout:
                datagram = b""
                source = ("", 0)
            except OSError:
                health.socket_errors += 1
                health.updated_unix_ns = time.time_ns()
                raise

            if datagram:
                if source[0] != allowed_source:
                    health.reject_source()
                else:
                    try:
                        frames = parse_mavlink2_datagram(datagram)
                    except FrameError:
                        health.reject_frame()
                    else:
                        health.accept(datagram, frames)

            now = time.monotonic()
            if now >= next_health:
                health.refresh(args.stale_after)
                write_health(health_path, health)
                next_health = now + args.health_interval
            if now >= next_log:
                print(json.dumps(asdict(health), sort_keys=True), flush=True)
                next_log = now + args.log_interval
    finally:
        receiver.close()
        health.refresh(args.stale_after)
        write_health(health_path, health)
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--bind", default="192.168.1.6")
    result.add_argument("--port", type=int, default=14550)
    result.add_argument("--source", default="192.168.1.47")
    result.add_argument(
        "--health", default="/var/lib/efis-mavlink-observer/health.json"
    )
    result.add_argument("--health-interval", type=float, default=5.0)
    result.add_argument("--log-interval", type=float, default=60.0)
    result.add_argument("--stale-after", type=float, default=3.0)
    result.add_argument("--bind-wait", type=float, default=60.0)
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if not 1 <= args.port <= 65_535:
        raise SystemExit("port must be in range 1..65535")
    if (
        args.health_interval <= 0
        or args.log_interval <= 0
        or args.stale_after <= 0
        or args.bind_wait < 0
    ):
        raise SystemExit("intervals must be positive and bind wait non-negative")
    try:
        return run(args)
    except (OSError, ValueError, argparse.ArgumentTypeError) as exc:
        raise SystemExit(f"mavlink observer sink failed: {exc}") from exc


if __name__ == "__main__":
    raise SystemExit(main())
