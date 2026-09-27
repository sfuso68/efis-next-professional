#!/usr/bin/env python3
"""Validate the five non-executable product/qualification contracts."""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTRACT = ROOT / "config/product/avionix-product-contracts-batch-v1.json"


class ContractError(ValueError):
    """A product safety boundary was weakened or became ambiguous."""


PACKAGE_SPECS = [
    ("PB-EMS-AUTHORITY", "PB-EMS"),
    ("PB-PRODUCT-SKU", "PB-SKU"),
    ("PB-FPV-FOUR", "PB-FPV"),
    ("PB-VOICE-OFFLINE", "PB-VOI"),
    ("PB-HIL-FAULTS", "PB-HIL"),
]


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ContractError(f"{path}: root must be an object")
    return value


def require(condition: bool, control_id: str, message: str) -> None:
    if not condition:
        raise ContractError(f"{control_id}: {message}")


def safe_repo_path(value: object) -> bool:
    if not isinstance(value, str) or not value or "\\" in value:
        return False
    path = PurePosixPath(value)
    return not path.is_absolute() and ".." not in path.parts


def validate(contract: dict, root: Path = ROOT) -> dict:
    require(
        set(contract) == {"schema", "revision", "status", "execution_authorized", "flight_approval", "authority", "packages", "decision"},
        "PB-ROOT-01", "contract root keys differ from V1",
    )
    require(
        (contract["schema"], contract["revision"]) == ("avionix-next.product-contracts-batch.v1", "1.0.0"),
        "PB-ROOT-01", "contract identity differs from V1",
    )
    require(
        contract["status"] == "PREPARED_SW_NOT_RUN"
        and contract["execution_authorized"] is False
        and contract["flight_approval"] is False,
        "PB-ROOT-02", "batch must remain non-executable and non-flight-approved",
    )
    require(
        contract["decision"] == "NO_GO_PENDING_SOURCES_AND_INTERACTIVE_HARDWARE_EVIDENCE",
        "PB-ROOT-03", "decision must remain NO-GO",
    )

    authority = contract["authority"]
    require(isinstance(authority, dict) and len(authority) == 6, "PB-ROOT-04", "authority registry changed")
    for name, value in authority.items():
        require(safe_repo_path(value), "PB-ROOT-04", f"unsafe authority path: {name}")
        require((root / value).is_file(), "PB-ROOT-04", f"missing authority file: {value}")

    packages = contract["packages"]
    require(isinstance(packages, list) and len(packages) == 5, "PB-ROOT-05", "exactly five packages are required")
    state_counts: dict[str, int] = {}
    for package, (package_id, prefix) in zip(packages, PACKAGE_SPECS):
        require(
            isinstance(package, dict) and set(package) == {"id", "title", "state", "dependencies", "invariants", "controls"},
            f"{prefix}-00", "package shape changed",
        )
        require(package["id"] == package_id, f"{prefix}-00", "package order or identity changed")
        require(package["state"] == "PREPARED_SW_NOT_RUN", f"{prefix}-00", "package state changed")
        require(
            isinstance(package["dependencies"], list) and len(package["dependencies"]) == 3
            and all(isinstance(item, str) and item for item in package["dependencies"]),
            f"{prefix}-00", "dependency registry changed",
        )
        controls = package["controls"]
        require(isinstance(controls, list) and len(controls) == 10, f"{prefix}-00", "control count changed")
        expected_ids = [f"{prefix}-{index:02d}" for index in range(1, 11)]
        require([item.get("id") for item in controls] == expected_ids, f"{prefix}-00", "control IDs changed")
        for item in controls:
            require(set(item) == {"id", "key", "state"}, item.get("id", prefix), "control shape changed")
            require(item["state"] in {"DEFINED", "VERIFY_HW", "BLOCKED_DEPENDENCY", "NO_GO"}, item["id"], "unsupported state")
            state_counts[item["state"]] = state_counts.get(item["state"], 0) + 1
        require(controls[-1] == {"id": expected_ids[-1], "key": "go_no_go", "state": "NO_GO"}, f"{prefix}-10", "final gate changed")

    ems, sku, fpv, voice, hil = packages
    require(
        ems["invariants"] == {
            "supported_engine_models": ["ROTAX_912_UL", "ROTAX_912_ULS"],
            "active_profile_state": "SIMULATION",
            "active_limits_status": "INDICATIVE_UNVERIFIED",
            "unverified_limits_may_drive_normal_cas": False,
            "manual_page_reference_required": True,
            "engine_serial_required": True,
            "independent_profile_per_engine_model": True,
        }, "PB-EMS-01", "EMS authority boundary changed",
    )
    active = load_json(root / authority["active_aircraft_profile"])
    require(active.get("state") == "SIMULATION", "PB-EMS-07", "active profile is not simulation")
    require(active.get("engine", {}).get("limits", {}).get("status") == "INDICATIVE_UNVERIFIED", "PB-EMS-06", "active limit status drifted")
    require(active.get("engine", {}).get("serial_number") == "TO_BE_ENTERED", "PB-EMS-02", "serial changed without source review")
    require(active.get("engine", {}).get("manual_reference") == "TO_BE_VERIFIED", "PB-EMS-03", "manual changed without source review")

    require(
        sku["invariants"] == {
            "single_codebase": True,
            "profiles": ["STANDARD", "PROFESSIONAL"],
            "safety_state_semantics_identical": True,
            "profile_switch_in_flight_allowed": False,
            "feature_enable_requires_manifest": True,
            "unknown_feature_policy": "REJECT",
            "missing_profile_policy": "NO_START",
        }, "PB-SKU-01", "product-profile boundary changed",
    )

    video = load_json(root / authority["video_config"])
    require(
        fpv["invariants"] == {
            "camera_count": 4,
            "decoder_platform": "RPI4",
            "ems_and_fpv_same_host_allowed": False,
            "video_and_flight_data_bridge_allowed": False,
            "operational_wifi_allowed": False,
            "rpi5_primary_decode_allowed": False,
            "stale_frame_age_visible": True,
            "recording_requires_telemetry_sidecar": True,
        }, "PB-FPV-02", "FPV isolation boundary changed",
    )
    require(len(video.get("cameras", [])) == 4, "PB-FPV-02", "video configuration is not four-feed")
    require(video.get("network", {}).get("operationalWifiAllowed") is False, "PB-FPV-04", "operational Wi-Fi enabled")
    require(video.get("recording", {}).get("recordTelemetrySidecar") is True, "PB-FPV-09", "telemetry sidecar disabled")

    voice_config = load_json(root / authority["voice_config"])
    require(
        voice["invariants"] == {
            "offline_only": True,
            "push_to_talk_only": True,
            "closed_grammar_only": True,
            "free_form_command_execution": False,
            "radio_transmission_allowed": False,
            "pixhawk_command_allowed": False,
            "actuation_allowed": False,
            "phraseology_pilot_review_required": True,
            "critical_audio_policy": "FIXED_VERSIONED_CLIPS",
            "dynamic_noncritical_audio_policy": "OFFLINE_TTS_WITH_VISUAL_FALLBACK",
        }, "PB-VOI-01", "voice/radio boundary changed",
    )
    require(voice_config.get("activation") == "PUSH_TO_TALK_ONLY", "PB-VOI-01", "voice config is not PTT-only")
    for key in ("free_form_command_execution", "radio_transmission_allowed", "pixhawk_command_allowed", "actuation_allowed"):
        require(voice_config.get(key) is False, "PB-VOI-04", f"unsafe voice config: {key}")

    require(
        hil["invariants"] == {
            "simulation_visible": True,
            "deterministic_seed_required": True,
            "scenario_sha256_required": True,
            "production_network_injection_allowed": False,
            "production_storage_fault_allowed": False,
            "actuator_output_allowed": False,
            "flight_control_command_allowed": False,
            "same_replay_all_consumers": True,
            "operator_and_utc_required": True,
        }, "PB-HIL-01", "HIL isolation boundary changed",
    )

    return {
        "schema": "avionix-next.product-contracts-batch-validation.v1",
        "status": "PASS",
        "execution_status": "NOT_RUN",
        "execution_authorized": False,
        "flight_approval": False,
        "packages": 5,
        "controls_total": 50,
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
        print(f"AVIONIX product contracts batch: FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
