#!/usr/bin/env python3
"""Validate the non-executable G1 sensor-node upstream architecture proposal."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PROPOSAL = ROOT / "config/network/avionix-g1-upstream-proposal-v1.json"
NETWORK_PLAN = ROOT / "config/network/avionics-network-v1.json"
G2_VERTICAL = ROOT / "config/network/avionix-g2-vertical-acceptance-v1.json"
READINESS = ROOT / "config/hardware/avionix-g1-g2-readiness-v1.json"
ICD = ROOT / "config/protocols/avionix-data-icd-v1.json"
TYPES = ROOT / "include/efis/core/types.hpp"
CONTROL_DIGEST = "1fe14ce2eef497a5c5e09a7ad89f7bfd756b21cc05d0e792c36cc76933b59786"

CONTROL_SPECS = [
    ("G1-U20-01", "proposal_identity", "DEFINED"),
    ("G1-U20-02", "not_approved_not_executable", "DEFINED"),
    ("G1-U20-03", "sensor_node_identity", "PROPOSED"),
    ("G1-U20-04", "blackbox_destination", "DEFINED"),
    ("G1-U20-05", "separate_raw_protocol", "PROPOSED"),
    ("G1-U20-06", "udp_unicast_port_reservation", "PROPOSED"),
    ("G1-U20-07", "one_way_no_command", "DEFINED"),
    ("G1-U20-08", "blackbox_normalization_authority", "DEFINED"),
    ("G1-U20-09", "raw_payload_minimum", "DEFINED"),
    ("G1-U20-10", "source_id_append_only_reservation", "PROPOSED"),
    ("G1-U20-11", "legacy_alias_forbidden", "DEFINED"),
    ("G1-U20-12", "crc_sequence_boot_identity", "DEFINED"),
    ("G1-U20-13", "publish_rate_not_invented", "BLOCKED_HARDWARE"),
    ("G1-U20-14", "freshness_not_invented", "BLOCKED_HARDWARE"),
    ("G1-U20-15", "dual_raw_recording", "DEFINED"),
    ("G1-U20-16", "same_dataset_replay", "DEFINED"),
    ("G1-U20-17", "negative_paths", "DEFINED"),
    ("G1-U20-18", "switch_capacity_blocker", "BLOCKED_ARCHITECTURE"),
    ("G1-U20-19", "deployment_blockers_complete", "DEFINED"),
    ("G1-U20-20", "no_go_decision", "NO_GO"),
]


class ProposalError(ValueError):
    """The proposed architecture violates a frozen safety boundary."""


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise ProposalError(f"{control}: {message}")


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ProposalError(f"{path}: root must be an object")
    return value


def expected_controls() -> list[dict]:
    return [{"id": identifier, "key": key, "state": state} for identifier, key, state in CONTROL_SPECS]


def catalog_digest(controls: list[dict]) -> str:
    encoded = json.dumps(controls, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def validate(proposal: dict, network: dict, vertical: dict, readiness: dict, icd: dict, types_text: str) -> dict:
    require(
        set(proposal) == {
            "schema", "revision", "status", "execution_authorized", "flight_approval",
            "source", "destination", "transport", "authority", "source_id", "payload",
            "integrity", "recording", "safety", "deployment_blockers", "network_capacity",
            "candidate_switch", "decision", "controls",
        },
        "G1-U20-01", "root properties differ from V1",
    )
    require(
        (proposal["schema"], proposal["revision"]) == ("avionix-next.g1-upstream-proposal.v1", "1.1.0"),
        "G1-U20-01", "proposal identity differs from V1",
    )
    require(proposal["controls"] == expected_controls(), "G1-U20-01", "control catalog changed")
    require(catalog_digest(proposal["controls"]) == CONTROL_DIGEST, "G1-U20-01", "control digest changed")
    require(
        proposal["status"] == "PROPOSED_NOT_APPROVED"
        and proposal["execution_authorized"] is False
        and proposal["flight_approval"] is False,
        "G1-U20-02", "proposal was promoted or authorized",
    )
    require(
        proposal["source"] == {
            "node": "sensor-rpi4", "role": "ASSO5-SENSOR-NODE",
            "proposed_address": "10.77.10.10", "address_status": "RESERVED_NOT_CONFIGURED",
        },
        "G1-U20-03", "source identity/address changed",
    )
    require(
        proposal["destination"] == {"node": "blackbox", "role": "broker-recorder", "address": "10.77.10.2"},
        "G1-U20-04", "destination is not the Blackbox broker",
    )
    transport = proposal["transport"]
    require(
        transport == {
            "protocol": "ERAP_V1", "protocol_name": "Engine Raw Acquisition Protocol V1",
            "protocol_status": "PROPOSED_NOT_FROZEN", "network": "UDP_UNICAST", "port": 49004,
            "port_status": "RESERVED_NOT_BOUND", "direction": "ONE_WAY", "command_path": "ABSENT",
            "payload_authority": "RAW_ACQUISITION_ONLY",
            "publish_rate_hz": "UNRESOLVED_PENDING_HARDWARE_CHARACTERIZATION",
            "freshness_thresholds": "UNRESOLVED_PENDING_RATE_QUALIFICATION",
        },
        "G1-U20-05", "raw transport proposal changed",
    )
    require(transport["port"] != 49003, "G1-U20-06", "upstream must not reuse EDP display port")
    require(transport["direction"] == "ONE_WAY" and transport["command_path"] == "ABSENT", "G1-U20-07", "command path introduced")

    require(
        proposal["authority"] == {
            "acquisition": "sensor-rpi4", "raw_health": "sensor-rpi4", "normalization": "blackbox",
            "calibration_acceptance": "blackbox", "arbitration": "blackbox",
            "downstream_publication": "blackbox", "display": "RECEIVE_ONLY",
        },
        "G1-U20-08", "authority boundary changed",
    )
    required_fields = [
        "schema_version", "node_id", "boot_id", "sequence", "timestamp_monotonic",
        "timestamp_utc", "channel_id", "raw_value", "raw_unit", "adapter_health",
        "calibration_candidate_revision", "hardware_revision", "simulation",
    ]
    require(
        proposal["payload"] == {
            "engineering_value_authoritative": False, "required_fields": required_fields,
            "unknown_channel_policy": "REJECT", "missing_identity_policy": "REJECT",
            "simulation_policy": "EXPLICIT_AND_NON_REAL",
        },
        "G1-U20-09", "raw payload contract changed",
    )
    require(
        proposal["source_id"] == {
            "proposed_key": "asso5_engine", "proposed_numeric_value": 9,
            "status": "RESERVED_PENDING_ICD_REVIEW", "append_only": True,
            "frozen_icd_v1_unchanged": True, "runtime_use_allowed": False,
            "legacy_alias_allowed": False,
        },
        "G1-U20-10", "SourceId reservation changed",
    )
    frozen_sources = icd["enumerations"]["source_id"]
    require("asso5_engine" not in frozen_sources and frozen_sources["blackbox_broker"] == 8, "G1-U20-10", "frozen ICD was silently changed")
    require("asso5_engine" not in types_text, "G1-U20-10", "runtime SourceId was added before review")
    require(proposal["source_id"]["legacy_alias_allowed"] is False, "G1-U20-11", "legacy alias allowed")

    require(
        proposal["integrity"] == {
            "byte_order": "BIG_ENDIAN", "payload_crc": "CRC32C_CASTAGNOLI", "boot_id_required": True,
            "sequence_policy": "REJECT_DUPLICATE_OR_OUT_OF_ORDER", "monotonic_time_required": True,
            "unknown_schema_policy": "REJECT",
        },
        "G1-U20-12", "integrity contract changed",
    )
    require("UNRESOLVED" in transport["publish_rate_hz"], "G1-U20-13", "publish rate was invented")
    require("UNRESOLVED" in transport["freshness_thresholds"], "G1-U20-14", "freshness was invented")
    require(
        proposal["recording"] == {
            "sensor_local_raw_log_required": True, "blackbox_raw_ingest_log_required": True,
            "normalized_log_required": True, "calibration_revision_required": True,
            "sha256_evidence_required": True, "same_dataset_replay_required": True,
        },
        "G1-U20-15", "recording/replay contract changed",
    )
    require(proposal["recording"]["same_dataset_replay_required"] is True, "G1-U20-16", "same-dataset replay disabled")
    require(
        proposal["safety"] == {
            "pixhawk_path": "ABSENT", "mavlink_path": "ABSENT", "display_direct_path": "FORBIDDEN",
            "routing_bridge_nat": "FORBIDDEN", "register_write": "FORBIDDEN",
            "output_activation": "FORBIDDEN", "unknown_calibration_policy": "UNAVAILABLE",
            "firewall_default": "DROP_EXCEPT_SENSOR_TO_BLACKBOX_UDP_49004",
        },
        "G1-U20-17", "negative-path safety changed",
    )

    flight_switch = next(item for item in network["switches"] if item["id"] == "flight-switch")
    capacity = proposal["network_capacity"]
    require(
        flight_switch["prototype_ports"] == 5 and flight_switch["minimum_operational_ports"] == 6,
        "G1-U20-18", "authoritative switch capacity changed",
    )
    require(
        capacity == {
            "installed_switch": "Waveshare Gigabit-Switch-5P (EU)", "installed_ports": 5,
            "required_operational_ports_before_sensor_node": 6,
            "required_operational_ports_with_sensor_node": 7,
            "status": "BLOCKED_CAPACITY_REVIEW",
            "candidate_status": "CANDIDATE_CAPACITY_UNVERIFIED",
        },
        "G1-U20-18", "switch capacity blocker changed",
    )
    candidate = proposal["candidate_switch"]
    require(
        candidate == {
            "manufacturer": "MokerLink",
            "model": "IND-F082GS",
            "identity_status": "PRESUMED_FROM_MARKETING_TITLE_NOT_LABEL_VERIFIED",
            "intended_segment": "flight-data",
            "deployment_status": "CANDIDATE_NOT_PHYSICALLY_VERIFIED",
            "installed": False,
            "execution_authorized": False,
            "flight_approved": False,
            "management": "UNMANAGED",
            "ports": {
                "rj45_fast_ethernet": 8, "rj45_gigabit": 1, "sfp_gigabit": 1,
                "copper_total": 9, "sfp_counts_as_copper": False,
            },
            "power_input": {
                "minimum_vdc": 12, "maximum_vdc": 55,
                "redundant_inputs_claimed": True,
                "status": "VENDOR_CLAIM_NOT_BENCH_VERIFIED",
            },
            "environment": {
                "minimum_operating_c": -40, "maximum_operating_c": 75,
                "ingress_protection_claimed": "IP40",
                "status": "VENDOR_CLAIM_NOT_QUALIFICATION",
            },
            "use_constraints": {
                "flight_data_only": True, "video_segment_forbidden": True,
                "camera_attachment_forbidden": True, "extend_mode_forbidden": True,
                "vlan_mode_forbidden_until_verified": True,
                "sfp_not_counted_without_transceiver": True,
            },
            "capacity_assessment": {
                "required_copper_ports": 7, "candidate_copper_ports": 9,
                "numerical_margin_ports": 2,
                "status": "CANDIDATE_CAPACITY_UNVERIFIED",
            },
            "required_evidence": [
                "MODEL_LABEL_PHOTO", "TERMINAL_AND_POWER_LABEL_PHOTO",
                "PORT_MAP_AND_MODE_SWITCH_PHOTO", "ALL_RJ45_PORTS_LINK_TEST",
                "NEGOTIATED_SPEED_PER_PORT", "MIN_MAX_INPUT_AND_BROWNOUT_TEST",
                "CURRENT_TEMPERATURE_AND_RESTART_REPORT",
                "FLIGHT_DATA_VIDEO_PHYSICAL_ISOLATION_TEST",
                "FAULT_AND_RECOVERY_TEST",
            ],
        },
        "G1-U20-18", "candidate switch registration changed or promoted",
    )
    require(
        candidate["ports"]["copper_total"] >= capacity["required_operational_ports_with_sensor_node"]
        and candidate["capacity_assessment"]["status"] == "CANDIDATE_CAPACITY_UNVERIFIED",
        "G1-U20-18", "candidate capacity was hidden or promoted",
    )
    require(not any(node["id"] == "sensor-rpi4" for node in network["nodes"]), "G1-U20-18", "network plan was deployed prematurely")
    require(not any(flow.get("port") == 49004 for flow in network["flows"]), "G1-U20-18", "port 49004 was activated prematurely")

    blockers = [
        "PHYSICAL_BOARD_INVENTORY_0_OF_4", "CHANNEL_MAP_UNRESOLVED", "CALIBRATION_UNRESOLVED",
        "SOURCE_ID_NOT_IN_FROZEN_ICD", "ERAP_V1_NOT_FROZEN", "PUBLISH_RATE_UNRESOLVED",
        "FRESHNESS_THRESHOLDS_UNRESOLVED", "FLIGHT_SWITCH_CAPACITY_INSUFFICIENT",
        "FLIGHT_SWITCH_CANDIDATE_NOT_VERIFIED",
        "LIVE_RUNBOOK_NOT_APPROVED",
    ]
    require(proposal["deployment_blockers"] == blockers, "G1-U20-19", "deployment blockers changed")
    require(
        vertical["topology"]["source_to_blackbox"]["transport"] == "UNRESOLVED"
        and vertical["topology"]["source_to_blackbox"]["execution_allowed"] is False,
        "G1-U20-19", "G2 contract was resolved without approval",
    )
    require(
        readiness["unresolved"]["real_source_id"] == "UNRESOLVED_PENDING_ARCHITECTURE_DECISION"
        and readiness["unresolved"]["source_to_blackbox_transport"] == "UNRESOLVED_PENDING_G1_DESIGN",
        "G1-U20-19", "R100 unresolved boundary was changed",
    )
    require(
        proposal["decision"] == "NO_GO_PENDING_ARCHITECTURE_AND_HARDWARE_REVIEW"
        and proposal["flight_approval"] is False,
        "G1-U20-20", "NO-GO boundary changed",
    )
    return {
        "schema": proposal["schema"], "status": "PASS", "proposal_status": proposal["status"],
        "controls_validated": 20, "controls_total": 20, "proposed_protocol": transport["protocol"],
        "proposed_port": transport["port"], "source_id_status": proposal["source_id"]["status"],
        "switch_capacity": capacity["status"], "execution_authorized": False,
        "candidate_switch": candidate["model"],
        "candidate_switch_status": candidate["deployment_status"],
        "flight_approval": False, "decision": proposal["decision"],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("proposal", nargs="?", type=Path, default=DEFAULT_PROPOSAL)
    args = parser.parse_args(argv)
    try:
        report = validate(
            load_json(args.proposal), load_json(NETWORK_PLAN), load_json(G2_VERTICAL),
            load_json(READINESS), load_json(ICD), TYPES.read_text(encoding="utf-8"),
        )
    except (OSError, json.JSONDecodeError, ProposalError, KeyError, StopIteration, TypeError) as error:
        print(f"G1 upstream proposal invalid: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
