#!/usr/bin/env python3
"""Arm or verify the server MAVLink observer reboot acceptance gate."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import getpass
import json
from pathlib import Path
import subprocess
import time


SCHEMA = "efis-next.server-observer-reboot-acceptance.v1"
REQUIRED_CONTAINERS = {
    "adguard",
    "caddy",
    "grafana",
    "influxdb",
    "litellm-db",
    "litellm",
    "mapapp",
    "mediamtx",
    "node-red",
    "open-webui",
    "tileserver",
    "uptime-kuma",
}


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, text=True, capture_output=True)


def properties(text: str) -> dict[str, str]:
    return dict(line.split("=", 1) for line in text.splitlines() if "=" in line)


def read_boot_id(path: Path = Path("/proc/sys/kernel/random/boot_id")) -> str:
    return path.read_text(encoding="utf-8").strip()


def read_json(path: Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
        return value if isinstance(value, dict) else {"error": "not an object"}
    except (OSError, json.JSONDecodeError) as error:
        return {"error": str(error)}


def enabled(unit: str, *, user: bool) -> bool:
    command = ["systemctl"]
    if user:
        command.append("--user")
    command.extend(["is-enabled", "--quiet", unit])
    return run(command).returncode == 0


def unit_state(unit: str, *, user: bool) -> dict[str, str]:
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
            "MainPID",
            "-p",
            "NRestarts",
            "--no-pager",
        ]
    )
    return properties(run(command).stdout)


def current_containers() -> set[str]:
    result = run(["docker", "ps", "--format", "{{.Names}}"])
    return {line.strip() for line in result.stdout.splitlines() if line.strip()}


def caddy_status() -> int | None:
    result = run(
        [
            "curl",
            "-k",
            "-sS",
            "-o",
            "/dev/null",
            "-w",
            "%{http_code}",
            "https://mappe.casa.lan/api/status",
        ]
    )
    try:
        return int(result.stdout)
    except ValueError:
        return None


def listener_present() -> bool:
    result = run(["ss", "-H", "-lunp"])
    return any("192.168.1.6:14550" in line for line in result.stdout.splitlines())


def linger_enabled(username: str) -> bool:
    result = run(["loginctl", "show-user", username, "-p", "Linger", "--value"])
    return result.returncode == 0 and result.stdout.strip() == "yes"


def evaluate(
    *,
    phase: str,
    baseline_boot_id: str,
    current_boot_id: str,
    sink: dict[str, str],
    sink_enabled: bool,
    firewall: dict[str, str],
    firewall_enabled: bool,
    health: dict[str, object],
    has_listener: bool,
    has_linger: bool,
    containers: set[str],
    caddy_http_status: int | None,
    now_unix_ns: int,
    max_health_age_seconds: float = 15.0,
) -> dict[str, object]:
    updated_ns = health.get("updated_unix_ns")
    age_seconds = (
        (now_unix_ns - int(updated_ns)) / 1_000_000_000
        if isinstance(updated_ns, int) and updated_ns <= now_unix_ns
        else None
    )
    counters = ("malformed", "socket_errors", "source_rejected")
    checks = {
        "sink_loaded": sink.get("LoadState") == "loaded",
        "sink_enabled": sink_enabled,
        "sink_running": sink.get("ActiveState") == "active"
        and sink.get("SubState") == "running",
        "sink_zero_restarts": sink.get("NRestarts") == "0",
        "firewall_loaded": firewall.get("LoadState") == "loaded",
        "firewall_enabled": firewall_enabled,
        "firewall_active": firewall.get("ActiveState") == "active",
        "health_live": health.get("status") == "LIVE",
        "health_rx_only": health.get("mode") == "RX_ONLY_NO_RETURN_PATH",
        "health_source_blackbox": health.get("allowed_source") == "192.168.1.47",
        "health_errors_zero": all(health.get(name) == 0 for name in counters),
        "health_fresh": age_seconds is not None
        and age_seconds <= max_health_age_seconds,
        "listener_present": has_listener,
        "linger_enabled": has_linger,
        "required_containers_running": REQUIRED_CONTAINERS <= containers,
        "caddy_protected_endpoint": caddy_http_status == 401,
    }
    if phase == "check":
        checks["reboot_observed"] = bool(baseline_boot_id) and (
            baseline_boot_id != current_boot_id
        )
    return {
        "schema": SCHEMA,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "phase": phase,
        "status": "PASS" if all(checks.values()) else "FAIL",
        "baseline_boot_id": baseline_boot_id,
        "current_boot_id": current_boot_id,
        "checks": checks,
        "health_age_seconds": age_seconds,
        "sink": sink,
        "firewall": firewall,
        "missing_containers": sorted(REQUIRED_CONTAINERS - containers),
        "caddy_http_status": caddy_http_status,
    }


def write_json(path: Path, value: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def collect(phase: str, baseline_boot_id: str, health_path: Path) -> dict[str, object]:
    return evaluate(
        phase=phase,
        baseline_boot_id=baseline_boot_id,
        current_boot_id=read_boot_id(),
        sink=unit_state("efis-mavlink-observer-sink.service", user=True),
        sink_enabled=enabled("efis-mavlink-observer-sink.service", user=True),
        firewall=unit_state("efis-mavlink-observer-firewall.service", user=False),
        firewall_enabled=enabled("efis-mavlink-observer-firewall.service", user=False),
        health=read_json(health_path),
        has_listener=listener_present(),
        has_linger=linger_enabled(getpass.getuser()),
        containers=current_containers(),
        caddy_http_status=caddy_status(),
        now_unix_ns=time.time_ns(),
    )


def parse_args() -> argparse.Namespace:
    state = Path("~/.local/state/efis-next").expanduser()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("arm", "check"))
    parser.add_argument("--baseline", type=Path, default=state / "server-reboot-arm.json")
    parser.add_argument("--output", type=Path, default=state / "server-reboot-result.json")
    parser.add_argument(
        "--health", type=Path, default=state / "mavlink-observer-health.json"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    baseline_boot_id = ""
    if args.phase == "check":
        baseline = read_json(args.baseline)
        baseline_boot_id = str(baseline.get("boot_id", ""))
    report = collect(args.phase, baseline_boot_id, args.health)
    if args.phase == "arm" and report["status"] == "PASS":
        write_json(
            args.baseline,
            {"schema": SCHEMA, "armed_utc": report["timestamp_utc"], "boot_id": report["current_boot_id"]},
        )
    write_json(args.output, report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
