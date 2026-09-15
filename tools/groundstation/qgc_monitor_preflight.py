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
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
from typing import Callable

from qgc_appdir_digest import digest_tree


MODE = "MONITOR_ONLY_NO_PIXHAWK_RETURN"
DEFAULT_LEGACY_UNIT = "efis-mavlink-readonly-bridge.service"
DEFAULT_ALLOWED_SOURCE = "192.168.1.47"
BLOCKED_EXIT = 78  # EX_CONFIG: operator action is required; do not restart-loop.
MANIFEST_SCHEMA = "efis-next.qgc-monitor-manifest.v2"


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inspect_artifacts(
    manifest_path: Path,
    *,
    bind_address: str,
    port: int,
    legacy_unit: str,
) -> dict[str, object]:
    """Validate the approved immutable QGC package and local policy contract."""
    result: dict[str, object] = {
        "pass": False,
        "manifest": str(manifest_path),
    }
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        appimage = manifest["appimage"]
        appdir = manifest["appdir"]
        listener = manifest["listener"]
        appimage_path = Path(appimage["path"]).expanduser()
        appdir_path = Path(appdir["path"]).expanduser()
        executable = Path(manifest["executable"]).expanduser()
        expected_sha256 = str(appimage["sha256"]).lower()
        if manifest["schema"] != MANIFEST_SCHEMA:
            raise ValueError("unsupported manifest schema")
        if manifest["mode"] != MODE:
            raise ValueError("manifest mode is not monitor-only")
        if listener["bind"] != bind_address or listener["port"] != port:
            raise ValueError("listener endpoint differs from unit arguments")
        if listener["allowed_source"] != DEFAULT_ALLOWED_SOURCE:
            raise ValueError("observer source is not the approved Blackbox")
        if manifest["legacy_unit"] != legacy_unit:
            raise ValueError("legacy unit differs from unit arguments")
        if len(expected_sha256) != 64:
            raise ValueError("invalid expected SHA-256")
        expected_tree_sha256 = str(appdir["tree_sha256"]).lower()
        if len(expected_tree_sha256) != 64:
            raise ValueError("invalid expected AppDir SHA-256")
        if not appdir_path.is_dir():
            raise ValueError("approved AppDir is missing or not a directory")
        if not executable.is_file() or not os.access(executable, os.X_OK):
            raise ValueError("approved AppRun is missing or not executable")
        try:
            executable.resolve(strict=True).relative_to(appdir_path.resolve(strict=True))
        except ValueError as error:
            raise ValueError("approved AppRun is outside AppDir") from error
        actual_sha256 = sha256_file(appimage_path)
        if actual_sha256 != expected_sha256:
            raise ValueError("AppImage SHA-256 mismatch")
        actual_appdir = digest_tree(appdir_path)
        expected_appdir = {
            "tree_sha256": expected_tree_sha256,
            "file_count": int(appdir["file_count"]),
            "symlink_count": int(appdir["symlink_count"]),
            "directory_count": int(appdir["directory_count"]),
            "file_bytes": int(appdir["file_bytes"]),
        }
        if any(
            expected_appdir[key] < 0
            for key in (
                "file_count",
                "symlink_count",
                "directory_count",
                "file_bytes",
            )
        ):
            raise ValueError("invalid negative AppDir inventory")
        for key, expected in expected_appdir.items():
            if actual_appdir[key] != expected:
                raise ValueError(f"AppDir {key} mismatch")
    except (KeyError, TypeError, ValueError, OSError, json.JSONDecodeError) as error:
        result["reason"] = str(error)
        return result

    result.update(
        {
            "pass": True,
            "application": manifest["application"],
            "version": manifest["version"],
            "appimage": str(appimage_path),
            "appimage_sha256": actual_sha256,
            "appdir": actual_appdir,
            "executable": str(executable),
            "allowed_source": listener["allowed_source"],
        }
    )
    return result


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
    *,
    serial_devices: list[str],
    bridge_active: bool,
    port_available: bool,
    artifact_integrity: dict[str, object] | None = None,
) -> dict[str, object]:
    checks = {
        "pixhawk_usb_absent": {
            "pass": not serial_devices,
            "devices": serial_devices,
        },
        "legacy_usb_bridge_inactive": {"pass": not bridge_active},
        "udp_listener_available": {"pass": port_available},
    }
    if artifact_integrity is not None:
        checks["artifact_integrity"] = artifact_integrity
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
        "--manifest",
        type=Path,
        default=Path("~/.local/lib/efis-next/qgc-monitor-manifest.json").expanduser(),
    )
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
        artifact_integrity=inspect_artifacts(
            args.manifest,
            bind_address=args.bind,
            port=args.port,
            legacy_unit=args.legacy_unit,
        ),
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
