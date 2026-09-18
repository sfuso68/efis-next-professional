#!/usr/bin/env python3
"""Validate the 100-control fail-closed G1/G2 integration readiness contract."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTRACT = ROOT / "config/hardware/avionix-g1-g2-readiness-v1.json"
DOMAINS_SHA256 = "dc923c6e9bcd632e18a81292848543fd0b182070ab673f8f6a31176ce827c8f6"

EXPECTED_DOMAINS = [
    ("inventory", "G1-R100-INV"),
    ("signal_mapping", "G1-R100-SIG"),
    ("calibration", "G1-R100-CAL"),
    ("transport", "G1-R100-NET"),
    ("evidence_release", "G1-R100-EVD"),
]

EXPECTED_SIGNALS = [
    ("engine_rpm", "rpm"),
    ("manifold_pressure", "inHg"),
    ("oil_pressure", "psi"),
    ("oil_temperature", "C"),
    ("fuel_pressure", "psi"),
    ("fuel_flow", "gph"),
    ("fuel_quantity_left", "gal"),
    ("fuel_quantity_right", "gal"),
    ("bus_voltage", "V"),
    ("alternator_current", "A"),
    ("cht_1", "C"),
    ("cht_2", "C"),
    ("cht_3", "C"),
    ("cht_4", "C"),
    ("egt_1", "C"),
    ("egt_2", "C"),
    ("egt_3", "C"),
    ("egt_4", "C"),
]

ALLOWED_STATES = {"PASS", "PREPARED", "BLOCKED_HARDWARE", "BLOCKED_ARCHITECTURE", "BLOCKED_APPROVAL"}


class ReadinessError(ValueError):
    """The readiness contract violates a frozen safety or evidence boundary."""


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise ReadinessError(f"{control}: {message}")


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ReadinessError("contract root must be an object")
    return value


def canonical_digest(value: object) -> str:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def flatten_controls(contract: dict) -> list[dict]:
    return [control for domain in contract["domains"] for control in domain["controls"]]


def _validate_controls(contract: dict) -> Counter:
    domains = contract["domains"]
    require(isinstance(domains, list) and len(domains) == 5, "G1-R100-001", "five domains required")
    for domain, (expected_name, prefix) in zip(domains, EXPECTED_DOMAINS):
        require(domain.get("domain") == expected_name, "G1-R100-001", "domain order or name changed")
        controls = domain.get("controls")
        require(isinstance(controls, list) and len(controls) == 20, "G1-R100-001", "twenty controls required per domain")
        for index, control in enumerate(controls, start=1):
            require(
                isinstance(control, dict) and set(control) == {"id", "key", "state"},
                "G1-R100-001", "control properties differ from V1",
            )
            require(control["id"] == f"{prefix}-{index:02d}", "G1-R100-001", "control ID sequence changed")
            require(isinstance(control["key"], str) and control["key"], "G1-R100-001", "control key missing")
            require(control["state"] in ALLOWED_STATES, "G1-R100-001", "unknown control state")
    require(len(flatten_controls(contract)) == 100, "G1-R100-001", "exactly one hundred controls required")
    require(canonical_digest(domains) == DOMAINS_SHA256, "G1-R100-001", "frozen control catalog changed")
    return Counter(control["state"] for control in flatten_controls(contract))


def _validate_cross_links(contract: dict) -> None:
    inventory = load_json(ROOT / contract["authorities"]["g1_inventory"])
    require(inventory["node"]["hostname"] == "ems-sensor", "G1-R100-002", "inventory node hostname drift")
    require(inventory["node"]["address"] == "192.168.1.89", "G1-R100-002", "inventory node address drift")
    require(len(inventory["boards"]) == 4, "G1-R100-003", "physical manifest must retain four slots")
    require(all(board["verification"] == "UNVERIFIED" for board in inventory["boards"]), "G1-R100-003", "hardware was promoted outside evidence flow")

    vertical = load_json(ROOT / contract["authorities"]["g2_vertical"])
    upstream = vertical["topology"]["source_to_blackbox"]
    require(upstream["transport"] == "UNRESOLVED", "G1-R100-004", "source transport was silently resolved")
    require(upstream["execution_allowed"] is False, "G1-R100-004", "source transport execution enabled")
    edp = next(flow for flow in vertical["downstream_flows"] if flow["protocol"] == "EDP_V1")
    require(edp["port"] == 49003 and edp["direction"] == "ONE_WAY", "G1-R100-005", "EDP downstream contract drift")

    types = (ROOT / "include/efis/core/types.hpp").read_text(encoding="utf-8")
    for signal, _unit in EXPECTED_SIGNALS:
        require(f'return "{signal}"' in types, "G1-R100-006", f"SignalId name missing: {signal}")
    snapshot = (ROOT / "include/efis/engine/engine_snapshot.hpp").read_text(encoding="utf-8")
    require("std::array<Sample<double>, 4> cht_c" in snapshot, "G1-R100-006", "four-channel CHT snapshot missing")
    require("std::array<Sample<double>, 4> egt_c" in snapshot, "G1-R100-006", "four-channel EGT snapshot missing")
    adr = (ROOT / contract["authorities"]["edp_adr"]).read_text(encoding="utf-8")
    require("18 campioni" in adr and "49003" in adr, "G1-R100-005", "EDP ADR no longer states 18 samples/49003")


def validate(contract: dict) -> dict:
    root_keys = {
        "schema", "revision", "status", "execution_authorized", "flight_approval",
        "authorities", "verified_software_baseline", "unresolved", "safety",
        "engine_signals", "transport", "evidence", "domains", "decision",
    }
    require(set(contract) == root_keys, "G1-R100-001", "root properties differ from V1")
    require(
        (contract["schema"], contract["revision"], contract["status"]) ==
        ("avionix-next.g1-g2-readiness.v1", "1.0.0", "PREPARED_SW_NOT_RUN"),
        "G1-R100-001", "contract identity or status differs from V1",
    )
    require(contract["execution_authorized"] is False, "G1-R100-007", "execution cannot be authorized")
    require(contract["flight_approval"] is False, "G1-R100-007", "flight cannot be approved")

    expected_authorities = {
        "master_brief": "docs/project/AVIONIX_NEXT_MASTER_PROJECT_BRIEF_CODEX_R0.md",
        "g1_discovery": "config/hardware/avionix-g1-sensor-node-discovery-v1.json",
        "g1_inventory": "config/hardware/avionix-g1-physical-inventory-v1.json",
        "g2_vertical": "config/network/avionix-g2-vertical-acceptance-v1.json",
        "edp_adr": "docs/adr/ADR-011-engine-data-protocol.md",
    }
    require(contract["authorities"] == expected_authorities, "G1-R100-002", "authority set changed")

    baseline = contract["verified_software_baseline"]
    require(baseline["node"] == {
        "hostname": "ems-sensor", "address": "192.168.1.89",
        "platform": "Raspberry Pi 4 Model B Rev 1.5",
        "capture_sha256": "f7cb1da79623c37891e662ec966f8d64be743a8d471663970993e4eac93f5e94",
    }, "G1-R100-002", "verified node provenance changed")
    require(baseline["expected_boards"] == [
        {"model": "SM-I-018", "quantity": 1},
        {"model": "SM-I-011", "quantity": 2},
        {"model": "SM-I-019", "quantity": 1},
    ], "G1-R100-003", "expected board inventory changed")
    require(
        baseline["engine_signal_count"] == 18
        and baseline["downstream_engine_protocol"] == "EDP_V1"
        and baseline["downstream_engine_port"] == 49003
        and baseline["blackbox_authority"] is True,
        "G1-R100-005", "verified software baseline changed",
    )

    unresolved = contract["unresolved"]
    require(unresolved == {
        "verified_physical_boards": 0,
        "board_addresses": "UNRESOLVED_NO_PROBE_AUTHORIZED",
        "physical_channel_map": "UNRESOLVED_PENDING_BOARD_INVENTORY",
        "calibration_coefficients": "UNRESOLVED_PENDING_BENCH_REFERENCE",
        "real_source_id": "UNRESOLVED_PENDING_ARCHITECTURE_DECISION",
        "source_to_blackbox_transport": "UNRESOLVED_PENDING_G1_DESIGN",
        "live_execution": "NOT_AUTHORIZED",
    }, "G1-R100-008", "unresolved hardware/architecture state was promoted")

    safety = contract["safety"]
    require(set(safety) == {
        "address_probe_allowed", "vendor_read_allowed", "register_write_allowed",
        "output_activation_allowed", "dip_change_allowed", "wiring_change_allowed",
        "runtime_deployment_allowed", "threshold_invention_allowed",
        "direct_sensor_to_display_allowed", "flight_use_allowed",
    }, "G1-R100-007", "safety property set changed")
    require(all(value is False for value in safety.values()), "G1-R100-007", "forbidden capability enabled")

    signals = contract["engine_signals"]
    require(len(signals) == 18, "G1-R100-006", "engine signal count differs from EDP V1")
    require(
        [(item.get("signal"), item.get("unit")) for item in signals] == EXPECTED_SIGNALS,
        "G1-R100-006", "engine signal names or units changed",
    )
    require(
        all(item.get("physical_channel") == "UNRESOLVED" and
            item.get("calibration_revision") == "UNRESOLVED" and
            set(item) == {"signal", "unit", "physical_channel", "calibration_revision"}
            for item in signals),
        "G1-R100-008", "physical mapping/calibration was invented",
    )

    require(contract["transport"] == {
        "source": "sensor-rpi4", "normalization_authority": "blackbox",
        "source_to_blackbox": "UNRESOLVED",
        "blackbox_to_ems": {"protocol": "EDP_V1", "transport": "UDP_UNICAST", "port": 49003, "direction": "ONE_WAY"},
        "displays_receive_only": True, "routing_bridge_nat_allowed": False,
    }, "G1-R100-005", "transport boundary changed")

    evidence = contract["evidence"]
    require(evidence == {
        "root": "docs/status/evidence/g1-g2-readiness", "relative_paths_only": True,
        "sha256_required": True, "utc_required": True, "operator_required": True,
        "minimum_soak_seconds": 3600, "same_dataset_replay_required": True,
    }, "G1-R100-009", "evidence boundary changed")
    root = PurePosixPath(evidence["root"])
    require(not root.is_absolute() and ".." not in root.parts, "G1-R100-009", "unsafe evidence root")

    counts = _validate_controls(contract)
    _validate_cross_links(contract)
    require(
        contract["decision"] == "NO_GO_PENDING_G1_PHYSICAL_INVENTORY_AND_ARCHITECTURE_REVIEW",
        "G1-R100-010", "NO-GO decision was weakened",
    )
    require(counts["BLOCKED_HARDWARE"] > 0 and counts["BLOCKED_ARCHITECTURE"] > 0,
            "G1-R100-010", "blocking dependencies disappeared")

    return {
        "schema": contract["schema"],
        "status": "PASS",
        "contract_status": contract["status"],
        "controls_validated": 100,
        "controls_total": 100,
        "state_counts": dict(sorted(counts.items())),
        "verified_physical_boards": 0,
        "source_to_blackbox_transport": "UNRESOLVED",
        "execution_authorized": False,
        "flight_approval": False,
        "decision": contract["decision"],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("contract", nargs="?", type=Path, default=DEFAULT_CONTRACT)
    args = parser.parse_args(argv)
    try:
        report = validate(load_json(args.contract))
    except (OSError, json.JSONDecodeError, ReadinessError, KeyError, TypeError, StopIteration) as error:
        print(f"G1/G2 readiness invalid: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
