#!/usr/bin/env python3
"""Fail-closed validation of the G1 MokerLink bench-acceptance package."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTRACT = ROOT / "config/network/avionix-g1-switch-acceptance-v1.json"
DEFAULT_RESULTS = ROOT / "config/network/avionix-g1-switch-results-not-run-v1.json"
UPSTREAM = ROOT / "config/network/avionix-g1-upstream-proposal-v1.json"

CONTROL_SPECS = [
    (f"G1-S20-{number:02d}", key, "DEFINED")
    for number, key in enumerate([
        "contract_identity", "closed_schema", "prepared_not_run", "zero_mutation_authority",
        "candidate_cross_check", "installed_switch_unchanged", "port_map_exact",
        "sfp_excluded_from_copper", "capacity_theoretical_only", "flight_data_only",
        "video_physically_separate", "modes_forbidden", "identity_photos_required",
        "all_rj45_link_speed_required", "power_brownout_required",
        "current_thermal_restart_required", "fault_recovery_required", "evidence_provenance",
        "not_run_has_no_claims", "bench_pass_not_flight_approval",
    ], 1)
]

EVIDENCE = [
    "MODEL_LABEL_PHOTO", "TERMINAL_AND_POWER_LABEL_PHOTO",
    "PORT_MAP_AND_MODE_SWITCH_PHOTO", "ALL_RJ45_PORTS_LINK_TEST",
    "NEGOTIATED_SPEED_PER_PORT", "MIN_MAX_INPUT_AND_BROWNOUT_TEST",
    "CURRENT_TEMPERATURE_AND_RESTART_REPORT",
    "FLIGHT_DATA_VIDEO_PHYSICAL_ISOLATION_TEST", "FAULT_AND_RECOVERY_TEST",
    "OPERATOR_SIGNOFF",
]


class AcceptanceError(ValueError):
    pass


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise AcceptanceError(f"{control}: {message}")


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise AcceptanceError(f"{path}: root must be an object")
    return value


def expected_controls() -> list[dict]:
    return [{"id": i, "key": k, "state": s} for i, k, s in CONTROL_SPECS]


def safe_artifact_path(value: str) -> bool:
    path = PurePosixPath(value)
    return bool(value) and not path.is_absolute() and ".." not in path.parts and "\\" not in value


def validate(contract: dict, results: dict, upstream: dict, evidence_root: Path | None = None) -> dict:
    require(set(contract) == {"schema", "revision", "status", "execution_authorized", "flight_approval", "candidate", "authority", "scope", "expected_ports", "vendor_claims", "required_evidence", "result_contract", "controls"}, "G1-S20-02", "contract properties differ from V1")
    require((contract["schema"], contract["revision"]) == ("avionix-next.g1-switch-acceptance.v1", "1.0.0"), "G1-S20-01", "contract identity changed")
    require(contract["controls"] == expected_controls(), "G1-S20-01", "control catalog changed")
    require(contract["status"] == "PREPARED_SW_NOT_RUN" and contract["execution_authorized"] is False and contract["flight_approval"] is False, "G1-S20-03", "contract was promoted")
    require(contract["authority"] == "READ_ONLY_BENCH_ACCEPTANCE_NO_NETWORK_MUTATION" and contract["execution_authorized"] is False, "G1-S20-04", "mutation authority introduced")

    candidate = contract["candidate"]
    upstream_candidate = upstream["candidate_switch"]
    require(candidate["manufacturer"] == upstream_candidate["manufacturer"] and candidate["model"] == upstream_candidate["model"] and candidate["identity_status"] == upstream_candidate["identity_status"], "G1-S20-05", "candidate does not match upstream proposal")
    require(upstream["network_capacity"]["installed_switch"] == "Waveshare Gigabit-Switch-5P (EU)" and candidate["installed"] is False and candidate["replaces_installed_switch"] is False, "G1-S20-06", "installed topology was changed")

    ports = contract["expected_ports"]
    require(ports["sfp_counts_as_copper"] is False, "G1-S20-08", "SFP counted as copper")
    require(ports == {"rj45_fast_ethernet": 8, "rj45_gigabit": 1, "sfp_gigabit": 1, "copper_total": 9, "sfp_counts_as_copper": False, "required_copper_ports": 7, "theoretical_margin_ports": 2}, "G1-S20-07", "expected port map changed")
    require(ports["copper_total"] - ports["required_copper_ports"] == ports["theoretical_margin_ports"] and upstream_candidate["capacity_assessment"]["status"] == "CANDIDATE_CAPACITY_UNVERIFIED", "G1-S20-09", "theoretical capacity was promoted")

    scope = contract["scope"]
    require(scope["segment"] == "flight-data" and scope["ip_change"] == "FORBIDDEN" and scope["firewall_change"] == "FORBIDDEN", "G1-S20-10", "scope is not flight-data/read-only")
    require(scope["video_attachment"] == "FORBIDDEN" and scope["camera_attachment"] == "FORBIDDEN", "G1-S20-11", "VIDEO isolation weakened")
    require(scope["vlan_mode"].startswith("FORBIDDEN") and scope["extend_mode"] == "FORBIDDEN", "G1-S20-12", "unverified switch mode allowed")
    require(contract["required_evidence"][:3] == EVIDENCE[:3], "G1-S20-13", "identity photo set incomplete")
    require(all(item in contract["required_evidence"] for item in EVIDENCE[3:5]), "G1-S20-14", "RJ45 link/speed evidence incomplete")
    require("MIN_MAX_INPUT_AND_BROWNOUT_TEST" in contract["required_evidence"], "G1-S20-15", "power evidence missing")
    require("CURRENT_TEMPERATURE_AND_RESTART_REPORT" in contract["required_evidence"], "G1-S20-16", "thermal/restart evidence missing")
    require("FAULT_AND_RECOVERY_TEST" in contract["required_evidence"], "G1-S20-17", "fault/recovery evidence missing")

    require(set(results) == {"schema", "contract_revision", "candidate", "status", "operator", "started_utc", "completed_utc", "operator_confirmed", "hardware_verified", "flight_approval", "measurements", "artifacts"}, "G1-S20-18", "result properties differ from V1")
    require(results["schema"] == "avionix-next.g1-switch-results.v1" and results["contract_revision"] == contract["revision"], "G1-S20-18", "result identity/revision mismatch")
    require(results["candidate"] == {"manufacturer": "MokerLink", "model": "IND-F082GS"}, "G1-S20-05", "result candidate mismatch")
    require(results["flight_approval"] is False, "G1-S20-20", "bench evidence must never grant flight approval")

    if results["status"] == "NOT_RUN":
        require(results["operator"] is None and results["started_utc"] is None and results["completed_utc"] is None and results["operator_confirmed"] is False and results["hardware_verified"] is False and results["measurements"] == {} and results["artifacts"] == [], "G1-S20-19", "NOT_RUN contains claims or evidence")
    else:
        require(results["status"] in {"BENCH_PASS", "FAIL"}, "G1-S20-18", "unsupported result status")
        require(isinstance(results["operator"], str) and results["operator"].strip() and results["operator_confirmed"] is True, "G1-S20-18", "operator provenance missing")
        require(isinstance(results["started_utc"], str) and results["started_utc"].endswith("Z") and isinstance(results["completed_utc"], str) and results["completed_utc"].endswith("Z"), "G1-S20-18", "UTC timestamps missing")
        artifacts = results["artifacts"]
        require(len(artifacts) == len(EVIDENCE) and {a.get("kind") for a in artifacts} == set(EVIDENCE), "G1-S20-18", "evidence set incomplete")
        for artifact in artifacts:
            require(set(artifact) == {"kind", "path", "sha256"} and safe_artifact_path(artifact["path"]) and len(artifact["sha256"]) == 64, "G1-S20-18", "unsafe or malformed artifact")
            if evidence_root is not None:
                path = evidence_root / artifact["path"]
                require(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == artifact["sha256"], "G1-S20-18", "artifact missing or digest mismatch")
        if results["status"] == "BENCH_PASS":
            measurements = results["measurements"]
            require(results["hardware_verified"] is True and measurements.get("copper_ports_tested") == 9 and measurements.get("fe_ports_at_100mbps") == 8 and measurements.get("ge_ports_at_1000mbps") == 1, "G1-S20-14", "port acceptance incomplete")
            require(measurements.get("power_brownout_pass") is True, "G1-S20-15", "power/brownout did not pass")
            require(measurements.get("thermal_restart_pass") is True, "G1-S20-16", "thermal/restart did not pass")
            require(measurements.get("fault_recovery_pass") is True and measurements.get("video_isolation_pass") is True, "G1-S20-17", "fault/isolation did not pass")
    require(contract["result_contract"] == {"allowed_status": ["NOT_RUN", "BENCH_PASS", "FAIL"], "artifact_hash": "SHA-256", "timestamp": "UTC_RFC3339", "bench_pass_is_flight_approval": False, "missing_evidence_policy": "FAIL_CLOSED"}, "G1-S20-20", "result safety policy changed")
    return {"schema": contract["schema"], "controls": len(contract["controls"]), "result_status": results["status"], "flight_approval": False, "status": "PASS"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    parser.add_argument("--results", type=Path, default=DEFAULT_RESULTS)
    parser.add_argument("--evidence-root", type=Path)
    args = parser.parse_args()
    report = validate(load_json(args.contract), load_json(args.results), load_json(UPSTREAM), args.evidence_root)
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
