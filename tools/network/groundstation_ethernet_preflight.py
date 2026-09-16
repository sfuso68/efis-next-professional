#!/usr/bin/env python3
"""Read-only readiness check for the Ground Station Ethernet migration."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import ipaddress
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Callable


SCHEMA = "efis-next.groundstation-ethernet-preflight.v2"
INTERFACE_PATTERN = re.compile(r"[A-Za-z0-9_.:-]{1,15}")


def command(
    args: list[str], runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run
) -> str:
    result = runner(args, check=False, text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(f"command failed: {args[0]}: {result.stderr.strip()}")
    return result.stdout


def interface_snapshot(name: str) -> dict[str, object]:
    root = Path("/sys/class/net") / name
    exists = root.is_dir()
    def read(field: str) -> str | None:
        try:
            return (root / field).read_text(encoding="ascii").strip()
        except OSError:
            return None
    addresses: list[str] = []
    if exists:
        document = json.loads(command(["ip", "-j", "address", "show", "dev", name]))
        if document:
            addresses = [
                item["local"]
                for item in document[0].get("addr_info", [])
                if item.get("family") == "inet"
            ]
    return {
        "name": name,
        "exists": exists,
        "carrier": read("carrier") == "1",
        "operstate": read("operstate"),
        "ipv4": addresses,
    }


def route_snapshot(destination: str) -> dict[str, object]:
    try:
        routes = json.loads(command(["ip", "-j", "route", "get", destination]))
    except (RuntimeError, json.JSONDecodeError):
        return {"reachable": False, "device": None, "source": None, "gateway": None}
    if not routes:
        return {"reachable": False, "device": None, "source": None, "gateway": None}
    return {
        "reachable": True,
        "device": routes[0].get("dev"),
        "source": routes[0].get("prefsrc"),
        "gateway": routes[0].get("gateway"),
    }


def exact_route_snapshot(prefix: str) -> list[dict[str, object]]:
    """Return only routes explicitly installed for a prefix, not default-route matches."""
    try:
        routes = json.loads(command(["ip", "-j", "route", "show", prefix]))
    except (RuntimeError, json.JSONDecodeError):
        return []
    return routes if isinstance(routes, list) else []


def forwarding_snapshot() -> bool:
    value = Path("/proc/sys/net/ipv4/ip_forward").read_text(encoding="ascii").strip()
    return value == "1"


def qgc_snapshot() -> dict[str, object]:
    output = command([
        "systemctl", "--user", "show", "efis-qgc-monitor.service",
        "-p", "ActiveState", "-p", "SubState", "-p", "MainPID", "-p", "NRestarts",
    ])
    fields = dict(line.split("=", 1) for line in output.splitlines() if "=" in line)
    socket_output = command(["ss", "-H", "-u", "-l", "-n", "-p", "sport = :14550"])
    listeners = [line for line in socket_output.splitlines() if line.strip()]
    return {
        "active": fields.get("ActiveState") == "active" and fields.get("SubState") == "running",
        "pid": int(fields.get("MainPID", "0")),
        "restarts": int(fields.get("NRestarts", "-1")),
        "udp_14550_listeners": len(listeners),
        "listener_is_qgc": len(listeners) == 1 and "QGroundControl" in listeners[0],
    }


def evaluate(snapshot: dict[str, object], phase: str) -> dict[str, object]:
    ethernet = snapshot["ethernet"]
    wifi = snapshot["wifi"]
    route = snapshot["route"]
    flight_routes = snapshot["flight_data_routes"]
    qgc = snapshot["qgc"]
    address = str(snapshot["expected_address"])
    common = {
        "ethernet_present": bool(ethernet["exists"]),
        "ethernet_carrier": bool(ethernet["carrier"]),
        "qgc_active": bool(qgc["active"]),
        "qgc_zero_restarts": int(qgc["restarts"]) == 0,
        "qgc_single_udp_listener": int(qgc["udp_14550_listeners"]) == 1
        and bool(qgc["listener_is_qgc"]),
    }
    if phase == "physical":
        checks = {
            **common,
            "wifi_holds_service_address": address in wifi["ipv4"],
            "ethernet_does_not_duplicate_address": address not in ethernet["ipv4"],
        }
    else:
        checks = {
            **common,
            "ethernet_holds_service_address": address in ethernet["ipv4"],
            "wifi_released_service_address": address not in wifi["ipv4"],
            "route_uses_ethernet": route["device"] == ethernet["name"],
            "route_source_is_service_address": route["source"] == address,
            "blackbox_is_on_service_lan": route["gateway"] is None,
            "no_direct_flight_data_route_on_ethernet": not any(
                item.get("dev") == ethernet["name"] for item in flight_routes
            ),
            "ipv4_forwarding_disabled": snapshot["ipv4_forwarding"] is False,
        }
    return {
        "phase": phase,
        "status": "PASS" if all(checks.values()) else "BLOCKED",
        "checks": checks,
        "failed_checks": [key for key, value in checks.items() if not value],
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=("physical", "post-switch"), default="physical")
    parser.add_argument("--ethernet", default="eno1")
    parser.add_argument("--wifi", default="wlo1")
    parser.add_argument("--address", default="192.168.1.39")
    parser.add_argument("--blackbox", default="192.168.1.47")
    parser.add_argument("--flight-prefix", default="10.77.10.0/24")
    args = parser.parse_args()
    if not INTERFACE_PATTERN.fullmatch(args.ethernet) or not INTERFACE_PATTERN.fullmatch(args.wifi):
        parser.error("invalid interface name")
    for field in ("address", "blackbox"):
        value = ipaddress.ip_address(getattr(args, field))
        if value.version != 4 or value.is_unspecified:
            parser.error(f"{field} must be a specific IPv4 address")
        setattr(args, field, str(value))
    network = ipaddress.ip_network(args.flight_prefix, strict=True)
    if network.version != 4:
        parser.error("flight-prefix must be an IPv4 network")
    args.flight_prefix = str(network)
    if args.ethernet == args.wifi:
        parser.error("Ethernet and Wi-Fi interfaces must differ")
    return args


def main() -> int:
    args = parse_args()
    try:
        snapshot = {
            "schema": SCHEMA,
            "timestamp_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "mode": "READ_ONLY_NO_NETWORK_MUTATION",
            "expected_address": args.address,
            "ethernet": interface_snapshot(args.ethernet),
            "wifi": interface_snapshot(args.wifi),
            "route": route_snapshot(args.blackbox),
            "flight_data_routes": exact_route_snapshot(args.flight_prefix),
            "ipv4_forwarding": forwarding_snapshot(),
            "qgc": qgc_snapshot(),
        }
        snapshot["assessment"] = evaluate(snapshot, args.phase)
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(f"preflight failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(snapshot, indent=2, sort_keys=True))
    return 0 if snapshot["assessment"]["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
