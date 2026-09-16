#!/usr/bin/env python3
"""Collect a read-only Blackbox MAVLink observer acceptance report."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess

from analyze_mavlink_observer_soak import analyze, parse_lines


SCHEMA = "efis-next.blackbox-observer-acceptance.v1"


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, text=True, capture_output=True)


def properties(text: str) -> dict[str, str]:
    return dict(
        line.split("=", 1)
        for line in text.splitlines()
        if "=" in line
    )


def parse_temperature(text: str) -> float | None:
    match = re.search(r"temp=([-+]?\d+(?:\.\d+)?)'C", text)
    return float(match.group(1)) if match else None


def parse_throttled(text: str) -> int | None:
    match = re.search(r"throttled=(0x[0-9a-fA-F]+)", text)
    return int(match.group(1), 16) if match else None


def parse_integer(value: str | None) -> int | None:
    try:
        return int(value or "")
    except ValueError:
        return None


def memory_available_bytes(path: Path = Path("/proc/meminfo")) -> int | None:
    try:
        for line in path.read_text(encoding="utf-8").splitlines():
            if line.startswith("MemAvailable:"):
                return int(line.split()[1]) * 1024
    except (OSError, ValueError, IndexError):
        return None
    return None


def process_rss_bytes(pid: int, proc_root: Path = Path("/proc")) -> int | None:
    if pid <= 0:
        return None
    try:
        for line in (proc_root / str(pid) / "status").read_text(
            encoding="utf-8"
        ).splitlines():
            if line.startswith("VmRSS:"):
                return int(line.split()[1]) * 1024
    except (OSError, ValueError, IndexError):
        return None
    return None


def resource_report(
    *,
    service: dict[str, str],
    temperature_c: float | None,
    throttled_flags: int | None,
    available_memory_bytes: int | None,
    broker_rss_bytes: int | None,
    max_temperature_c: float,
    min_available_memory_bytes: int,
) -> dict[str, object]:
    main_pid = parse_integer(service.get("MainPID"))
    checks = {
        "service_running": service.get("ActiveState") == "active"
        and service.get("SubState") == "running",
        "service_zero_restarts": service.get("NRestarts") == "0",
        "broker_pid_valid": main_pid is not None and main_pid > 0,
        "temperature_available": temperature_c is not None,
        "temperature_below_limit": temperature_c is not None
        and temperature_c < max_temperature_c,
        "throttling_available": throttled_flags is not None,
        "throttling_clear": throttled_flags == 0,
        "memory_available": available_memory_bytes is not None,
        "memory_above_minimum": available_memory_bytes is not None
        and available_memory_bytes >= min_available_memory_bytes,
        "broker_rss_available": broker_rss_bytes is not None,
    }
    return {
        "status": "PASS" if all(checks.values()) else "FAIL",
        "checks": checks,
        "service": service,
        "temperature_c": temperature_c,
        "max_temperature_c": max_temperature_c,
        "throttled_flags": throttled_flags,
        "available_memory_bytes": available_memory_bytes,
        "minimum_available_memory_bytes": min_available_memory_bytes,
        "broker_rss_bytes": broker_rss_bytes,
    }


def write_report(path: Path, report: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration-seconds", type=float, default=600)
    parser.add_argument("--minimum-mavlink-hz", type=float, default=5)
    parser.add_argument("--minimum-sample-coverage", type=float, default=0.9)
    parser.add_argument("--max-temperature-c", type=float, default=80)
    parser.add_argument("--minimum-available-memory-mib", type=int, default=256)
    parser.add_argument("--observer", action="append", required=True)
    parser.add_argument("--display", action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    journal = run(
        [
            "journalctl",
            "-u",
            "efis-blackbox-broker.service",
            "--since",
            f"{args.duration_seconds:g} seconds ago",
            "-o",
            "cat",
            "--no-pager",
        ]
    )
    systemd = run(
        [
            "systemctl",
            "show",
            "efis-blackbox-broker.service",
            "-p",
            "ActiveState",
            "-p",
            "SubState",
            "-p",
            "MainPID",
            "-p",
            "NRestarts",
            "--no-pager",
        ]
    )
    service = properties(systemd.stdout)
    main_pid = parse_integer(service.get("MainPID")) or 0
    temperature = run(["vcgencmd", "measure_temp"])
    throttling = run(["vcgencmd", "get_throttled"])
    soak = analyze(
        parse_lines(journal.stdout.splitlines()),
        duration_seconds=args.duration_seconds,
        observers=args.observer,
        displays=args.display,
        minimum_mavlink_hz=args.minimum_mavlink_hz,
        minimum_sample_coverage=args.minimum_sample_coverage,
    )
    resources = resource_report(
        service=service,
        temperature_c=parse_temperature(temperature.stdout),
        throttled_flags=parse_throttled(throttling.stdout),
        available_memory_bytes=memory_available_bytes(),
        broker_rss_bytes=process_rss_bytes(main_pid),
        max_temperature_c=args.max_temperature_c,
        min_available_memory_bytes=args.minimum_available_memory_mib * 1024 * 1024,
    )
    collection = {
        "journal_read": journal.returncode == 0,
        "systemd_read": systemd.returncode == 0,
        "temperature_read": temperature.returncode == 0,
        "throttling_read": throttling.returncode == 0,
    }
    passed = (
        all(collection.values())
        and soak["status"] == "PASS"
        and resources["status"] == "PASS"
    )
    report = {
        "schema": SCHEMA,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "status": "PASS" if passed else "FAIL",
        "collection": collection,
        "soak": soak,
        "resources": resources,
    }
    write_report(args.output, report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
