#!/usr/bin/env python3
"""Arm or verify the QGC graphical-login reboot acceptance gate."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import getpass
import json
from pathlib import Path
import subprocess
import sys


SCHEMA = "efis-next.qgc-reboot-acceptance.v1"
REQUIRED_STATUS_CHECKS = (
    "preflight_pass",
    "artifact_integrity",
    "qgc_loaded",
    "qgc_enabled",
    "qgc_running",
    "qgc_zero_restarts",
    "single_udp_listener",
    "listener_is_qgroundcontrol",
    "speech_backend_loaded",
    "speech_backend_running",
    "qgc_speech_plugin_clean",
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def read_boot_id(path: Path = Path("/proc/sys/kernel/random/boot_id")) -> str:
    return path.read_text(encoding="utf-8").strip()


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=False, text=True, capture_output=True)


def linger_disabled(username: str) -> bool:
    result = run(["loginctl", "show-user", username, "-p", "Linger", "--value"])
    return result.returncode == 0 and result.stdout.strip() == "no"


def operational_checks(status: dict[str, object]) -> dict[str, bool]:
    source = status.get("checks", {})
    if not isinstance(source, dict):
        source = {}
    return {name: source.get(name) is True for name in REQUIRED_STATUS_CHECKS}


def evaluate(
    *,
    phase: str,
    baseline_boot_id: str,
    current_boot_id: str,
    status: dict[str, object],
    is_linger_disabled: bool,
) -> dict[str, object]:
    checks = operational_checks(status)
    checks["linger_disabled"] = is_linger_disabled
    if phase == "check":
        checks["reboot_observed"] = bool(baseline_boot_id) and (
            current_boot_id != baseline_boot_id
        )
    passed = all(checks.values())
    return {
        "schema": SCHEMA,
        "timestamp_utc": utc_now(),
        "phase": phase,
        "status": "PASS" if passed else "FAIL",
        "baseline_boot_id": baseline_boot_id,
        "current_boot_id": current_boot_id,
        "checks": checks,
        "qgc_status": status,
        "firewall_gate_separate": True,
    }


def write_json(path: Path, document: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def collect_qgc_status(script: Path, output: Path) -> dict[str, object]:
    result = run([sys.executable, str(script), "--output", str(output)])
    if result.returncode not in (0, 3):
        return {"status": "FAIL", "error": result.stderr.strip()}
    try:
        return json.loads(output.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return {"status": "FAIL", "error": str(error)}


def parse_args() -> argparse.Namespace:
    state = Path("~/.local/state/efis-next").expanduser()
    library = Path("~/.local/lib/efis-next").expanduser()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("arm", "check"))
    parser.add_argument("--baseline", type=Path, default=state / "qgc-reboot-arm.json")
    parser.add_argument("--output", type=Path, default=state / "qgc-reboot-result.json")
    parser.add_argument(
        "--status-output", type=Path, default=state / "qgc-monitor-status.json"
    )
    parser.add_argument(
        "--status-script", type=Path, default=library / "qgc_monitor_status.py"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    boot_id = read_boot_id()
    status = collect_qgc_status(args.status_script, args.status_output)
    if args.phase == "arm":
        report = evaluate(
            phase="arm",
            baseline_boot_id=boot_id,
            current_boot_id=boot_id,
            status=status,
            is_linger_disabled=linger_disabled(getpass.getuser()),
        )
        if report["status"] == "PASS":
            write_json(
                args.baseline,
                {"schema": SCHEMA, "armed_utc": utc_now(), "boot_id": boot_id},
            )
    else:
        try:
            baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
            baseline_boot_id = str(baseline["boot_id"])
        except (OSError, KeyError, TypeError, json.JSONDecodeError) as error:
            baseline_boot_id = ""
            status = {**status, "baseline_error": str(error)}
        report = evaluate(
            phase="check",
            baseline_boot_id=baseline_boot_id,
            current_boot_id=boot_id,
            status=status,
            is_linger_disabled=linger_disabled(getpass.getuser()),
        )
    write_json(args.output, report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
