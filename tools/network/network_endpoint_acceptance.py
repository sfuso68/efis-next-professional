#!/usr/bin/env python3
"""Measure bounded IPv4 endpoint loss and RTT variation without changing a network."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import ipaddress
import json
from pathlib import Path
import re
import subprocess
from typing import Callable


SCHEMA = "efis-next.network-endpoint-acceptance.v1"
ALLOWED_NETWORKS = (
    ipaddress.ip_network("10.77.10.0/24"),
    ipaddress.ip_network("192.168.1.0/24"),
)


@dataclass(frozen=True)
class Target:
    identifier: str
    address: str
    max_loss_percent: float
    max_average_ms: float
    max_rtt_mdev_ms: float


def parse_target(value: str) -> Target:
    try:
        identity, address, loss, average, jitter = value.split(",")
        parsed_address = ipaddress.ip_address(address)
        target = Target(
            identifier=identity,
            address=str(parsed_address),
            max_loss_percent=float(loss),
            max_average_ms=float(average),
            max_rtt_mdev_ms=float(jitter),
        )
    except (ValueError, TypeError) as error:
        raise argparse.ArgumentTypeError(
            "target must be ID,IPv4,MAX_LOSS_PERCENT,MAX_AVG_MS,MAX_RTT_MDEV_MS"
        ) from error
    if not target.identifier or not re.fullmatch(r"[A-Za-z0-9_.-]+", target.identifier):
        raise argparse.ArgumentTypeError("target ID contains unsupported characters")
    if parsed_address.version != 4 or not any(
        parsed_address in network for network in ALLOWED_NETWORKS
    ):
        raise argparse.ArgumentTypeError("target is outside approved bench networks")
    if min(
        target.max_loss_percent,
        target.max_average_ms,
        target.max_rtt_mdev_ms,
    ) < 0:
        raise argparse.ArgumentTypeError("target limits must not be negative")
    if target.max_loss_percent > 100:
        raise argparse.ArgumentTypeError("loss limit must not exceed 100 percent")
    return target


def parse_ping(text: str) -> dict[str, float | int]:
    packet = re.search(
        r"(\d+) packets transmitted, (\d+) received, "
        r"(?:[+]?\d+ errors, )?([0-9.]+)% packet loss",
        text,
    )
    timing = re.search(
        r"(?:rtt|round-trip) min/avg/max/(?:mdev|stddev) = "
        r"([0-9.]+)/([0-9.]+)/([0-9.]+)/([0-9.]+) ms",
        text,
    )
    if not packet or not timing:
        raise ValueError("incomplete Linux ping summary")
    return {
        "transmitted": int(packet.group(1)),
        "received": int(packet.group(2)),
        "loss_percent": float(packet.group(3)),
        "minimum_ms": float(timing.group(1)),
        "average_ms": float(timing.group(2)),
        "maximum_ms": float(timing.group(3)),
        "rtt_mdev_ms": float(timing.group(4)),
    }


def evaluate(target: Target, metrics: dict[str, float | int]) -> dict[str, object]:
    checks = {
        "all_probes_accounted": int(metrics["transmitted"]) == (
            int(metrics["received"])
            + round(int(metrics["transmitted"]) * float(metrics["loss_percent"]) / 100)
        ),
        "loss_within_limit": float(metrics["loss_percent"])
        <= target.max_loss_percent,
        "average_within_limit": float(metrics["average_ms"])
        <= target.max_average_ms,
        "rtt_mdev_within_limit": float(metrics["rtt_mdev_ms"])
        <= target.max_rtt_mdev_ms,
    }
    return {
        "id": target.identifier,
        "address": target.address,
        "status": "PASS" if all(checks.values()) else "FAIL",
        "limits": {
            "max_loss_percent": target.max_loss_percent,
            "max_average_ms": target.max_average_ms,
            "max_rtt_mdev_ms": target.max_rtt_mdev_ms,
        },
        "metrics": metrics,
        "checks": checks,
    }


def probe(
    target: Target,
    *,
    count: int,
    interval_seconds: float,
    timeout_seconds: int,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
) -> dict[str, object]:
    command = [
        "ping",
        "-n",
        "-q",
        "-c",
        str(count),
        "-i",
        f"{interval_seconds:g}",
        "-W",
        str(timeout_seconds),
        target.address,
    ]
    result = runner(command, check=False, text=True, capture_output=True)
    try:
        metrics = parse_ping(result.stdout + "\n" + result.stderr)
        report = evaluate(target, metrics)
    except ValueError as error:
        report = {
            "id": target.identifier,
            "address": target.address,
            "status": "FAIL",
            "error": str(error),
        }
    report["ping_exit_code"] = result.returncode
    if result.returncode not in (0, 1):
        report["status"] = "FAIL"
    return report


def write_report(path: Path, report: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", action="append", type=parse_target, required=True)
    parser.add_argument("--count", type=int, default=100)
    parser.add_argument("--interval-seconds", type=float, default=0.2)
    parser.add_argument("--timeout-seconds", type=int, default=1)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 5 <= args.count <= 1000:
        parser.error("count must be between 5 and 1000")
    if not 0.05 <= args.interval_seconds <= 2:
        parser.error("interval must be between 0.05 and 2 seconds")
    if not 1 <= args.timeout_seconds <= 10:
        parser.error("timeout must be between 1 and 10 seconds")
    identifiers = [target.identifier for target in args.target]
    addresses = [target.address for target in args.target]
    if len(set(identifiers)) != len(identifiers) or len(set(addresses)) != len(addresses):
        parser.error("target IDs and addresses must be unique")
    return args


def main() -> int:
    args = parse_args()
    targets = [
        probe(
            target,
            count=args.count,
            interval_seconds=args.interval_seconds,
            timeout_seconds=args.timeout_seconds,
        )
        for target in args.target
    ]
    report = {
        "schema": SCHEMA,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "mode": "READ_ONLY_ICMP_BENCH_PROBE",
        "status": "PASS" if all(item["status"] == "PASS" for item in targets) else "FAIL",
        "count_per_target": args.count,
        "interval_seconds": args.interval_seconds,
        "targets": targets,
    }
    write_report(args.output, report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
