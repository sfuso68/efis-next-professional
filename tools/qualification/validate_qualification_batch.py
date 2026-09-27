#!/usr/bin/env python3
"""Validate the non-executable AVIONIX five-package qualification batch."""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTRACT = ROOT / "config/qualification/avionix-qualification-batch-v1.json"


class ContractError(ValueError):
    """A qualification invariant was weakened or became ambiguous."""


PACKAGE_SPECS = [
    ("QB-G1-ASSO5", "QB-G1", 10),
    ("QB-G2-VERTICAL", "QB-G2", 10),
    ("QB-G3-AIRDATA", "QB-G3", 10),
    ("QB-G4-BLACKBOX", "QB-G4", 10),
    ("QB-G4-NAV-TAWS", "QB-G5", 10),
]

ROOT_KEYS = {
    "schema", "revision", "status", "execution_authorized", "flight_approval",
    "authority", "packages", "decision",
}


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ContractError(f"{path}: root must be an object")
    return value


def require(condition: bool, control_id: str, message: str) -> None:
    if not condition:
        raise ContractError(f"{control_id}: {message}")


def safe_repo_path(value: object) -> bool:
    if not isinstance(value, str) or not value:
        return False
    path = PurePosixPath(value)
    return not path.is_absolute() and ".." not in path.parts and "\\" not in value


def require_exact_keys(value: object, expected: set[str], control_id: str) -> dict:
    require(isinstance(value, dict), control_id, "value must be an object")
    assert isinstance(value, dict)
    require(set(value) == expected, control_id, "missing or unknown properties")
    return value


def validate(contract: dict, root: Path = ROOT) -> dict:
    require(set(contract) == ROOT_KEYS, "QB-ROOT-01", "contract root keys differ from V1")
    require(
        (contract.get("schema"), contract.get("revision")) ==
        ("avionix-next.qualification-batch.v1", "1.0.0"),
        "QB-ROOT-01", "contract identity differs from V1",
    )
    require(
        contract.get("status") == "PREPARED_SW_NOT_RUN"
        and contract.get("execution_authorized") is False
        and contract.get("flight_approval") is False,
        "QB-ROOT-02", "the batch must remain non-executable and non-flight-approved",
    )
    require(
        contract.get("decision") == "NO_GO_PENDING_INTERACTIVE_HARDWARE_EVIDENCE",
        "QB-ROOT-03", "batch decision must remain NO-GO",
    )

    authority = require_exact_keys(
        contract.get("authority"),
        {"project_plan", "master_matrix", "g1_g2_readiness", "g2_vertical"},
        "QB-ROOT-04",
    )
    for key, value in authority.items():
        require(safe_repo_path(value), "QB-ROOT-04", f"unsafe authority path: {key}")
        require((root / value).is_file(), "QB-ROOT-04", f"missing authority file: {value}")

    readiness = load_json(root / authority["g1_g2_readiness"])
    vertical = load_json(root / authority["g2_vertical"])
    packages = contract.get("packages")
    require(isinstance(packages, list) and len(packages) == 5, "QB-ROOT-05", "exactly five packages are required")
    assert isinstance(packages, list)

    total_controls = 0
    state_counts: dict[str, int] = {}
    evidence_names: set[tuple[str, str]] = set()
    for package, (package_id, prefix, count) in zip(packages, PACKAGE_SPECS):
        control_id = f"{prefix}-00"
        package = require_exact_keys(
            package,
            {"id", "title", "state", "dependencies", "invariants", "evidence_required", "controls"},
            control_id,
        )
        require(package["id"] == package_id, control_id, "package order or identity changed")
        require(package["state"] == "PREPARED_SW_NOT_RUN", control_id, "package is not prepared/not-run")
        require(
            isinstance(package["dependencies"], list) and len(package["dependencies"]) >= 3
            and all(isinstance(item, str) and item for item in package["dependencies"]),
            control_id, "dependencies are incomplete",
        )
        require(
            isinstance(package["evidence_required"], list) and len(package["evidence_required"]) >= 6,
            control_id, "evidence set is incomplete",
        )
        for artifact in package["evidence_required"]:
            require(isinstance(artifact, str) and artifact, control_id, "invalid evidence name")
            require((package_id, artifact) not in evidence_names, control_id, "duplicate evidence name")
            evidence_names.add((package_id, artifact))

        controls = package["controls"]
        require(isinstance(controls, list) and len(controls) == count, control_id, "control count changed")
        expected_ids = [f"{prefix}-{index:02d}" for index in range(1, count + 1)]
        require([item.get("id") for item in controls] == expected_ids, control_id, "control registry changed")
        for item in controls:
            require(set(item) == {"id", "key", "state"}, item.get("id", control_id), "control shape changed")
            require(
                item["state"] in {"DEFINED", "VERIFY_HW", "BLOCKED_DEPENDENCY", "NO_GO"},
                item["id"], "unsupported control state",
            )
            require(isinstance(item["key"], str) and item["key"], item["id"], "empty control key")
            state_counts[item["state"]] = state_counts.get(item["state"], 0) + 1
        require(controls[-1] == {"id": expected_ids[-1], "key": "go_no_go", "state": "NO_GO"}, control_id, "final gate changed")
        total_controls += len(controls)

    g1, g2, air, blackbox, nav = packages
    require(
        g1["invariants"] == {
            "expected_boards": ["SM-I-018:1", "SM-I-011:2", "SM-I-019:1"],
            "engine_signal_count": 18,
            "physical_channels": "UNRESOLVED",
            "calibration_coefficients": "UNRESOLVED",
            "threshold_invention_allowed": False,
            "register_write_allowed": False,
            "wiring_or_dip_change_allowed": False,
        },
        "QB-G1-03", "ASSO5 mapping/calibration safety boundary changed",
    )
    expected_boards = [f"{item['model']}:{item['quantity']}" for item in readiness["verified_software_baseline"]["expected_boards"]]
    require(g1["invariants"]["expected_boards"] == expected_boards, "QB-G1-01", "board list drifted from G1 readiness")
    require(readiness["unresolved"]["verified_physical_boards"] == 0, "QB-G1-01", "G1 readiness unexpectedly claims physical boards")
    require(len(readiness["engine_signals"]) == 18, "QB-G1-04", "engine signal list drifted")
    require(
        all(item["physical_channel"] == "UNRESOLVED" and item["calibration_revision"] == "UNRESOLVED" for item in readiness["engine_signals"]),
        "QB-G1-03", "a mapping/calibration value exists without this batch's evidence",
    )

    require(
        g2["invariants"] == {
            "source_to_blackbox": "UNRESOLVED",
            "publisher_authority": "BLACKBOX_BROKER",
            "display_authority": "RECEIVE_ONLY",
            "command_or_actuation_path": "ABSENT",
            "recording_sha256_required": True,
            "cross_consumer_outcome_match_required": True,
            "mandatory_fault_count": 12,
        },
        "QB-G2-01", "G2 vertical/replay safety boundary changed",
    )
    require(vertical["topology"]["source_to_blackbox"]["transport"] == "UNRESOLVED", "QB-G2-01", "G2 authority transport drifted")
    require(len(vertical["mandatory_faults"]) == 12, "QB-G2-09", "G2 fault matrix drifted")
    require(vertical["recording_replay"]["replay_uses_same_recording"] is True, "QB-G2-07", "same-recording invariant weakened")

    require(
        air["invariants"] == {
            "sensor": "MATEK_ASPD_DLVR",
            "qualification_mode": "PASSIVE_OR_OFFLINE_ONLY",
            "parameter_write_allowed": False,
            "output_activation_allowed": False,
            "ground_speed_as_ias_allowed": False,
            "coefficients": "UNRESOLVED",
            "temperature_compensation": "UNRESOLVED",
        },
        "QB-G3-01", "air-data qualification safety boundary changed",
    )
    require(
        blackbox["invariants"] == {
            "minimum_endurance_seconds": 3600,
            "production_destructive_fault_allowed": False,
            "power_cut_allowed": False,
            "recorder_continuity_required": True,
            "retained_artifact_sha256_required": True,
            "replay_parity_required": True,
        },
        "QB-G4-02", "Blackbox endurance/recovery safety boundary changed",
    )
    require(
        nav["invariants"] == {
            "missing_terrain_policy": "TAWS_UNAVAILABLE_NEVER_SAFE",
            "expired_terrain_policy": "TAWS_UNAVAILABLE_NEVER_SAFE",
            "missing_nav_policy": "NAV_FIELDS_INVALID",
            "stale_weather_safety_use_allowed": False,
            "simulation_must_be_visible": True,
            "command_or_actuation_authority": False,
            "provenance_and_freshness_required": True,
        },
        "QB-G5-03", "NAV/TAWS fail-closed boundary changed",
    )

    require(total_controls == 50, "QB-ROOT-05", "qualification control total changed")
    return {
        "schema": "avionix-next.qualification-batch-validation.v1",
        "status": "PASS",
        "execution_status": "NOT_RUN",
        "execution_authorized": False,
        "flight_approval": False,
        "packages": len(packages),
        "controls_total": total_controls,
        "control_states": dict(sorted(state_counts.items())),
        "decision": contract["decision"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    args = parser.parse_args()
    try:
        report = validate(load_json(args.contract))
    except (ContractError, KeyError, TypeError, json.JSONDecodeError, OSError) as exc:
        print(f"AVIONIX qualification batch: FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
