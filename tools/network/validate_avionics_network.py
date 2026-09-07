#!/usr/bin/env python3
"""Validate the static EFIS avionics network plan without touching the network."""

from __future__ import annotations

import argparse
import ipaddress
import json
from pathlib import Path
import sys


class PlanError(ValueError):
    pass


def validate(plan: dict) -> dict:
    if plan.get("schema") != "efis-next.avionics-network.v1":
        raise PlanError("unsupported network-plan schema")

    segments = {}
    for item in plan.get("segments", []):
        segment_id = item.get("id")
        if not segment_id or segment_id in segments:
            raise PlanError("segment ids must be present and unique")
        network = ipaddress.ip_network(item.get("cidr", ""), strict=True)
        if network.version != 4:
            raise PlanError(f"{segment_id}: only IPv4 is supported in V1")
        if item.get("dhcp") is not False or item.get("default_gateway") is not None:
            raise PlanError(f"{segment_id}: operational segments need static IPs and no gateway")
        if item.get("mtu") != 1500:
            raise PlanError(f"{segment_id}: MTU must remain 1500 until hardware acceptance")
        segments[segment_id] = network

    if set(segments) != {"flight-data", "video"}:
        raise PlanError("V1 requires exactly flight-data and video segments")

    nodes = {}
    occupied: dict[str, set[ipaddress.IPv4Address]] = {key: set() for key in segments}
    for node in plan.get("nodes", []):
        node_id = node.get("id")
        if not node_id or node_id in nodes:
            raise PlanError("node ids must be present and unique")
        addresses = node.get("addresses", {})
        if not addresses:
            raise PlanError(f"{node_id}: at least one address is required")
        for segment_id, address_text in addresses.items():
            if segment_id not in segments:
                raise PlanError(f"{node_id}: unknown segment {segment_id}")
            address = ipaddress.ip_address(address_text)
            network = segments[segment_id]
            if address not in network or address in {network.network_address, network.broadcast_address}:
                raise PlanError(f"{node_id}: {address} is not a usable {segment_id} address")
            if address in occupied[segment_id]:
                raise PlanError(f"{segment_id}: duplicate address {address}")
            occupied[segment_id].add(address)
        if node.get("role") == "camera" and set(addresses) != {"video"}:
            raise PlanError(f"{node_id}: cameras must remain isolated on video")
        nodes[node_id] = node

    expected_flows = {
        ("pixhawk-6x", "blackbox", 14550),
        ("blackbox", "pfd-01", 49002),
        ("blackbox", "ems-01", 49002),
    }
    actual_flows = set()
    for flow in plan.get("flows", []):
        source, destination = flow.get("source"), flow.get("destination")
        segment_id = flow.get("segment")
        if source not in nodes or destination not in nodes:
            raise PlanError(f"{flow.get('id', 'flow')}: endpoint node is unknown")
        if segment_id not in nodes[source]["addresses"] or segment_id not in nodes[destination]["addresses"]:
            raise PlanError(f"{flow.get('id', 'flow')}: endpoints do not share {segment_id}")
        if flow.get("transport") != "udp" or flow.get("unicast") is not True:
            raise PlanError(f"{flow.get('id', 'flow')}: V1 data flows must be UDP unicast")
        if flow.get("direction") != "one-way":
            raise PlanError(f"{flow.get('id', 'flow')}: primary flows must be one-way")
        port = flow.get("port")
        if not isinstance(port, int) or not 1 <= port <= 65535:
            raise PlanError(f"{flow.get('id', 'flow')}: invalid UDP port")
        actual_flows.add((source, destination, port))
    missing = expected_flows - actual_flows
    if missing:
        raise PlanError(f"required flows missing: {sorted(missing)}")

    switches = {}
    for switch in plan.get("switches", []):
        segment_id = switch.get("segment")
        if segment_id not in segments or segment_id in switches:
            raise PlanError("each operational segment needs one declared switch")
        port_count = switch.get("minimum_operational_ports")
        if not isinstance(port_count, int) or port_count < 1:
            raise PlanError(f"{switch.get('id', 'switch')}: invalid operational port count")
        roles = set(switch.get("allowed_roles", []))
        attached = [
            node for node in nodes.values()
            if segment_id in node["addresses"] and node["role"] != "ground-only"
        ]
        if port_count < len(attached):
            raise PlanError(f"{switch.get('id', 'switch')}: insufficient operational ports")
        disallowed = {node["role"] for node in attached} - roles
        if disallowed:
            raise PlanError(f"{switch.get('id', 'switch')}: disallowed roles {sorted(disallowed)}")
        switches[segment_id] = switch
    if set(switches) != set(segments):
        raise PlanError("every operational segment must declare a switch")

    return {
        "schema": plan["schema"],
        "segments": len(segments),
        "nodes": len(nodes),
        "flows": len(actual_flows),
        "switches": len(switches),
        "status": "PASS",
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plan", type=Path)
    args = parser.parse_args(argv)
    try:
        report = validate(json.loads(args.plan.read_text(encoding="utf-8")))
    except (OSError, json.JSONDecodeError, PlanError, ValueError) as error:
        print(f"network plan invalid: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
