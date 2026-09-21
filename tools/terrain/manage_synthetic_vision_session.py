#!/usr/bin/env python3
"""Prepare and finalize fail-closed Synthetic Vision hardware sessions.

The utility does not start the renderer, alter a display, clear a cache, or
inject a terrain fault.  ``init`` creates an immutable session skeleton for
PFD and MFD.  ``finalize`` accepts only two independently valid C9 result
records and emits a digest-bound session manifest after explicit operator
confirmation.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile

try:
    from .validate_synthetic_vision_benchmark import (
        ARTIFACT_TYPES, CONTRACT, digest, load_json, validate_contract,
        validate_results,
    )
except ImportError:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from validate_synthetic_vision_benchmark import (  # type: ignore
        ARTIFACT_TYPES, CONTRACT, digest, load_json, validate_contract,
        validate_results,
    )


ROLES = ("PFD", "MFD")
CONFIRMATION = "I_CONFIRM_REAL_SYNTHETIC_VISION_OBSERVATION"
COMMIT = re.compile(r"^[0-9a-f]{40}$")
SESSION_KEYS = {
    "schema", "status", "flight_approval", "created_utc", "source_commit",
    "contract", "contract_sha256", "roles",
}
ROLE_KEYS = {"role", "results_file", "artifact_root", "required_artifact_kinds"}
EVIDENCE_KEYS = {
    "schema", "status", "flight_approval", "captured_utc", "source_commit",
    "contract_sha256", "operator", "roles",
}
EVIDENCE_ROLE_KEYS = {"role", "release_id", "results_file", "results_sha256"}


class SessionError(ValueError):
    """Raised when a session would weaken the C9 evidence boundary."""


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _is_utc(value: object) -> bool:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        return (parsed.utcoffset() is not None and
                parsed.utcoffset().total_seconds() == 0)
    except (TypeError, ValueError):
        return False


def _write_new_json(path: Path, value: dict) -> None:
    if path.exists():
        raise SessionError(f"refusing to overwrite {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
            "w", encoding="utf-8", dir=path.parent, delete=False) as handle:
        temporary = Path(handle.name)
        json.dump(value, handle, indent=2, sort_keys=True)
        handle.write("\n")
    temporary.replace(path)


def _load_object(path: Path) -> dict:
    if path.is_symlink():
        raise SessionError(f"symlinked JSON is forbidden: {path}")
    try:
        value = load_json(path)
    except (OSError, json.JSONDecodeError) as exc:
        raise SessionError(f"invalid or missing JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise SessionError(f"JSON root must be an object: {path}")
    return value


def _safe_relative(base: Path, value: object) -> Path:
    if not isinstance(value, str) or not value:
        raise SessionError("session contains an empty path")
    relative = Path(value)
    if relative.is_absolute():
        raise SessionError(f"absolute path is forbidden: {value}")
    candidate = base / relative
    resolved = candidate.resolve()
    try:
        resolved.relative_to(base.resolve())
    except ValueError as exc:
        raise SessionError(f"path escapes the session: {value}") from exc
    return candidate


def _result_template(
        contract: dict, role: str, source_commit: str,
        contract_sha256: str) -> dict:
    return {
        "schema": "avionix.synthetic-vision-benchmark-results.v1",
        "benchmark_id": contract["benchmark_id"],
        "contract_sha256": contract_sha256,
        "source_commit": source_commit,
        "platform": contract["platform"],
        "role": role,
        "release_id": None,
        "status": "NOT_RUN",
        "flight_approval": False,
        "duration_seconds": 0,
        "modes": [],
        "measurements": {},
        "checkpoints": [
            {"id": item["id"], "day_observed": False, "night_observed": False}
            for item in contract["checkpoints"]
        ],
        "artifacts": {},
        "operator": None,
    }


def init_session(root: Path, output: Path, source_commit: str) -> Path:
    if not COMMIT.fullmatch(source_commit):
        raise SessionError("source commit must be a full lowercase Git object ID")
    if output.exists():
        raise SessionError(f"session directory already exists: {output}")
    canonical = root / "config/terrain/avionix-synthetic-vision-benchmark-v1.json"
    contract = _load_object(canonical)
    errors = validate_contract(contract, root)
    if errors:
        raise SessionError("canonical C9 contract is invalid:\n  " + "\n  ".join(errors))

    output.mkdir(parents=True)
    contract_target = output / "benchmark-contract.json"
    shutil.copyfile(canonical, contract_target)
    contract_sha256 = digest(contract_target)
    roles = []
    for role in ROLES:
        role_dir = output / role.lower()
        (role_dir / "artifacts").mkdir(parents=True)
        _write_new_json(
            role_dir / "results.json",
            _result_template(contract, role, source_commit, contract_sha256))
        roles.append({
            "role": role,
            "results_file": f"{role.lower()}/results.json",
            "artifact_root": f"{role.lower()}/artifacts",
            "required_artifact_kinds": sorted(ARTIFACT_TYPES),
        })
    session_path = output / "session.json"
    _write_new_json(session_path, {
        "schema": "avionix.synthetic-vision-session.v1",
        "status": "PREPARED_SW_NOT_RUN",
        "flight_approval": False,
        "created_utc": _utc_now(),
        "source_commit": source_commit,
        "contract": "benchmark-contract.json",
        "contract_sha256": contract_sha256,
        "roles": roles,
    })
    return session_path


def load_session(bundle: Path, root: Path) -> tuple[dict, dict, list[tuple[dict, Path]]]:
    session = _load_object(bundle / "session.json")
    if set(session) != SESSION_KEYS:
        raise SessionError("session properties differ from the closed schema")
    if (session.get("schema") != "avionix.synthetic-vision-session.v1" or
            session.get("status") != "PREPARED_SW_NOT_RUN" or
            session.get("flight_approval") is not False or
            not COMMIT.fullmatch(str(session.get("source_commit"))) or
            not _is_utc(session.get("created_utc"))):
        raise SessionError("session authority, state, timestamp, or source commit is invalid")

    contract_path = _safe_relative(bundle, session.get("contract"))
    if contract_path.is_symlink() or not contract_path.is_file():
        raise SessionError("session contract is missing or symlinked")
    if digest(contract_path) != session.get("contract_sha256"):
        raise SessionError("session contract digest changed")
    canonical = root / "config/terrain/avionix-synthetic-vision-benchmark-v1.json"
    if not canonical.is_file() or digest(canonical) != session.get("contract_sha256"):
        raise SessionError("session contract does not match the canonical C9 contract")
    contract = _load_object(contract_path)
    errors = validate_contract(contract, root)
    if errors:
        raise SessionError("session contract is invalid:\n  " + "\n  ".join(errors))

    role_entries = session.get("roles")
    if (not isinstance(role_entries, list) or
            [item.get("role") for item in role_entries if isinstance(item, dict)] != list(ROLES) or
            any(not isinstance(item, dict) or set(item) != ROLE_KEYS for item in role_entries)):
        raise SessionError("session role list is invalid")
    runs: list[tuple[dict, Path]] = []
    for entry in role_entries:
        role = entry["role"]
        if entry["required_artifact_kinds"] != sorted(ARTIFACT_TYPES):
            raise SessionError(f"{role} artifact contract changed")
        results_path = _safe_relative(bundle, entry["results_file"])
        artifact_root = _safe_relative(bundle, entry["artifact_root"])
        if results_path.is_symlink() or artifact_root.is_symlink() or not artifact_root.is_dir():
            raise SessionError(f"{role} results or artifact root is invalid")
        results = _load_object(results_path)
        if results.get("role") != role:
            raise SessionError(f"{role} result identity changed")
        if results.get("source_commit") != session["source_commit"]:
            raise SessionError(f"{role} source commit differs from the session")
        runs.append((results, artifact_root))
    return session, contract, runs


def finalize_session(
        root: Path, bundle: Path, operator: str, confirmation: str,
        evidence_name: str = "evidence.json") -> Path:
    if confirmation != CONFIRMATION:
        raise SessionError("explicit real-hardware operator confirmation is required")
    operator = operator.strip()
    if not operator:
        raise SessionError("operator name is required")
    session, contract, runs = load_session(bundle, root)
    role_records = []
    for results, artifact_root in runs:
        errors = validate_results(
            results, contract, artifact_root, session["contract_sha256"])
        if errors:
            raise SessionError(
                f"{results.get('role')} C9 result is incomplete:\n  " + "\n  ".join(errors))
        if results.get("operator") != operator:
            raise SessionError(f"{results.get('role')} operator differs from final signoff")
        results_path = bundle / results["role"].lower() / "results.json"
        role_records.append({
            "role": results["role"],
            "release_id": results["release_id"],
            "results_file": str(results_path.relative_to(bundle)).replace("\\", "/"),
            "results_sha256": digest(results_path),
        })
    evidence = {
        "schema": "avionix.synthetic-vision-session-evidence.v1",
        "status": "PASS",
        "flight_approval": False,
        "captured_utc": _utc_now(),
        "source_commit": session["source_commit"],
        "contract_sha256": session["contract_sha256"],
        "operator": operator,
        "roles": role_records,
    }
    if set(evidence) != EVIDENCE_KEYS:
        raise SessionError("internal evidence schema mismatch")
    evidence_path = _safe_relative(bundle, evidence_name)
    _write_new_json(evidence_path, evidence)
    try:
        verify_session(root, bundle, evidence_name)
    except SessionError:
        evidence_path.unlink(missing_ok=True)
        raise
    return evidence_path


def verify_session(
        root: Path, bundle: Path, evidence_name: str = "evidence.json") -> Path:
    session, contract, runs = load_session(bundle, root)
    evidence_path = _safe_relative(bundle, evidence_name)
    evidence = _load_object(evidence_path)
    if set(evidence) != EVIDENCE_KEYS:
        raise SessionError("evidence properties differ from the closed schema")
    if (evidence.get("schema") != "avionix.synthetic-vision-session-evidence.v1" or
            evidence.get("status") != "PASS" or
            evidence.get("flight_approval") is not False or
            not _is_utc(evidence.get("captured_utc")) or
            evidence.get("source_commit") != session["source_commit"] or
            evidence.get("contract_sha256") != session["contract_sha256"] or
            not isinstance(evidence.get("operator"), str) or
            not evidence["operator"].strip()):
        raise SessionError("evidence authority, state, timestamp, or identity is invalid")
    role_records = evidence.get("roles")
    if (not isinstance(role_records, list) or
            [item.get("role") for item in role_records if isinstance(item, dict)] != list(ROLES) or
            any(not isinstance(item, dict) or set(item) != EVIDENCE_ROLE_KEYS
                for item in role_records)):
        raise SessionError("evidence role records are invalid")
    for record, (results, artifact_root) in zip(role_records, runs):
        role = record["role"]
        expected_path = f"{role.lower()}/results.json"
        results_path = _safe_relative(bundle, record.get("results_file"))
        if (record.get("results_file") != expected_path or results_path.is_symlink() or
                not results_path.is_file() or digest(results_path) != record.get("results_sha256")):
            raise SessionError(f"{role} result file or digest is invalid")
        if (record.get("release_id") != results.get("release_id") or
                results.get("operator") != evidence["operator"]):
            raise SessionError(f"{role} release or operator differs from the evidence")
        errors = validate_results(
            results, contract, artifact_root, session["contract_sha256"])
        if errors:
            raise SessionError(
                f"{role} result no longer validates:\n  " + "\n  ".join(errors))
    return evidence_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    subparsers = parser.add_subparsers(dest="command", required=True)
    init_parser = subparsers.add_parser("init")
    init_parser.add_argument("--output", required=True, type=Path)
    init_parser.add_argument("--source-commit", required=True)
    finalize_parser = subparsers.add_parser("finalize")
    finalize_parser.add_argument("--bundle", required=True, type=Path)
    finalize_parser.add_argument("--operator", required=True)
    finalize_parser.add_argument("--confirmation", required=True)
    finalize_parser.add_argument("--evidence-name", default="evidence.json")
    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--bundle", required=True, type=Path)
    verify_parser.add_argument("--evidence-name", default="evidence.json")
    args = parser.parse_args()
    try:
        if args.command == "init":
            path = init_session(args.root, args.output, args.source_commit)
            print(f"SYNTHETIC VISION session PREPARED-SW/NOT-RUN: {path}")
        elif args.command == "finalize":
            path = finalize_session(
                args.root, args.bundle, args.operator, args.confirmation,
                args.evidence_name)
            print(f"SYNTHETIC VISION session evidence PASS: {path}")
        else:
            path = verify_session(args.root, args.bundle, args.evidence_name)
            print(f"SYNTHETIC VISION session verification PASS: {path}")
        return 0
    except (OSError, SessionError) as exc:
        print(f"SYNTHETIC VISION session FAILED: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
