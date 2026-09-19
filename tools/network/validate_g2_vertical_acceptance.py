#!/usr/bin/env python3
"""Validate the fail-closed AVIONIX G2 live-vertical acceptance contract."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTRACT = ROOT / "config/network/avionix-g2-vertical-acceptance-v1.json"
DEFAULT_NETWORK = ROOT / "config/network/avionics-network-v1.json"
DEFAULT_ICD = ROOT / "config/protocols/avionix-data-icd-v1.json"


class ContractError(ValueError):
    """The proposed live-vertical contract weakened a frozen invariant."""


CONTROL_SPECS = [
    ("G2-V20-01", "contract_identity", "DEFINED", "validator-report"),
    ("G2-V20-02", "prepared_not_run", "DEFINED", "status-report"),
    ("G2-V20-03", "frozen_icd_commit", "DEFINED", "commit-digest"),
    ("G2-V20-04", "topology_stages", "DEFINED", "topology-report"),
    ("G2-V20-05", "source_transport_dependency", "BLOCKED_DEPENDENCY", "g1-inventory-and-adapter"),
    ("G2-V20-06", "blackbox_authority", "DEFINED", "broker-identity"),
    ("G2-V20-07", "pfd_ddp_flow", "DEFINED", "passive-capture"),
    ("G2-V20-08", "mfd_ddp_flow", "DEFINED", "passive-capture"),
    ("G2-V20-09", "ems_edp_flow", "DEFINED", "passive-capture"),
    ("G2-V20-10", "one_way_receive_only", "DEFINED", "firewall-and-socket-report"),
    ("G2-V20-11", "no_command_or_actuation", "DEFINED", "negative-path-report"),
    ("G2-V20-12", "simulation_provenance", "DEFINED", "sample-provenance-report"),
    ("G2-V20-13", "integrity_and_identity", "DEFINED", "fault-report"),
    ("G2-V20-14", "freshness_monotonic", "DEFINED", "timing-report"),
    ("G2-V20-15", "sequence_session_discipline", "DEFINED", "sequence-report"),
    ("G2-V20-16", "recorder_replay_parity", "DEFINED", "replay-report"),
    ("G2-V20-17", "passive_observer", "DEFINED", "observer-report"),
    ("G2-V20-18", "mandatory_fault_matrix", "DEFINED", "fault-matrix-report"),
    ("G2-V20-19", "evidence_provenance", "DEFINED", "evidence-bundle"),
    ("G2-V20-20", "go_no_go_and_rollback", "NO_GO", "approved-runbook-and-rollback"),
]

ROOT_KEYS = {
    "schema", "revision", "status", "execution_authorized", "flight_approval",
    "freeze", "topology", "downstream_flows", "safety", "integrity",
    "freshness", "recording_replay", "observer", "mandatory_faults",
    "evidence", "rollback", "controls",
}

STAGES = [
    "sensor-simulator", "rpi4-acquisition-adapter", "blackbox-broker-recorder",
    "pfd-ddp-consumer", "mfd-ddp-consumer", "ems-edp-consumer", "replay-verifier",
]

FLOWS = [
    {"id": "blackbox-ddp-pfd", "source": "blackbox", "destination": "pfd-01", "protocol": "DDP_V2", "transport": "UDP_UNICAST", "port": 49002, "direction": "ONE_WAY"},
    {"id": "blackbox-ddp-mfd", "source": "blackbox", "destination": "mfd-01", "protocol": "DDP_V2", "transport": "UDP_UNICAST", "port": 49002, "direction": "ONE_WAY"},
    {"id": "blackbox-edp-ems", "source": "blackbox", "destination": "ems-01", "protocol": "EDP_V1", "transport": "UDP_UNICAST", "port": 49003, "direction": "ONE_WAY"},
]

FAULTS = [
    "source_absent", "source_stale", "crc_corruption", "unknown_source",
    "duplicate_sequence", "out_of_order_sequence", "publisher_restart",
    "blackbox_restart", "network_loss", "storage_full", "clock_regression",
    "consumer_restart",
]


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ContractError(f"{path}: root must be an object")
    return value


def require(condition: bool, control_id: str, message: str) -> None:
    if not condition:
        raise ContractError(f"{control_id}: {message}")


def expected_controls() -> list[dict]:
    return [
        {"id": item[0], "key": item[1], "state": item[2], "evidence_required": item[3]}
        for item in CONTROL_SPECS
    ]


def validate(contract: dict, network: dict, icd: dict) -> dict:
    require(set(contract) == ROOT_KEYS, "G2-V20-01", "contract root keys differ from V1")
    require(
        (contract["schema"], contract["revision"]) ==
        ("avionix-next.g2-vertical-acceptance.v1", "1.0.0"),
        "G2-V20-01", "contract identity differs from V1",
    )
    require(
        contract["status"] == "PREPARED_SW_NOT_RUN"
        and contract["execution_authorized"] is False
        and contract["flight_approval"] is False,
        "G2-V20-02", "prepared/not-run or approval state was weakened",
    )

    freeze = contract["freeze"]
    require(
        freeze == {
            "source_commit": "c331a9d3846d1f59addcea76f9db530174c8ec11",
            "icd_path": "config/protocols/avionix-data-icd-v1.json",
            "network_plan_path": "config/network/avionics-network-v1.json",
        },
        "G2-V20-03", "frozen source references changed",
    )
    require(icd.get("status") == "FROZEN-SW", "G2-V20-03", "ICD is not frozen")

    topology = contract["topology"]
    require(
        set(topology) == {"stages", "publisher_authority", "display_authority", "source_to_blackbox"},
        "G2-V20-04", "topology contains missing or unknown properties",
    )
    require(topology.get("stages") == STAGES, "G2-V20-04", "topology stages changed")
    source_link = topology.get("source_to_blackbox", {})
    require(
        source_link == {
            "transport": "UNRESOLVED",
            "execution_allowed": False,
            "blocked_by": ["AVX-G1-01", "AVX-G1-02", "AVX-G1-03"],
        },
        "G2-V20-05", "unresolved source transport must remain a no-go dependency",
    )
    require(
        topology.get("publisher_authority") == "BLACKBOX_BROKER"
        and icd.get("authority", {}).get("publisher_role") == "BLACKBOX_BROKER",
        "G2-V20-06", "Blackbox publisher authority changed",
    )
    require(
        icd.get("authority", {}).get("default_source_node") == 47,
        "G2-V20-06", "frozen Blackbox source node changed",
    )

    require(contract["downstream_flows"] == FLOWS, "G2-V20-07", "downstream flow set changed")
    network_flows = {item.get("id"): item for item in network.get("flows", [])}
    for control_id, expected in zip(("G2-V20-07", "G2-V20-08", "G2-V20-09"), FLOWS):
        actual = network_flows.get(expected["id"], {})
        require(
            (actual.get("source"), actual.get("destination"), actual.get("port")) ==
            (expected["source"], expected["destination"], expected["port"]),
            control_id, f"network plan drift for {expected['id']}",
        )
        require(
            actual.get("transport") == "udp" and actual.get("unicast") is True
            and actual.get("direction") == "one-way",
            "G2-V20-10", f"{expected['id']} is not one-way UDP unicast",
        )
        protocol = icd.get("protocols", {}).get(expected["protocol"], {})
        require(
            protocol.get("default_udp_port") == expected["port"],
            control_id, f"ICD port drift for {expected['protocol']}",
        )

    nodes = {item.get("id"): item for item in network.get("nodes", [])}
    require(nodes.get("pfd-01", {}).get("platform") == "rpi5", "G2-V20-07", "PFD platform drift")
    require(nodes.get("mfd-01", {}).get("platform") == "rpi5", "G2-V20-08", "MFD platform drift")
    require(nodes.get("ems-01", {}).get("platform") == "rpi4", "G2-V20-09", "EMS platform drift")

    safety = contract["safety"]
    require(
        set(safety) == {
            "command_path", "actuation_path", "pixhawk_write_allowed",
            "display_transmit_allowed", "simulation_must_be_visible",
            "unknown_source_policy",
        },
        "G2-V20-11", "safety contains missing or unknown properties",
    )
    require(
        topology.get("display_authority") == "RECEIVE_ONLY"
        and icd.get("authority", {}).get("display_direction") == "RECEIVE_ONLY"
        and safety.get("display_transmit_allowed") is False,
        "G2-V20-10", "display receive-only boundary changed",
    )
    require(
        safety.get("command_path") == "ABSENT"
        and safety.get("actuation_path") == "ABSENT"
        and safety.get("pixhawk_write_allowed") is False
        and safety.get("unknown_source_policy") == "REJECT",
        "G2-V20-11", "command or actuation path must remain absent",
    )
    require(
        safety.get("simulation_must_be_visible") is True
        and icd.get("enumerations", {}).get("validity", {}).get("simulated") == 5,
        "G2-V20-12", "simulation provenance is not explicit",
    )

    integrity = contract["integrity"]
    require(
        integrity == {
            "byte_order": "BIG_ENDIAN",
            "payload_crc": "CRC32C_CASTAGNOLI",
            "source_identity_required": True,
            "publisher_instance_required": True,
            "sequence_policy": "REJECT_DUPLICATE_OR_OUT_OF_ORDER",
            "schema_policy": "REJECT_UNKNOWN",
        }
        and icd.get("transport", {}).get("byte_order") == "BIG_ENDIAN",
        "G2-V20-13", "integrity or identity policy changed",
    )

    freshness = contract["freshness"]
    require(
        freshness == {
            "clock": "CONSUMER_LOCAL_MONOTONIC",
            "stale_after_ms": 250,
            "lost_after_ms": 1000,
            "rejected_packet_preserves_last_valid": True,
        }
        and icd.get("freshness", {}).get("clock") == "CONSUMER_LOCAL_MONOTONIC"
        and icd.get("freshness", {}).get("stale_after_ms") == 250
        and icd.get("freshness", {}).get("lost_after_ms") == 1000,
        "G2-V20-14", "freshness model drifted from the frozen ICD",
    )
    require(
        integrity.get("sequence_policy") == "REJECT_DUPLICATE_OR_OUT_OF_ORDER"
        and integrity.get("publisher_instance_required") is True,
        "G2-V20-15", "sequence/session discipline was weakened",
    )

    replay = contract["recording_replay"]
    require(
        replay == {
            "raw_and_normalized_provenance_required": True,
            "recording_sha256_required": True,
            "replay_uses_same_recording": True,
            "cross_consumer_outcome_match_required": True,
        },
        "G2-V20-16", "recorder/replay parity requirements changed",
    )
    observer = contract["observer"]
    require(
        observer == {
            "mode": "PASSIVE_NO_BIND_NO_RETURN_PATH",
            "payload_persistence": False,
            "minimum_smoke_seconds": 60,
            "minimum_endurance_seconds": 3600,
        },
        "G2-V20-17", "observer safety or duration requirements changed",
    )
    require(contract["mandatory_faults"] == FAULTS, "G2-V20-18", "mandatory fault matrix changed")

    evidence = contract["evidence"]
    require(
        evidence == {
            "required_nodes": ["sensor-rpi4", "blackbox", "pfd-01", "mfd-01", "ems-01"],
            "required_artifacts": ["preflight", "capture", "recorder-digest", "replay-report", "fault-report", "rollback-report"],
            "sha256_required": True,
            "utc_required": True,
            "operator_required": True,
            "flight_approval": False,
        },
        "G2-V20-19", "evidence provenance requirements changed",
    )
    rollback = contract["rollback"]
    require(
        rollback == {
            "deployment_allowed_by_this_contract": False,
            "baseline_snapshot_required": True,
            "service_restart_limit": 0,
            "restore_procedure_required": True,
        },
        "G2-V20-20", "go/no-go or rollback boundary changed",
    )

    controls = contract["controls"]
    expected_registry = expected_controls()
    require(
        isinstance(controls, list) and len(controls) == len(expected_registry),
        "G2-V20-01", "control registry must contain exactly 20 controls",
    )
    for expected, actual in zip(expected_registry, controls):
        require(actual == expected, expected["id"], "control registry entry changed")
    require(
        network.get("schema") == "efis-next.avionics-network.v1"
        and network.get("status") == "BENCH_NOT_FLIGHT_APPROVED"
        and icd.get("transport", {}).get("mode") == "UDP_UNICAST"
        and icd.get("transport", {}).get("authentication") == "NONE",
        "G2-V20-20", "network/transport approval boundary changed",
    )

    return {
        "schema": contract["schema"],
        "status": "PASS",
        "execution_status": "NOT_RUN",
        "execution_authorized": False,
        "controls_passed": len(CONTROL_SPECS),
        "controls_total": len(CONTROL_SPECS),
        "blocking_dependency": "AVX-G1-01/02/03",
        "next_gate": "USER_APPROVAL_AFTER_G1",
        "flight_approval": False,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("contract", nargs="?", type=Path, default=DEFAULT_CONTRACT)
    parser.add_argument("--network-plan", type=Path, default=DEFAULT_NETWORK)
    parser.add_argument("--icd", type=Path, default=DEFAULT_ICD)
    args = parser.parse_args(argv)
    try:
        report = validate(load_json(args.contract), load_json(args.network_plan), load_json(args.icd))
    except (OSError, json.JSONDecodeError, ContractError, KeyError, TypeError) as error:
        print(f"G2 vertical acceptance invalid: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
