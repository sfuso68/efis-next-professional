#!/usr/bin/env python3
"""Validate the fail-closed MAVLink observer policy without touching a network."""

from __future__ import annotations

import argparse
import ipaddress
import json
from pathlib import Path
import sys


class PolicyError(ValueError):
    pass


def validate(document: dict) -> dict:
    if document.get("schema") != "efis-next.mavlink-observer-gateway.v1":
        raise PolicyError("unsupported observer gateway schema")
    if document.get("status") != "BENCH_NOT_FLIGHT_APPROVED":
        raise PolicyError("observer gateway must remain bench-only")
    if document.get("implementation") != "APPLICATION_FANOUT_NOT_IP_BRIDGE":
        raise PolicyError("implementation must remain an application fan-out")

    service = ipaddress.ip_network("192.168.1.0/24")
    ingress = document.get("ingress")
    if not isinstance(ingress, dict):
        raise PolicyError("ingress is required")
    bind = ipaddress.ip_address(ingress.get("bind", ""))
    source = ipaddress.ip_address(ingress.get("allowed_source", ""))
    if not isinstance(bind, ipaddress.IPv4Address) or bind != ipaddress.ip_address("10.77.10.2"):
        raise PolicyError("ingress must bind the Blackbox FLIGHT-DATA address")
    if not isinstance(source, ipaddress.IPv4Address) or source != ipaddress.ip_address("10.77.10.3"):
        raise PolicyError("only the installed Pixhawk source is allowed")
    if ingress.get("port") != 14550 or ingress.get("segment") != "flight-data":
        raise PolicyError("unexpected MAVLink ingress port or segment")

    observers = document.get("observers")
    if not isinstance(observers, list) or not observers or len(observers) > 8:
        raise PolicyError("one to eight observers are required")
    ids: set[str] = set()
    endpoints: set[tuple[ipaddress.IPv4Address, int]] = set()
    for observer in observers:
        if not isinstance(observer, dict):
            raise PolicyError("observer must be an object")
        observer_id = observer.get("id")
        if not isinstance(observer_id, str) or not observer_id or observer_id in ids:
            raise PolicyError("observer ids must be present and unique")
        ids.add(observer_id)
        address = ipaddress.ip_address(observer.get("address", ""))
        port = observer.get("port")
        if not isinstance(address, ipaddress.IPv4Address) or address not in service:
            raise PolicyError("observer must use the declared service LAN")
        if port != 14550 or (address, port) in endpoints:
            raise PolicyError("observer endpoint must be unique on UDP 14550")
        endpoints.add((address, port))
        if observer.get("role") not in {
            "QGC_MONITOR_ONLY", "DEVELOPMENT_MONITOR_ONLY"
        }:
            raise PolicyError("observer role must be monitor-only")

    expected = {
        ("ground-station-laptop", ipaddress.ip_address("192.168.1.39")),
        ("home-server", ipaddress.ip_address("192.168.1.6")),
    }
    actual = {
        (observer["id"], ipaddress.ip_address(observer["address"]))
        for observer in observers
    }
    if actual != expected:
        raise PolicyError("observer inventory differs from the approved bench endpoints")

    controls = document.get("controls")
    if not isinstance(controls, dict):
        raise PolicyError("controls are required")
    required_false = {
        "ip_forward_required",
        "layer2_bridge_allowed",
        "nat_allowed",
        "broadcast_allowed",
        "multicast_allowed",
        "return_path_implemented",
        "mavlink_command_forwarding_allowed",
        "parameter_write_allowed",
        "mission_write_allowed",
        "arming_allowed",
    }
    violated = sorted(key for key in required_false if controls.get(key) is not False)
    if violated:
        raise PolicyError("unsafe controls enabled or missing: " + ", ".join(violated))

    return {
        "schema": document["schema"],
        "observers": len(observers),
        "return_path": False,
        "status": "PASS",
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("policy", type=Path)
    args = parser.parse_args(argv)
    try:
        report = validate(json.loads(args.policy.read_text(encoding="utf-8")))
    except (OSError, json.JSONDecodeError, PolicyError, ValueError) as exc:
        print(f"observer policy invalid: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
