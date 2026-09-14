#!/usr/bin/env python3
"""Fail-closed preflight for the EFIS QGroundControl observer.

The operational Ground Station receives a one-way UDP copy from the Blackbox.
It must not start when a local USB ACM serial device, the legacy USB bridge, or
another UDP/14550 consumer is present.  This script performs only local checks;
it does not create a connection or send a MAVLink datagram.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import glob
import json
from pathlib import Path
import socket
import subprocess
import sys
from typing import Callable


MODE = "MONITOR_ONLY_NO_PIXHAWK_RETURN"
DEFAULT_LEGACY_UNIT = "efis-mavlink-readonly-bridge.service"
BLOCKED_EXIT = 78  # EX_CONFIG: operator action is required; do not restart-loop.


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def discover_serial_devices(
    *,
    acm_pattern: str = "/dev/ttyACM*",
    serial_by_id: Path = Path("/dev/serial/by-id"),
) -> list[str]:
    """Return every ACM endpoint plus serial identities related to autopilots."""
    found = {str(Path(item)) for item in glob.glob(acm_pattern)}
    identity_terms = ("pixhawk", "ardupilot", "px4", "fmu")
    try:
        identities = serial_by_id.iterdir()
    except (FileNotFoundError, NotADirectoryError, PermissionError):
        identities = ()
    for item in identities:
        if any(term in item.name.lower() for term in identity_terms):
            found.add(str(item))
    return sorted(found)


def legacy_bridge_active(
    unit: str = DEFAULT_LEGACY_UNIT,
    *,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
) -> bool:
    result = runner(
        ["systemctl", "--user", "is-active", "--quiet", unit],
        check=False,
        text=True,
        capture_output=True,
    )
    return result.returncode == 0


def udp_port_available(
    port: int,
    *,
    bind_address: str = "0.0.0.0",
    socket_factory: Callable[..., socket.socket] = socket.socket,
) -> bool:
    probe = socket_factory(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.bind((bind_address, port))
    except OSError:
        return False
    finally:
        probe.close()
    return True


def evaluate(
    *, serial_devices: list[str], bridge_active: bool, port_available: bool
) -> dict[str, object]:
    checks = {
        "pixhawk_usb_absent": {
            "pass": not serial_devices,
            "devices": serial_devices,
        },
        "legacy_usb_bridge_inactive": {"pass": not bridge_active},
        "udp_listener_available": {"pass": port_available},
    }
    passed = all(bool(item["pass"]) for item in checks.values())
    return {
        "schema": "efis-next.qgc-monitor-preflight.v1",
        "timestamp_utc": utc_now(),
        "mode": MODE,
        "status": "PASS" if passed else "BLOCKED",
        "checks": checks,
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
    parser.add_argument("--port", type=int, default=14550)
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--legacy-unit", default=DEFAULT_LEGACY_UNIT)
    parser.add_argument("--acm-pattern", default="/dev/ttyACM*")
    parser.add_argument("--serial-by-id", type=Path, default=Path("/dev/serial/by-id"))
    parser.add_argument(
        "--report",
        type=Path,
        default=Path("~/.local/state/efis-next/qgc-monitor-preflight.json").expanduser(),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not 1 <= args.port <= 65535:
        print("qgc-monitor-preflight: invalid UDP port", file=sys.stderr)
        return 2

    report = evaluate(
        serial_devices=discover_serial_devices(
            acm_pattern=args.acm_pattern, serial_by_id=args.serial_by_id
        ),
        bridge_active=legacy_bridge_active(args.legacy_unit),
        port_available=udp_port_available(args.port, bind_address=args.bind),
    )
    report["udp_endpoint"] = f"{args.bind}:{args.port}"
    report["legacy_unit"] = args.legacy_unit
    write_report(args.report, report)
    print(json.dumps(report, sort_keys=True))
    if report["status"] != "PASS":
        print(
            "qgc-monitor-preflight: blocked; inspect " + str(args.report),
            file=sys.stderr,
        )
        return BLOCKED_EXIT
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
