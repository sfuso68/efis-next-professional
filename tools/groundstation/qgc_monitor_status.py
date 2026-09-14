#!/usr/bin/env python3
"""Report the operational state of the EFIS QGC monitor-only station."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, text=True, capture_output=True)


def systemctl_state(unit: str, *, user: bool) -> dict[str, str]:
    command = ["systemctl"]
    if user:
        command.append("--user")
    command.extend(
        [
            "show",
            unit,
            "-p",
            "LoadState",
            "-p",
            "ActiveState",
            "-p",
            "SubState",
            "-p",
            "NRestarts",
            "--no-pager",
        ]
    )
    result = run(command)
    properties: dict[str, str] = {}
    for line in result.stdout.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            properties[key] = value
    return properties


def is_enabled(unit: str, *, user: bool) -> bool:
    command = ["systemctl"]
    if user:
        command.append("--user")
    command.extend(["is-enabled", "--quiet", unit])
    return run(command).returncode == 0


def listener_lines(port: int) -> list[str]:
    result = run(["ss", "-H", "-lunp"])
    suffix = f":{port}"
    return [
        line.strip()
        for line in result.stdout.splitlines()
        if any(field.endswith(suffix) for field in line.split())
    ]


def evaluate(
    *,
    preflight: dict[str, object],
    qgc_state: dict[str, str],
    qgc_enabled: bool,
    listeners: list[str],
    firewall_state: dict[str, str],
    firewall_enabled: bool,
) -> dict[str, object]:
    checks = {
        "preflight_pass": preflight.get("status") == "PASS",
        "artifact_integrity": bool(
            preflight.get("checks", {})
            .get("artifact_integrity", {})
            .get("pass", False)
        ),
        "qgc_loaded": qgc_state.get("LoadState") == "loaded",
        "qgc_enabled": qgc_enabled,
        "qgc_running": qgc_state.get("ActiveState") == "active"
        and qgc_state.get("SubState") == "running",
        "qgc_zero_restarts": qgc_state.get("NRestarts") == "0",
        "single_udp_listener": len(listeners) == 1,
        "listener_is_qgroundcontrol": len(listeners) == 1
        and "QGroundControl" in listeners[0],
        "firewall_loaded": firewall_state.get("LoadState") == "loaded",
        "firewall_enabled": firewall_enabled,
        "firewall_active": firewall_state.get("ActiveState") == "active",
    }
    critical = (
        "preflight_pass",
        "artifact_integrity",
        "qgc_loaded",
        "qgc_enabled",
        "qgc_running",
        "qgc_zero_restarts",
        "single_udp_listener",
        "listener_is_qgroundcontrol",
    )
    firewall = ("firewall_loaded", "firewall_enabled", "firewall_active")
    if not all(checks[name] for name in critical):
        status = "FAIL"
    elif not all(checks[name] for name in firewall):
        status = "PARTIAL"
    else:
        status = "PASS"
    return {
        "schema": "efis-next.qgc-monitor-status.v1",
        "mode": "MONITOR_ONLY_NO_PIXHAWK_RETURN",
        "status": status,
        "checks": checks,
        "qgc": qgc_state,
        "firewall": firewall_state,
        "listeners": listeners,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--preflight",
        type=Path,
        default=Path("~/.local/state/efis-next/qgc-monitor-preflight.json").expanduser(),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("~/.local/state/efis-next/qgc-monitor-status.json").expanduser(),
    )
    parser.add_argument("--port", type=int, default=14550)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        preflight = json.loads(args.preflight.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        preflight = {"status": "MISSING", "error": str(error)}
    report = evaluate(
        preflight=preflight,
        qgc_state=systemctl_state("efis-qgc-monitor.service", user=True),
        qgc_enabled=is_enabled("efis-qgc-monitor.service", user=True),
        listeners=listener_lines(args.port),
        firewall_state=systemctl_state("efis-qgc-monitor-firewall.service", user=False),
        firewall_enabled=is_enabled("efis-qgc-monitor-firewall.service", user=False),
    )
    document = json.dumps(report, indent=2, sort_keys=True) + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_text(document, encoding="utf-8")
    temporary.replace(args.output)
    print(document, end="")
    return {"PASS": 0, "PARTIAL": 3, "FAIL": 1}[report["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
