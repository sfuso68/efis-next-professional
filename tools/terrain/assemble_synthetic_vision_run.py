#!/usr/bin/env python3
"""Assemble one C10 Synthetic Vision role from real capture artifacts.

The assembler never starts the renderer and never invents measurements.  It
derives the C9 result from six closed-format artifacts already deposited in a
role's C10 artifact directory.  Any missing interval, unsafe path, altered
digest, negative observation or threshold violation fails closed.
"""

from __future__ import annotations

import argparse
import csv
from datetime import datetime
import json
import math
from pathlib import Path
import re
import sys
import tempfile
import zipfile

try:
    from .manage_synthetic_vision_session import (
        CONFIRMATION, ROLES, SessionError, load_session,
    )
    from .validate_synthetic_vision_benchmark import digest, validate_results
except ImportError:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from manage_synthetic_vision_session import (  # type: ignore
        CONFIRMATION, ROLES, SessionError, load_session,
    )
    from validate_synthetic_vision_benchmark import (  # type: ignore
        digest, validate_results,
    )


FILES = {
    "FRAME_TIMING": "frame-timing.csv",
    "THERMAL": "thermal.csv",
    "TILE_TRANSITION": "tile-transition.json",
    "MISSING_TILE_FAULT": "missing-tile-fault.json",
    "CHECKPOINT_SCREENSHOTS": "checkpoint-screenshots.json",
    "OPERATOR_SIGNOFF": "operator-signoff.json",
}
RELEASE = re.compile(r"^[0-9a-f]{40}$")
FRAME_HEADER = ["elapsed_second", "frames", "p95_frame_ms", "maximum_frame_ms"]
THERMAL_HEADER = ["elapsed_second", "temperature_c", "throttled"]
TILE_KEYS = {
    "schema", "boundary", "boundary_discontinuity_m", "missing_samples",
    "finite_projection", "horizon_continuity_pass", "tile_boundary_crossed",
}
FAULT_KEYS = {
    "schema", "unlabelled_fallbacks", "missing_tile_fallback_explicit",
    "recovery_observed",
}
SCREENSHOT_KEYS = {"schema", "screenshots"}
SCREENSHOT_RECORD_KEYS = {"checkpoint", "mode", "file", "sha256"}
SIGNOFF_KEYS = {
    "schema", "operator", "signed_utc", "confirmation",
    "near_field_visual_pass", "horizon_continuity_pass", "notes",
}


class AssemblyError(ValueError):
    """Raised when captured evidence cannot support a C9 result."""


def _load_object(path: Path) -> dict:
    if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
        raise AssemblyError(f"artifact is missing, empty or symlinked: {path.name}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AssemblyError(f"artifact JSON is invalid: {path.name}: {exc}") from exc
    if not isinstance(value, dict):
        raise AssemblyError(f"artifact JSON root must be an object: {path.name}")
    return value


def _is_utc(value: object) -> bool:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        return (parsed.utcoffset() is not None and
                parsed.utcoffset().total_seconds() == 0)
    except (TypeError, ValueError):
        return False


def _number(value: object, label: str) -> float:
    try:
        parsed = float(str(value))
    except (TypeError, ValueError) as exc:
        raise AssemblyError(f"{label} is not numeric") from exc
    if not math.isfinite(parsed):
        raise AssemblyError(f"{label} is not finite")
    return parsed


def _integer(value: object, label: str) -> int:
    parsed = _number(value, label)
    if not parsed.is_integer():
        raise AssemblyError(f"{label} is not an integer")
    return int(parsed)


def _read_csv(path: Path, header: list[str]) -> list[dict[str, str]]:
    if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
        raise AssemblyError(f"artifact is missing, empty or symlinked: {path.name}")
    with path.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != header:
            raise AssemblyError(f"{path.name} header differs from the closed schema")
        rows = list(reader)
    if not rows:
        raise AssemblyError(f"{path.name} contains no samples")
    return rows


def _continuous_seconds(rows: list[dict[str, str]], label: str) -> list[int]:
    seconds = [_integer(row.get("elapsed_second"), f"{label} elapsed_second")
               for row in rows]
    if seconds[0] != 0 or any(right != left + 1 for left, right in zip(seconds, seconds[1:])):
        raise AssemblyError(f"{label} timeline must be continuous from second zero")
    return seconds


def _frame_metrics(path: Path, required_duration: int) -> dict:
    rows = _read_csv(path, FRAME_HEADER)
    seconds = _continuous_seconds(rows, "frame timing")
    if seconds[-1] < required_duration:
        raise AssemblyError("frame timing duration is below the contract")
    frames = [_integer(row["frames"], "frames") for row in rows]
    p95 = [_number(row["p95_frame_ms"], "p95_frame_ms") for row in rows]
    maximum = [_number(row["maximum_frame_ms"], "maximum_frame_ms") for row in rows]
    if any(value < 0 for value in frames + p95 + maximum):
        raise AssemblyError("frame timing values must be non-negative")
    if any(p > m for p, m in zip(p95, maximum)):
        raise AssemblyError("frame p95 cannot exceed the corresponding maximum")
    return {
        "duration_seconds": seconds[-1],
        "minimum_fps": min(frames),
        "p95_frame_ms": max(p95),
        "maximum_frame_ms": max(maximum),
    }


def _thermal_metrics(path: Path, required_duration: int) -> dict:
    rows = _read_csv(path, THERMAL_HEADER)
    seconds = _continuous_seconds(rows, "thermal")
    if seconds[-1] < required_duration:
        raise AssemblyError("thermal duration is below the contract")
    temperatures = [_number(row["temperature_c"], "temperature_c") for row in rows]
    throttled = [row["throttled"].strip().lower() for row in rows]
    if any(value < -100 or value > 200 for value in temperatures):
        raise AssemblyError("temperature sample is outside the capture sanity range")
    if any(not re.fullmatch(r"0x[0-9a-f]+", value) for value in throttled):
        raise AssemblyError("throttled sample is malformed")
    if any(value != "0x0" for value in throttled):
        raise AssemblyError("thermal log records throttling or undervoltage")
    return {"maximum_temperature_c": max(temperatures), "throttled": "0x0"}


def _safe_child(root: Path, relative: object) -> Path:
    if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
        raise AssemblyError("screenshot path must be a non-empty relative path")
    candidate = root / relative
    try:
        candidate.resolve().relative_to(root.resolve())
    except ValueError as exc:
        raise AssemblyError(f"screenshot escapes the artifact root: {relative}") from exc
    if candidate.is_symlink() or not candidate.is_file() or candidate.stat().st_size == 0:
        raise AssemblyError(f"screenshot is missing, empty or symlinked: {relative}")
    return candidate


def _screenshot_checkpoints(path: Path, artifact_root: Path,
                            checkpoint_ids: list[str]) -> tuple[list[dict], list[Path]]:
    data = _load_object(path)
    if set(data) != SCREENSHOT_KEYS or data.get("schema") != "avionix.synthetic-vision-screenshots.v1":
        raise AssemblyError("screenshot manifest differs from the closed schema")
    records = data.get("screenshots")
    expected = [(checkpoint, mode) for checkpoint in checkpoint_ids
                for mode in ("DAY", "NIGHT")]
    if (not isinstance(records, list) or len(records) != len(expected) or
            any(not isinstance(item, dict) or set(item) != SCREENSHOT_RECORD_KEYS
                for item in records)):
        raise AssemblyError("screenshot records are incomplete or open-ended")
    observed = []
    screenshot_paths = []
    for record, pair in zip(records, expected):
        if (record.get("checkpoint"), record.get("mode")) != pair:
            raise AssemblyError("screenshots must cover ordered DAY/NIGHT checkpoints")
        target = _safe_child(artifact_root, record.get("file"))
        if record.get("sha256") != digest(target):
            raise AssemblyError(f"screenshot digest mismatch: {record.get('file')}")
        observed.append(pair)
        screenshot_paths.append(target)
    checkpoints = [
        {"id": checkpoint, "day_observed": (checkpoint, "DAY") in observed,
         "night_observed": (checkpoint, "NIGHT") in observed}
        for checkpoint in checkpoint_ids
    ]
    return checkpoints, screenshot_paths


def _bundle_screenshots(manifest: Path, screenshots: list[Path],
                        artifact_root: Path) -> Path:
    output = artifact_root / "checkpoint-screenshots.zip"
    if output.exists():
        raise AssemblyError("refusing to overwrite checkpoint-screenshots.zip")
    with tempfile.NamedTemporaryFile(dir=artifact_root, delete=False) as handle:
        temporary = Path(handle.name)
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.write(manifest, manifest.name)
            for path in screenshots:
                relative = path.relative_to(artifact_root)
                archive.write(path, str(relative).replace("\\", "/"))
        temporary.replace(output)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise
    return output


def _closed_json(path: Path, keys: set[str], schema: str) -> dict:
    data = _load_object(path)
    if set(data) != keys or data.get("schema") != schema:
        raise AssemblyError(f"{path.name} differs from the closed schema")
    return data


def assemble_role(root: Path, bundle: Path, role: str, release_id: str,
                  operator: str, confirmation: str) -> Path:
    role = role.upper()
    if role not in ROLES:
        raise AssemblyError("role must be PFD or MFD")
    if not RELEASE.fullmatch(release_id):
        raise AssemblyError("release identity must be a full lowercase Git object ID")
    operator = operator.strip()
    if not operator or confirmation != CONFIRMATION:
        raise AssemblyError("named operator and exact real-observation confirmation are required")

    session, contract, runs = load_session(bundle, root)
    results, artifact_root = next(item for item in runs if item[0]["role"] == role)
    results_path = bundle / role.lower() / "results.json"
    if results.get("status") != "NOT_RUN":
        raise AssemblyError(f"refusing to overwrite an executed {role} result")
    paths = {kind: artifact_root / name for kind, name in FILES.items()}

    frame = _frame_metrics(paths["FRAME_TIMING"], contract["run_requirements"]["duration_seconds"])
    thermal = _thermal_metrics(paths["THERMAL"], contract["run_requirements"]["duration_seconds"])
    tile = _closed_json(paths["TILE_TRANSITION"], TILE_KEYS,
                        "avionix.synthetic-vision-tile-transition.v1")
    fault = _closed_json(paths["MISSING_TILE_FAULT"], FAULT_KEYS,
                         "avionix.synthetic-vision-missing-tile-fault.v1")
    signoff = _closed_json(paths["OPERATOR_SIGNOFF"], SIGNOFF_KEYS,
                           "avionix.synthetic-vision-operator-signoff.v1")
    if (tile.get("boundary") != contract["run_requirements"]["tile_boundary_crossing"] or
            signoff.get("operator") != operator or
            signoff.get("confirmation") != CONFIRMATION or
            not _is_utc(signoff.get("signed_utc")) or
            not isinstance(signoff.get("notes"), str)):
        raise AssemblyError("tile boundary or operator signoff identity is invalid")

    checkpoint_ids = [item["id"] for item in contract["checkpoints"]]
    checkpoints, screenshot_paths = _screenshot_checkpoints(
        paths["CHECKPOINT_SCREENSHOTS"], artifact_root, checkpoint_ids)
    screenshot_bundle = _bundle_screenshots(
        paths["CHECKPOINT_SCREENSHOTS"], screenshot_paths, artifact_root)
    evidence_paths = dict(paths)
    evidence_paths["CHECKPOINT_SCREENSHOTS"] = screenshot_bundle
    artifacts = {
        kind: {"path": path.name, "sha256": digest(path)}
        for kind, path in evidence_paths.items()
    }
    measurements = {
        "minimum_fps": frame["minimum_fps"],
        "p95_frame_ms": frame["p95_frame_ms"],
        "maximum_frame_ms": frame["maximum_frame_ms"],
        "maximum_temperature_c": thermal["maximum_temperature_c"],
        "throttled": thermal["throttled"],
        "boundary_discontinuity_m": tile.get("boundary_discontinuity_m"),
        "missing_samples": tile.get("missing_samples"),
        "unlabelled_fallbacks": fault.get("unlabelled_fallbacks"),
        "finite_projection": tile.get("finite_projection"),
        "near_field_visual_pass": signoff.get("near_field_visual_pass"),
        "horizon_continuity_pass": (
            tile.get("horizon_continuity_pass") is True and
            signoff.get("horizon_continuity_pass") is True),
        "tile_boundary_crossed": tile.get("tile_boundary_crossed"),
        "missing_tile_fallback_explicit": (
            fault.get("missing_tile_fallback_explicit") is True and
            fault.get("recovery_observed") is True),
    }
    assembled = {
        "schema": "avionix.synthetic-vision-benchmark-results.v1",
        "benchmark_id": contract["benchmark_id"],
        "contract_sha256": session["contract_sha256"],
        "source_commit": session["source_commit"],
        "platform": contract["platform"],
        "role": role,
        "release_id": release_id,
        "status": "PASS",
        "flight_approval": False,
        "duration_seconds": frame["duration_seconds"],
        "modes": ["DAY", "NIGHT"],
        "measurements": measurements,
        "checkpoints": checkpoints,
        "artifacts": artifacts,
        "operator": operator,
    }
    errors = validate_results(
        assembled, contract, artifact_root, session["contract_sha256"])
    if errors:
        screenshot_bundle.unlink(missing_ok=True)
        raise AssemblyError("captured run fails C9:\n  " + "\n  ".join(errors))

    with tempfile.NamedTemporaryFile(
            "w", encoding="utf-8", dir=results_path.parent, delete=False) as handle:
        temporary = Path(handle.name)
        json.dump(assembled, handle, indent=2, sort_keys=True)
        handle.write("\n")
    temporary.replace(results_path)
    return results_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--bundle", required=True, type=Path)
    parser.add_argument("--role", required=True, choices=ROLES)
    parser.add_argument("--release-id", required=True)
    parser.add_argument("--operator", required=True)
    parser.add_argument("--confirmation", required=True)
    args = parser.parse_args()
    try:
        path = assemble_role(
            args.root, args.bundle, args.role, args.release_id,
            args.operator, args.confirmation)
        print(f"SYNTHETIC VISION {args.role} run assembled PASS: {path}")
        return 0
    except (OSError, SessionError, AssemblyError) as exc:
        print(f"SYNTHETIC VISION run assembly FAILED: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
