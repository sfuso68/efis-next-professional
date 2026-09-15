#!/usr/bin/env python3
"""Report the operational state of the EFIS QGC monitor-only station."""

from __future__ import annotations

import argparse
import configparser
import json
import os
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


def enablement_state(unit: str, *, user: bool) -> str:
    command = ["systemctl"]
    if user:
        command.append("--user")
    command.extend(["is-enabled", unit])
    result = run(command)
    return result.stdout.strip()


def qgc_audio_muted(path: Path) -> bool:
    parser = configparser.ConfigParser()
    parser.optionxform = str
    try:
        parser.read(path, encoding="utf-8")
        return parser.getboolean("General", "audioMuted")
    except (OSError, configparser.Error, KeyError, ValueError):
        return False


def speech_process_lines() -> list[str]:
    result = run(["pgrep", "-a", "-u", str(os.getuid()), "-x", "speech-dispatcher"])
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def listener_lines(port: int) -> list[str]:
    result = run(["ss", "-H", "-lunp"])
    suffix = f":{port}"
    return [
        line.strip()
        for line in result.stdout.splitlines()
        if any(field.endswith(suffix) for field in line.split())
    ]


def current_invocation_warnings(unit: str) -> list[str]:
    state = run(["systemctl", "--user", "show", unit, "-p", "InvocationID"])
    invocation_id = ""
    for line in state.stdout.splitlines():
        if line.startswith("InvocationID="):
            invocation_id = line.split("=", 1)[1].strip()
    if not invocation_id:
        return ["QGC invocation ID unavailable"]
    journal = run(
        [
            "journalctl",
            "--user",
            f"_SYSTEMD_INVOCATION_ID={invocation_id}",
            "-o",
            "cat",
            "--no-pager",
        ]
    )
    patterns = ("error loading text-to-speech",)
    return [
        line.strip()
        for line in journal.stdout.splitlines()
        if any(pattern in line.lower() for pattern in patterns)
    ]


def evaluate(
    *,
    preflight: dict[str, object],
    qgc_state: dict[str, str],
    qgc_enabled: bool,
    listeners: list[str],
    firewall_state: dict[str, str],
    firewall_enabled: bool,
    speech_state: dict[str, str] | None = None,
    speech_socket_state: dict[str, str] | None = None,
    speech_enablement: str = "",
    speech_socket_enablement: str = "",
    qgc_audio_is_muted: bool = False,
    speech_processes: list[str] | None = None,
    qgc_runtime_warnings: list[str] | None = None,
) -> dict[str, object]:
    speech_state = speech_state or {}
    speech_socket_state = speech_socket_state or {}
    speech_processes = speech_processes or []
    qgc_runtime_warnings = qgc_runtime_warnings or []
    speech_output_muted = (
        speech_enablement == "masked"
        and speech_socket_enablement == "masked"
        and speech_state.get("ActiveState") == "inactive"
        and speech_socket_state.get("ActiveState") == "inactive"
        and qgc_audio_is_muted
        and not speech_processes
    )
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
        "speech_backend_loaded": speech_state.get("LoadState") == "loaded",
        "speech_backend_running": speech_state.get("ActiveState") == "active"
        and speech_state.get("SubState") == "running",
        "qgc_speech_plugin_clean": not qgc_runtime_warnings,
        "speech_output_muted": speech_output_muted,
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
    supporting = (
        "firewall_loaded",
        "firewall_enabled",
        "firewall_active",
        "speech_output_muted",
    )
    if not all(checks[name] for name in critical):
        status = "FAIL"
    elif not all(checks[name] for name in supporting):
        status = "PARTIAL"
    else:
        status = "PASS"
    return {
        "schema": "efis-next.qgc-monitor-status.v2",
        "mode": "MONITOR_ONLY_NO_PIXHAWK_RETURN",
        "status": status,
        "checks": checks,
        "qgc": qgc_state,
        "firewall": firewall_state,
        "speech_backend": speech_state,
        "speech_socket": speech_socket_state,
        "speech_enablement": speech_enablement,
        "speech_socket_enablement": speech_socket_enablement,
        "qgc_audio_muted": qgc_audio_is_muted,
        "speech_processes": speech_processes,
        "speech_policy": "GROUND_STATION_SYNTHETIC_SPEECH_MUTED",
        "qgc_runtime_warnings": qgc_runtime_warnings,
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
    parser.add_argument(
        "--qgc-settings",
        type=Path,
        default=Path("~/.config/QGroundControl/QGroundControl.ini").expanduser(),
    )
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
        speech_state=systemctl_state("speech-dispatcher.service", user=True),
        speech_socket_state=systemctl_state("speech-dispatcher.socket", user=True),
        speech_enablement=enablement_state("speech-dispatcher.service", user=True),
        speech_socket_enablement=enablement_state(
            "speech-dispatcher.socket", user=True
        ),
        qgc_audio_is_muted=qgc_audio_muted(args.qgc_settings),
        speech_processes=speech_process_lines(),
        qgc_runtime_warnings=current_invocation_warnings("efis-qgc-monitor.service"),
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
