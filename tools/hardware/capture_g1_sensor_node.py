#!/usr/bin/env python3
"""Capture a deterministic, read-only G1 sensor-node evidence report."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
from typing import Callable


SCHEMA = "avionix-next.g1-node-capture.v1"
CONTROL_IDS = [f"G1-C10-{index:02d}" for index in range(1, 11)]
COMMANDS: dict[str, tuple[str, ...]] = {
    "hostname": ("hostname",),
    "addresses": ("hostname", "-I"),
    "kernel": ("uname", "-a"),
    "architecture": ("uname", "-m"),
    "storage": ("lsblk", "-J", "-o", "NAME,SIZE,TYPE,FSTYPE,MOUNTPOINTS,MODEL"),
    "filesystems": ("df", "-PT"),
    "network": ("ip", "-j", "address"),
    "gpio_i2c": ("pinctrl", "get", "2-3"),
    "throttled": ("vcgencmd", "get_throttled"),
    "temperature": ("vcgencmd", "measure_temp"),
}
FORBIDDEN_COMMAND_NAMES = {
    "i2cdetect", "i2cget", "i2cset", "i2cdump", "modprobe", "raspi-config",
    "apt", "apt-get", "pip", "systemctl", "reboot", "shutdown",
}


class CaptureError(ValueError):
    """The capture or its safety contract is incomplete."""


def _read_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8", errors="replace").rstrip("\x00\n")
    except OSError:
        return None


def _sha256(path: Path) -> str | None:
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        return None


def _parse_os_release(raw: str | None) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in (raw or "").splitlines():
        if "=" not in line or line.startswith("#"):
            continue
        key, value = line.split("=", 1)
        values[key] = value.strip().strip('"')
    return values


def run_fixed_command(command: tuple[str, ...]) -> dict:
    if not command or command[0] in FORBIDDEN_COMMAND_NAMES:
        raise CaptureError("command is not in the read-only boundary")
    if command not in COMMANDS.values():
        raise CaptureError("arbitrary command rejected")
    try:
        result = subprocess.run(
            command, check=False, capture_output=True, text=True, timeout=5,
            shell=False,
        )
    except (FileNotFoundError, subprocess.TimeoutExpired) as error:
        return {"argv": list(command), "available": False, "error": type(error).__name__}
    return {
        "argv": list(command), "available": True, "returncode": result.returncode,
        "stdout": result.stdout.rstrip(), "stderr": result.stderr.rstrip(),
    }


def _controllers(root: Path) -> list[dict[str, str | None]]:
    base = root / "sys/class/i2c-dev"
    result = []
    for entry in sorted(base.glob("i2c-*"), key=lambda item: item.name):
        result.append({
            "id": entry.name,
            "name": _read_text(entry / "name"),
            "of_node": str((entry / "device/of_node").resolve(strict=False)),
        })
    return result


def _device_nodes(root: Path) -> list[str]:
    return [f"/dev/{path.name}" for path in sorted((root / "dev").glob("i2c-*"))]


def capture(
    root: Path = Path("/"),
    runner: Callable[[tuple[str, ...]], dict] = run_fixed_command,
    captured_utc: str | None = None,
) -> dict:
    commands = {name: runner(argv) for name, argv in COMMANDS.items()}
    config = root / "boot/firmware/config.txt"
    config_text = _read_text(config)
    meminfo = _read_text(root / "proc/meminfo")
    model = _read_text(root / "proc/device-tree/model")
    os_release = _parse_os_release(_read_text(root / "etc/os-release"))
    timestamp = captured_utc or datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")

    report = {
        "schema": SCHEMA,
        "revision": "1.0.0",
        "captured_utc": timestamp,
        "mode": "READ_ONLY_CONTROLLER_ENUMERATION",
        "node": {
            "hostname": commands["hostname"].get("stdout", ""),
            "addresses": commands["addresses"].get("stdout", "").split(),
            "os_release": os_release,
            "kernel": commands["kernel"].get("stdout", ""),
            "architecture": commands["architecture"].get("stdout", platform.machine()),
            "model": model,
        },
        "resources": {
            "meminfo": meminfo,
            "storage": commands["storage"],
            "filesystems": commands["filesystems"],
        },
        "network": commands["network"],
        "i2c": {
            "boot_config_path": "/boot/firmware/config.txt",
            "boot_config_sha256": _sha256(config),
            "i2c_arm_enabled": bool(config_text and any(
                line.strip() == "dtparam=i2c_arm=on" for line in config_text.splitlines()
            )),
            "device_nodes": _device_nodes(root),
            "controllers": _controllers(root),
            "gpio": commands["gpio_i2c"],
            "address_probe_performed": False,
            "vendor_read_performed": False,
        },
        "health": {
            "throttled": commands["throttled"],
            "temperature": commands["temperature"],
        },
        "command_evidence": commands,
        "safety": {
            "shell_used": False,
            "sudo_used": False,
            "write_command_used": False,
            "forbidden_operations_performed": [],
            "boards_verified": 0,
            "flight_approval": False,
            "decision": "NO_GO_PENDING_PHYSICAL_BOARD_INVENTORY",
        },
        "controls": [{"id": item, "status": "PASS"} for item in CONTROL_IDS],
    }
    validate(report)
    return report


def _require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise CaptureError(f"{control}: {message}")


def validate(report: dict) -> None:
    _require(report.get("schema") == SCHEMA and report.get("revision") == "1.0.0",
             "G1-C10-01", "capture identity invalid")
    _require(report.get("captured_utc", "").endswith("Z") and report.get("node", {}).get("hostname"),
             "G1-C10-02", "UTC or hostname missing")
    node = report.get("node", {})
    _require(node.get("os_release", {}).get("VERSION_ID") and node.get("kernel") and
             node.get("architecture") and node.get("model"),
             "G1-C10-03", "platform identity incomplete")
    resources = report.get("resources", {})
    _require(resources.get("meminfo") and resources.get("storage", {}).get("available") and
             resources.get("filesystems", {}).get("available"),
             "G1-C10-04", "memory or storage evidence missing")
    _require(report.get("network", {}).get("available"),
             "G1-C10-05", "network inventory unavailable")
    i2c = report.get("i2c", {})
    _require(i2c.get("boot_config_sha256") and i2c.get("i2c_arm_enabled") is True,
             "G1-C10-06", "I2C boot configuration not proven")
    _require("/dev/i2c-1" in i2c.get("device_nodes", []) and
             any(item.get("id") == "i2c-1" for item in i2c.get("controllers", [])) and
             i2c.get("address_probe_performed") is False and i2c.get("vendor_read_performed") is False,
             "G1-C10-07", "controller-only boundary not proven")
    gpio = i2c.get("gpio", {})
    _require(gpio.get("available") and "SDA1" in gpio.get("stdout", "") and
             "SCL1" in gpio.get("stdout", ""),
             "G1-C10-08", "GPIO I2C function not proven")
    health = report.get("health", {})
    _require(health.get("throttled", {}).get("available") and
             health.get("temperature", {}).get("available"),
             "G1-C10-09", "health evidence unavailable")
    safety = report.get("safety", {})
    _require(report.get("mode") == "READ_ONLY_CONTROLLER_ENUMERATION" and
             safety == {
                 "shell_used": False, "sudo_used": False, "write_command_used": False,
                 "forbidden_operations_performed": [], "boards_verified": 0,
                 "flight_approval": False,
                 "decision": "NO_GO_PENDING_PHYSICAL_BOARD_INVENTORY",
             } and report.get("controls") == [
                 {"id": item, "status": "PASS"} for item in CONTROL_IDS
             ], "G1-C10-10", "safety or NO-GO boundary weakened")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        report = capture()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (CaptureError, OSError) as error:
        print(f"G1 node capture failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps({
        "schema": SCHEMA, "status": "PASS", "controls": "10/10",
        "output": str(args.output), "decision": report["safety"]["decision"],
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
