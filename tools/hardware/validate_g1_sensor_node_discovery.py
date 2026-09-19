#!/usr/bin/env python3
"""Validate the fail-closed G1 ASSO5/Sequent discovery contract."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTRACT = ROOT / "config/hardware/avionix-g1-sensor-node-discovery-v1.json"


class ContractError(ValueError):
    """The discovery contract violates a frozen read-only boundary."""


ROOT_KEYS = {
    "schema", "revision", "status", "execution_authorized", "flight_approval",
    "source_authority", "node", "expected_inventory", "discovery_stages",
    "i2c_policy", "allowed_operations", "forbidden_operations", "board_evidence",
    "power_safety", "adapter_boundary", "health_contract", "evidence",
    "go_no_go", "controls",
}

CONTROL_SPECS = [
    ("G1-D20-01", "contract_identity", "DEFINED"),
    ("G1-D20-02", "prepared_not_run", "DEFINED"),
    ("G1-D20-03", "source_authority", "DEFINED"),
    ("G1-D20-04", "node_role_unresolved_identity", "BLOCKED_HARDWARE"),
    ("G1-D20-05", "os_kernel_arch_capture", "DEFINED"),
    ("G1-D20-06", "power_storage_capture", "DEFINED"),
    ("G1-D20-07", "i2c_controller_listing_only", "DEFINED"),
    ("G1-D20-08", "four_board_count", "DEFINED"),
    ("G1-D20-09", "exact_model_quantities", "DEFINED"),
    ("G1-D20-10", "secondary_018_absence", "DEFINED"),
    ("G1-D20-11", "board_photo_labels", "DEFINED"),
    ("G1-D20-12", "dip_visual_capture_no_change", "DEFINED"),
    ("G1-D20-13", "single_power_source", "DEFINED"),
    ("G1-D20-14", "no_output_or_register_write", "DEFINED"),
    ("G1-D20-15", "no_install_or_configuration_change", "DEFINED"),
    ("G1-D20-16", "real_sim_separation", "DEFINED"),
    ("G1-D20-17", "health_state_vocabulary", "DEFINED"),
    ("G1-D20-18", "fault_catalog", "DEFINED"),
    ("G1-D20-19", "evidence_provenance", "DEFINED"),
    ("G1-D20-20", "go_no_go_boundary", "NO_GO"),
]

EXPECTED_BOARDS = [
    {"model": "SM-I-018", "quantity": 1, "function": "engine-analog", "verification": "UNVERIFIED"},
    {"model": "SM-I-011", "quantity": 2, "function": "digital-inputs", "verification": "UNVERIFIED"},
    {"model": "SM-I-019", "quantity": 1, "function": "thermocouples", "verification": "UNVERIFIED"},
]

FORBIDDEN = [
    "change-dip-or-jumper", "probe-i2c-addresses", "write-i2c-or-modbus-registers",
    "activate-output-or-relay", "write-eeprom-or-firmware",
    "install-or-upgrade-package-or-driver", "change-service-or-boot-configuration",
    "change-network-configuration", "change-wiring-or-power-source",
    "power-from-usb-c-and-hat-simultaneously", "connect-engine-sensor",
    "deploy-runtime",
]


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ContractError("contract root must be an object")
    return value


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise ContractError(f"{control}: {message}")


def expected_controls() -> list[dict]:
    return [{"id": item[0], "key": item[1], "state": item[2]} for item in CONTROL_SPECS]


def validate(contract: dict) -> dict:
    require(set(contract) == ROOT_KEYS, "G1-D20-01", "root properties differ from V1")
    require(
        (contract["schema"], contract["revision"]) ==
        ("avionix-next.g1-sensor-node-discovery.v1", "1.0.0"),
        "G1-D20-01", "contract identity differs from V1",
    )
    require(
        contract["status"] == "PREPARED_SW_NOT_RUN"
        and contract["execution_authorized"] is False
        and contract["flight_approval"] is False,
        "G1-D20-02", "not-run or approval boundary was weakened",
    )

    require(
        contract["source_authority"] == {
            "master_brief": "docs/project/AVIONIX_NEXT_MASTER_PROJECT_BRIEF_CODEX_R0.md",
            "decision_register": "docs/project/AVIONIX_NEXT_DECISION_REGISTER.md",
            "backlog_items": ["AVX-G1-01", "AVX-G1-02"],
        },
        "G1-D20-03", "source authority changed",
    )
    node = contract["node"]
    require(
        node == {
            "role": "ASSO5-SENSOR-NODE", "hostname": "UNRESOLVED",
            "address": "UNRESOLVED", "platform": "RPI4_EXPECTED_UNVERIFIED",
            "operating_system": "UNRESOLVED", "architecture": "UNRESOLVED",
            "power_source": "UNRESOLVED", "storage": "UNRESOLVED",
            "result": "NOT_RUN",
        },
        "G1-D20-04", "unverified node identity was silently resolved",
    )

    allowed = contract["allowed_operations"]
    require(
        allowed == [
            "read-hostname-os-kernel-architecture", "read-memory-storage-and-mounts",
            "read-network-interface-state", "read-throttling-and-temperature-if-supported",
            "list-i2c-controllers-without-address-probing",
            "photograph-board-front-back-labels-and-dip", "hash-and-package-evidence",
        ],
        "G1-D20-05", "read-only operation allowlist changed",
    )
    require("read-memory-storage-and-mounts" in allowed, "G1-D20-06", "power/storage capture missing")

    i2c = contract["i2c_policy"]
    require(
        i2c == {
            "controller_listing_allowed": True, "address_probe_allowed": False,
            "generic_scan_allowed": False, "vendor_read_allowed": False,
            "write_allowed": False,
            "reason": "board revision, bus and approved vendor read command are not yet known",
        },
        "G1-D20-07", "I2C policy must remain enumeration-only",
    )

    inventory = contract["expected_inventory"]
    require(inventory.get("total_boards") == 4, "G1-D20-08", "expected board count is not four")
    require(
        inventory == {
            "source": "USER_CONFIRMED_D-007", "total_boards": 4,
            "boards": EXPECTED_BOARDS,
            "secondary_sm_i_018": "NOT_INSTALLED_UNVERIFIED",
        },
        "G1-D20-09", "model quantities differ from D-007",
    )
    require(
        inventory["secondary_sm_i_018"] == "NOT_INSTALLED_UNVERIFIED",
        "G1-D20-10", "secondary SM-I-018 absence was promoted without evidence",
    )

    board_evidence = contract["board_evidence"]
    require(
        board_evidence == {
            "required_per_board": ["front-photo", "back-photo", "model", "revision", "serial-if-present", "dip-visual-state", "connector-labels"],
            "photo_sha256_required": True, "physical_slot_required": True,
            "observed_address_required": False, "electrical_probe_required": False,
        },
        "G1-D20-11", "physical evidence requirements changed",
    )
    require("dip-visual-state" in board_evidence["required_per_board"], "G1-D20-12", "DIP visual evidence missing")

    power = contract["power_safety"]
    require(
        power == {
            "single_power_source_required": True, "supply_model_and_rating_required": True,
            "wiring_change_allowed": False, "energization_change_allowed": False,
            "brownout_test_allowed": False,
        },
        "G1-D20-13", "power safety boundary changed",
    )
    require(contract["forbidden_operations"] == FORBIDDEN, "G1-D20-14", "forbidden operation set changed")
    require(
        "install-or-upgrade-package-or-driver" in FORBIDDEN
        and "change-service-or-boot-configuration" in FORBIDDEN,
        "G1-D20-15", "software/configuration mutation was enabled",
    )

    adapter = contract["adapter_boundary"]
    require(
        adapter == {
            "real_adapter_status": "NOT_IMPLEMENTED",
            "simulation_adapter_status": "EXISTING_CAPABILITY_NOT_QUALIFIED_FOR_G1",
            "real_and_sim_must_be_distinguishable": True,
            "gui_hardware_access_allowed": False,
            "publisher_transport": "UNRESOLVED",
        },
        "G1-D20-16", "adapter boundary or transport was invented",
    )

    health = contract["health_contract"]
    require(
        health.get("required_states") == ["VALID", "STALE", "LOST", "FAIL", "SIMULATED"],
        "G1-D20-17", "health vocabulary changed",
    )
    require(
        health == {
            "required_states": ["VALID", "STALE", "LOST", "FAIL", "SIMULATED"],
            "required_faults": ["board_absent", "bus_timeout", "open_input", "short_input", "invalid_range", "adapter_restart"],
            "thresholds": "UNRESOLVED_UNTIL_HARDWARE_INVENTORY",
            "recovery_policy": "UNRESOLVED_UNTIL_ADAPTER_DESIGN",
        },
        "G1-D20-18", "fault catalog or unresolved thresholds changed",
    )

    evidence = contract["evidence"]
    require(
        evidence == {
            "required_artifacts": ["preflight", "node-report", "board-inventory", "photo-manifest", "power-report", "i2c-controller-report", "operator-review"],
            "operator_required": True, "utc_required": True, "sha256_required": True,
            "raw_command_output_required": True, "redaction_log_required": True,
            "result": "NOT_RUN",
        },
        "G1-D20-19", "evidence provenance changed",
    )
    go_no_go = contract["go_no_go"]
    require(
        go_no_go == {
            "discovery_only": True, "adapter_implementation_allowed": False,
            "live_publisher_allowed": False, "soak_allowed": False,
            "exit_requires_four_of_four_boards": True,
            "exit_requires_node_identity": True,
            "exit_requires_operator_approval": True,
            "current_decision": "NO_GO",
        },
        "G1-D20-20", "go/no-go boundary changed",
    )

    stages = contract["discovery_stages"]
    require(
        stages == [
            "operator-and-bench-preflight", "node-identity-read-only",
            "power-and-storage-read-only", "i2c-controller-enumeration-only",
            "physical-board-photo-and-label-capture", "dip-and-address-visual-capture",
            "evidence-assembly", "operator-review",
        ],
        "G1-D20-05", "discovery stage order changed",
    )
    require(contract["controls"] == expected_controls(), "G1-D20-01", "control registry changed")

    return {
        "schema": contract["schema"], "status": "PASS",
        "execution_status": "NOT_RUN", "execution_authorized": False,
        "controls_passed": 20, "controls_total": 20,
        "expected_boards": 4, "verified_boards": 0,
        "blocking_dependency": "PHYSICAL_ASSO5_AND_SEQUENT_ACCESS",
        "next_gate": "USER_APPROVAL_FOR_READ_ONLY_DISCOVERY",
        "flight_approval": False,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("contract", nargs="?", type=Path, default=DEFAULT_CONTRACT)
    args = parser.parse_args(argv)
    try:
        report = validate(load_json(args.contract))
    except (OSError, json.JSONDecodeError, ContractError, KeyError, TypeError) as error:
        print(f"G1 discovery contract invalid: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
