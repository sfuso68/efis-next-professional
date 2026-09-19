#!/usr/bin/env python3
"""Validate and import visual-only G1 board inventory evidence."""

from __future__ import annotations

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INTAKE = ROOT / "config/hardware/avionix-g1-visual-intake-v1.json"
DEFAULT_MANIFEST = ROOT / "config/hardware/avionix-g1-physical-inventory-v1.json"
DEFAULT_EVIDENCE_ROOT = ROOT / "docs/status/evidence/g1-physical-inventory"
REPOSITORY_EVIDENCE_ROOT = PurePosixPath("docs/status/evidence/g1-physical-inventory")
EXPECTED_SLOTS = {
    "G1-SLOT-01": "SM-I-018",
    "G1-SLOT-02": "SM-I-011",
    "G1-SLOT-03": "SM-I-011",
    "G1-SLOT-04": "SM-I-019",
}
BOARD_KEYS = {
    "slot", "model_observed", "revision_observed", "serial_observed",
    "dip_visual_state", "connector_labels", "front_file", "back_file",
}
DECLARATION = {
    "visual_only": True,
    "dip_changed": False,
    "wiring_changed": False,
    "electrical_probe_performed": False,
    "vendor_read_performed": False,
    "output_activated": False,
}
ALLOWED_SUFFIXES = {".jpg", ".jpeg", ".png"}


class IntakeError(ValueError):
    """Visual evidence is incomplete, unsafe, or inconsistent."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise IntakeError(message)


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(value, dict), f"{path}: JSON root must be an object")
    return value


def _required_text(value: object, field: str) -> str:
    require(isinstance(value, str) and bool(value.strip()), f"{field} is required")
    return value.strip()


def _optional_text(value: object, field: str) -> str | None:
    require(value is None or (isinstance(value, str) and bool(value.strip())), f"{field} is malformed")
    return value.strip() if isinstance(value, str) else None


def _safe_file(source_root: Path, value: object, field: str) -> Path:
    name = _required_text(value, field)
    pure = PurePosixPath(name.replace("\\", "/"))
    require(not pure.is_absolute() and ".." not in pure.parts, f"{field} must be a confined relative filename")
    require(len(pure.parts) == 1, f"{field} must not contain directories")
    require(pure.suffix.lower() in ALLOWED_SUFFIXES, f"{field} must be JPEG or PNG")
    path = source_root / pure.name
    require(path.is_file(), f"missing image: {pure.name}")
    header = path.read_bytes()[:8]
    require(
        header.startswith(b"\xff\xd8\xff") or header == b"\x89PNG\r\n\x1a\n",
        f"{field} content is not JPEG or PNG",
    )
    return path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def prepare(intake: dict, manifest: dict, source_root: Path) -> tuple[dict, list[tuple[Path, str, str]]]:
    require(
        manifest.get("status") == "PREPARED_SW_NOT_CAPTURED"
        and manifest.get("decision") == "NO_GO_PENDING_PHYSICAL_BOARD_INVENTORY",
        "source manifest must be the unpopulated NO-GO template",
    )
    require(
        set(intake) == {"schema", "operator", "captured_utc", "boards", "power", "capture_declaration", "notes"},
        "intake properties differ from V1",
    )
    require(intake["schema"] == "avionix-next.g1-visual-intake.v1", "unsupported intake schema")
    operator = _required_text(intake["operator"], "operator")
    captured_utc = _required_text(intake["captured_utc"], "captured_utc")
    try:
        parsed_utc = datetime.fromisoformat(captured_utc.replace("Z", "+00:00"))
    except ValueError as error:
        raise IntakeError("captured_utc must be valid ISO-8601 UTC ending in Z") from error
    require(
        captured_utc.endswith("Z") and parsed_utc.tzinfo == timezone.utc,
        "captured_utc must be valid ISO-8601 UTC ending in Z",
    )
    require(intake["capture_declaration"] == DECLARATION, "visual-only safety declaration is incomplete or false")
    require(intake["notes"] is None or isinstance(intake["notes"], str), "notes must be text or null")

    boards = intake["boards"]
    require(isinstance(boards, list) and len(boards) == 4, "exactly four board records are required")
    require([board.get("slot") for board in boards] == list(EXPECTED_SLOTS), "board slots/order differ from V1")

    result = copy.deepcopy(manifest)
    artifacts: list[tuple[Path, str, str]] = []
    filenames: set[str] = set()
    for observed, target in zip(boards, result["boards"]):
        require(isinstance(observed, dict) and set(observed) == BOARD_KEYS, "board properties differ from V1")
        slot = observed["slot"]
        require(observed["model_observed"] == EXPECTED_SLOTS[slot], f"{slot}: observed model mismatch")
        revision = _required_text(observed["revision_observed"], f"{slot}.revision_observed")
        serial = _optional_text(observed["serial_observed"], f"{slot}.serial_observed")
        dip = _required_text(observed["dip_visual_state"], f"{slot}.dip_visual_state")
        labels = observed["connector_labels"]
        require(isinstance(labels, list) and bool(labels), f"{slot}.connector_labels must not be empty")
        labels = [_required_text(label, f"{slot}.connector_labels") for label in labels]

        photos: dict[str, dict[str, str]] = {}
        for side in ("front", "back"):
            source = _safe_file(source_root, observed[f"{side}_file"], f"{slot}.{side}_file")
            require(source.name not in filenames, f"duplicate evidence filename: {source.name}")
            filenames.add(source.name)
            digest = sha256_file(source)
            relative = str(REPOSITORY_EVIDENCE_ROOT / source.name)
            photos[side] = {"path": relative, "sha256": digest}
            artifacts.append((source, source.name, digest))

        target.update(
            model_observed=observed["model_observed"],
            revision_observed=revision,
            serial_observed=serial,
            front_photo=photos["front"],
            back_photo=photos["back"],
            dip_visual_state=dip,
            connector_labels=labels,
            i2c_address_observed=None,
            electrical_probe_performed=False,
            verification="VERIFIED_VISUAL",
        )

    power = intake["power"]
    require(
        isinstance(power, dict) and set(power) == {
            "supply_model", "rated_voltage", "rated_current", "label_file", "single_source_confirmed"
        },
        "power properties differ from V1",
    )
    require(power["single_source_confirmed"] is True, "single power source must be visually confirmed")
    power_source = _safe_file(source_root, power["label_file"], "power.label_file")
    require(power_source.name not in filenames, f"duplicate evidence filename: {power_source.name}")
    power_digest = sha256_file(power_source)
    artifacts.append((power_source, power_source.name, power_digest))
    result["power"].update(
        supply_model=_required_text(power["supply_model"], "power.supply_model"),
        rated_voltage=_required_text(power["rated_voltage"], "power.rated_voltage"),
        rated_current=_required_text(power["rated_current"], "power.rated_current"),
        label_photo={"path": str(REPOSITORY_EVIDENCE_ROOT / power_source.name), "sha256": power_digest},
        single_source_confirmed=True,
        verification="VERIFIED_VISUAL",
    )
    result["operator_review"].update(
        operator=operator,
        captured_utc=captured_utc,
        approved=True,
        notes=intake["notes"],
    )
    result["status"] = "CAPTURED_VISUAL_PENDING_ENGINEERING"
    result["decision"] = "NO_GO_PENDING_ENGINEERING_REVIEW"
    result["flight_approval"] = False
    return result, artifacts


def apply_import(result: dict, artifacts: list[tuple[Path, str, str]], evidence_root: Path, manifest_path: Path) -> None:
    evidence_root.mkdir(parents=True, exist_ok=True)
    for _source, name, digest in artifacts:
        destination = evidence_root / name
        if destination.exists():
            require(destination.is_file() and sha256_file(destination) == digest, f"refusing to overwrite different artifact: {name}")
    for source, name, digest in artifacts:
        destination = evidence_root / name
        if destination.exists():
            continue
        else:
            shutil.copy2(source, destination)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=manifest_path.parent, delete=False) as stream:
        json.dump(result, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
        temporary = Path(stream.name)
    temporary.replace(manifest_path)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Directory containing the nine visual evidence images")
    parser.add_argument("--intake", type=Path, default=DEFAULT_INTAKE)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--evidence-root", type=Path, default=DEFAULT_EVIDENCE_ROOT)
    parser.add_argument("--apply", action="store_true", help="Copy evidence and update the physical inventory manifest")
    args = parser.parse_args(argv)
    try:
        result, artifacts = prepare(load_json(args.intake), load_json(args.manifest), args.source)
        if args.apply:
            apply_import(result, artifacts, args.evidence_root, args.manifest)
        report = {
            "schema": "avionix-next.g1-visual-import-report.v1",
            "status": "APPLIED" if args.apply else "PREVIEW_VALID",
            "artifacts": len(artifacts),
            "boards": len(result["boards"]),
            "manifest_status": result["status"],
            "decision": result["decision"],
            "flight_approval": result["flight_approval"],
        }
    except (OSError, json.JSONDecodeError, IntakeError, KeyError, TypeError) as error:
        print(f"G1 visual inventory import rejected: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
