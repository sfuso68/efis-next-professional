#!/usr/bin/env python3
"""Validate the fail-closed G1 physical inventory manifest."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path, PurePosixPath
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = ROOT / "config/hardware/avionix-g1-physical-inventory-v1.json"
PHOTO_ROOT = PurePosixPath("docs/status/evidence/g1-physical-inventory")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


class InventoryError(ValueError):
    """The physical inventory violates its frozen evidence boundary."""


CONTROL_SPECS = [
    ("G1-I10-01", "manifest_identity", "PASS"),
    ("G1-I10-02", "node_provenance", "PASS"),
    ("G1-I10-03", "four_expected_slots", "PASS"),
    ("G1-I10-04", "front_back_photo_contract", "PREPARED"),
    ("G1-I10-05", "label_identity_contract", "PREPARED"),
    ("G1-I10-06", "dip_visual_only", "PREPARED"),
    ("G1-I10-07", "connector_and_slot_contract", "PREPARED"),
    ("G1-I10-08", "power_evidence_contract", "PREPARED"),
    ("G1-I10-09", "no_probe_or_mutation", "PASS"),
    ("G1-I10-10", "fail_closed_decision", "NO_GO"),
]

EXPECTED_SLOTS = [
    ("G1-SLOT-01", "SM-I-018", "engine-analog"),
    ("G1-SLOT-02", "SM-I-011", "digital-inputs"),
    ("G1-SLOT-03", "SM-I-011", "digital-inputs"),
    ("G1-SLOT-04", "SM-I-019", "thermocouples"),
]

BOARD_KEYS = {
    "slot", "expected_model", "expected_function", "model_observed",
    "revision_observed", "serial_observed", "front_photo", "back_photo",
    "dip_visual_state", "connector_labels", "i2c_address_observed",
    "electrical_probe_performed", "verification",
}


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise InventoryError(f"{control}: {message}")


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise InventoryError("manifest root must be an object")
    return value


def expected_controls() -> list[dict]:
    return [{"id": item[0], "key": item[1], "state": item[2]} for item in CONTROL_SPECS]


def _safe_relative_photo(value: object) -> bool:
    if not isinstance(value, dict) or set(value) != {"path", "sha256"}:
        return False
    path = value["path"]
    digest = value["sha256"]
    if not isinstance(path, str) or not isinstance(digest, str):
        return False
    pure = PurePosixPath(path)
    return (
        not pure.is_absolute()
        and ".." not in pure.parts
        and pure.parts[:4] == PHOTO_ROOT.parts
        and bool(SHA256_RE.fullmatch(digest))
    )


def _board_complete(board: dict) -> bool:
    return (
        board["model_observed"] == board["expected_model"]
        and isinstance(board["revision_observed"], str)
        and bool(board["revision_observed"].strip())
        and (board["serial_observed"] is None or isinstance(board["serial_observed"], str))
        and _safe_relative_photo(board["front_photo"])
        and _safe_relative_photo(board["back_photo"])
        and isinstance(board["dip_visual_state"], str)
        and bool(board["dip_visual_state"].strip())
        and isinstance(board["connector_labels"], list)
        and bool(board["connector_labels"])
        and all(isinstance(item, str) and item.strip() for item in board["connector_labels"])
        and board["i2c_address_observed"] is None
        and board["electrical_probe_performed"] is False
        and board["verification"] == "VERIFIED_VISUAL"
    )


def _power_complete(power: dict) -> bool:
    return (
        all(isinstance(power[key], str) and power[key].strip()
            for key in ("supply_model", "rated_voltage", "rated_current"))
        and _safe_relative_photo(power["label_photo"])
        and power["single_source_confirmed"] is True
        and power["verification"] == "VERIFIED_VISUAL"
    )


def validate(manifest: dict) -> dict:
    root_keys = {
        "schema", "revision", "status", "source_contract", "node", "boards",
        "power", "capture_policy", "operator_review",
        "prohibited_operations_observed", "decision", "flight_approval", "controls",
    }
    require(set(manifest) == root_keys, "G1-I10-01", "root properties differ from V1")
    require(
        (manifest["schema"], manifest["revision"], manifest["source_contract"]) == (
            "avionix-next.g1-physical-inventory.v1", "1.0.0",
            "config/hardware/avionix-g1-sensor-node-discovery-v1.json",
        ),
        "G1-I10-01", "manifest identity differs from V1",
    )
    require(manifest["controls"] == expected_controls(), "G1-I10-01", "control registry changed")
    require(
        manifest["node"] == {
            "hostname": "ems-sensor", "address": "192.168.1.89",
            "platform": "Raspberry Pi 4 Model B Rev 1.5",
            "capture_report_sha256": "f7cb1da79623c37891e662ec966f8d64be743a8d471663970993e4eac93f5e94",
        },
        "G1-I10-02", "node provenance differs from verified capture",
    )

    boards = manifest["boards"]
    require(isinstance(boards, list) and len(boards) == 4, "G1-I10-03", "exactly four slots required")
    for board, expected in zip(boards, EXPECTED_SLOTS):
        require(isinstance(board, dict) and set(board) == BOARD_KEYS, "G1-I10-03", "board properties differ from V1")
        require(
            (board["slot"], board["expected_model"], board["expected_function"]) == expected,
            "G1-I10-03", "slot/model/function assignment changed",
        )
    require(
        Counter(board["expected_model"] for board in boards) ==
        Counter({"SM-I-018": 1, "SM-I-011": 2, "SM-I-019": 1}),
        "G1-I10-03", "expected model quantities changed",
    )

    for board in boards:
        for key in ("front_photo", "back_photo"):
            require(board[key] is None or _safe_relative_photo(board[key]), "G1-I10-04", "photo evidence is unsafe or malformed")
        require(
            board["model_observed"] is None or board["model_observed"] == board["expected_model"],
            "G1-I10-05", "observed model contradicts expected inventory",
        )
        require(
            board["revision_observed"] is None or
            (isinstance(board["revision_observed"], str) and board["revision_observed"].strip()),
            "G1-I10-05", "revision evidence malformed",
        )
        require(
            board["dip_visual_state"] is None or
            (isinstance(board["dip_visual_state"], str) and board["dip_visual_state"].strip()),
            "G1-I10-06", "DIP evidence malformed",
        )
        require(isinstance(board["connector_labels"], list), "G1-I10-07", "connector labels must be a list")
        require(board["i2c_address_observed"] is None, "G1-I10-09", "I2C address observation is not authorized")
        require(board["electrical_probe_performed"] is False, "G1-I10-09", "electrical probing is not authorized")

    power = manifest["power"]
    require(
        isinstance(power, dict) and set(power) == {
            "supply_model", "rated_voltage", "rated_current", "label_photo",
            "single_source_confirmed", "verification",
        },
        "G1-I10-08", "power evidence properties differ from V1",
    )
    require(power["label_photo"] is None or _safe_relative_photo(power["label_photo"]), "G1-I10-08", "power label photo invalid")

    policy = manifest["capture_policy"]
    require(
        policy == {
            "photo_root": str(PHOTO_ROOT), "photo_hash": "SHA-256",
            "relative_paths_only": True, "dip_change_allowed": False,
            "wiring_change_allowed": False, "address_probe_allowed": False,
            "vendor_read_allowed": False, "output_activation_allowed": False,
        },
        "G1-I10-09", "read-only capture policy was weakened",
    )
    require(manifest["prohibited_operations_observed"] == [], "G1-I10-09", "prohibited operation recorded")
    require(manifest["flight_approval"] is False, "G1-I10-10", "inventory cannot approve flight")

    complete_boards = sum(_board_complete(board) for board in boards)
    power_complete = _power_complete(power)
    review = manifest["operator_review"]
    require(
        isinstance(review, dict) and set(review) == {"operator", "captured_utc", "approved", "notes"},
        "G1-I10-10", "operator review properties differ from V1",
    )
    complete = complete_boards == 4 and power_complete and review["approved"] is True

    if complete:
        require(manifest["status"] == "CAPTURED_VISUAL_PENDING_ENGINEERING", "G1-I10-10", "complete evidence has wrong status")
        require(manifest["decision"] == "NO_GO_PENDING_ENGINEERING_REVIEW", "G1-I10-10", "engineering review cannot be bypassed")
        require(isinstance(review["operator"], str) and review["operator"].strip(), "G1-I10-10", "operator missing")
        require(isinstance(review["captured_utc"], str) and review["captured_utc"].strip(), "G1-I10-10", "capture UTC missing")
    else:
        require(manifest["status"] == "PREPARED_SW_NOT_CAPTURED", "G1-I10-10", "draft status was promoted without complete evidence")
        require(manifest["decision"] == "NO_GO_PENDING_PHYSICAL_BOARD_INVENTORY", "G1-I10-10", "draft decision must remain NO-GO")
        require(review["approved"] is False, "G1-I10-10", "incomplete inventory cannot be approved")

    return {
        "schema": manifest["schema"],
        "status": "PASS",
        "inventory_status": manifest["status"],
        "controls_passed": 10,
        "controls_total": 10,
        "boards_expected": 4,
        "boards_verified_visual": complete_boards,
        "power_verified_visual": power_complete,
        "operator_approved": review["approved"],
        "decision": manifest["decision"],
        "flight_approval": False,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", nargs="?", type=Path, default=DEFAULT_MANIFEST)
    args = parser.parse_args(argv)
    try:
        report = validate(load_json(args.manifest))
    except (OSError, json.JSONDecodeError, InventoryError, KeyError, TypeError) as error:
        print(f"G1 physical inventory invalid: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
