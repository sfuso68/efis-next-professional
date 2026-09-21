#!/usr/bin/env python3
"""Prepare and finalize fail-closed HMI hardware acceptance sessions.

This utility never drives a display, injects a fault, or invents a hardware
result.  ``init`` creates a safe, empty bundle.  ``finalize`` accepts only
three complete role records and an explicit operator confirmation, then uses
the existing evidence packager and independent validator.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile

try:
    from .assemble_hmi_hardware_evidence import assemble, validate_results
except ImportError:
    # Some embedded Python runtimes run in isolated mode and omit the script
    # directory from sys.path.  Keep direct CLI execution deterministic there.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from assemble_hmi_hardware_evidence import assemble, validate_results


ROLES = ("PFD", "MFD", "EMS")
KINDS = (
    "SCREENSHOT", "FRAME_TIMING", "THERMAL", "TOUCH_LOG", "BOOT_LOG",
    "TRANSPORT_REPLAY", "FAULT_RECOVERY", "OPERATOR_SIGNOFF",
)
CONFIRMATION = "I_CONFIRM_REAL_HARDWARE_OBSERVATION"
COMMIT = re.compile(r"[0-9a-f]{40}")
SESSION_KEYS = {
    "schema", "status", "flight_approval", "created_utc", "source_commit",
    "plan", "plan_sha256", "roles",
}
ROLE_KEYS = {"role", "run_file", "required_artifact_kinds"}
RUN_KEYS = {"role", "node", "release", "measurements", "artifacts"}


class SessionError(ValueError):
    """Raised when a session would weaken the fail-closed evidence boundary."""


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _is_utc(value: object) -> bool:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        return (parsed.utcoffset() is not None and
                parsed.utcoffset().total_seconds() == 0)
    except (TypeError, ValueError):
        return False


def _digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _write_new_json(path: Path, value: dict) -> None:
    if path.exists():
        raise SessionError(f"refusing to overwrite {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
            "w", encoding="utf-8", dir=path.parent, delete=False) as handle:
        temporary = Path(handle.name)
        json.dump(value, handle, indent=2)
        handle.write("\n")
    temporary.replace(path)


def _load_object(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
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
    resolved = (base / relative).resolve()
    if base.resolve() not in resolved.parents:
        raise SessionError(f"path escapes the session: {value}")
    return resolved


def init_session(root: Path, output: Path, source_commit: str) -> Path:
    if not COMMIT.fullmatch(source_commit):
        raise SessionError("source commit must be a full lowercase Git object ID")
    if output.exists():
        raise SessionError(f"session directory already exists: {output}")
    plan_source = root / "config/ui/avionix-hmi-hardware-acceptance-v1.json"
    if not plan_source.is_file():
        raise SessionError(f"acceptance plan is missing: {plan_source}")

    output.mkdir(parents=True)
    plan_target = output / "acceptance-plan.json"
    plan_target.write_bytes(plan_source.read_bytes())
    roles = []
    for role in ROLES:
        role_dir = output / role.lower()
        (role_dir / "artifacts").mkdir(parents=True)
        run_path = role_dir / "run.json"
        _write_new_json(run_path, {
            "role": role,
            "node": None,
            "release": None,
            "measurements": {},
            "artifacts": {},
        })
        roles.append({
            "role": role,
            "run_file": f"{role.lower()}/run.json",
            "required_artifact_kinds": list(KINDS),
        })

    _write_new_json(output / "results.json", {
        "schema": "avionix.hmi-hardware-results.v1",
        "status": "NOT_RUN",
        "operator_confirmed": False,
        "operator": None,
        "captured_utc": None,
        "runs": [
            {"role": role, "node": None, "release": None,
             "measurements": {}, "artifacts": {}}
            for role in ROLES
        ],
    })
    session_path = output / "session.json"
    _write_new_json(session_path, {
        "schema": "avionix.hmi-hardware-session.v1",
        "status": "PREPARED_SW_NOT_RUN",
        "flight_approval": False,
        "created_utc": _utc_now(),
        "source_commit": source_commit,
        "plan": "acceptance-plan.json",
        "plan_sha256": _digest(plan_target),
        "roles": roles,
    })
    return session_path


def load_session(bundle: Path, root: Path | None = None) -> tuple[dict, list[dict]]:
    session = _load_object(bundle / "session.json")
    if set(session) != SESSION_KEYS:
        raise SessionError("session properties differ from the closed schema")
    if (session.get("schema") != "avionix.hmi-hardware-session.v1" or
            session.get("status") != "PREPARED_SW_NOT_RUN" or
            session.get("flight_approval") is not False or
            not COMMIT.fullmatch(str(session.get("source_commit"))) or
            not _is_utc(session.get("created_utc"))):
        raise SessionError("session authority, state, or source commit is invalid")
    plan = _safe_relative(bundle, session.get("plan"))
    if not plan.is_file() or _digest(plan) != session.get("plan_sha256"):
        raise SessionError("acceptance plan is missing or its digest changed")
    if root is not None:
        canonical = root / "config/ui/avionix-hmi-hardware-acceptance-v1.json"
        if not canonical.is_file() or _digest(canonical) != session.get("plan_sha256"):
            raise SessionError("session plan does not match the canonical plan")
    roles = session.get("roles")
    if (not isinstance(roles, list) or
            [item.get("role") for item in roles if isinstance(item, dict)] != list(ROLES) or
            any(not isinstance(item, dict) or set(item) != ROLE_KEYS for item in roles)):
        raise SessionError("session role list is invalid")
    runs = []
    for item in roles:
        if item["required_artifact_kinds"] != list(KINDS):
            raise SessionError(f"{item['role']} artifact contract changed")
        run_path = _safe_relative(bundle, item["run_file"])
        run = _load_object(run_path)
        if set(run) != RUN_KEYS or run.get("role") != item["role"]:
            raise SessionError(f"{item['role']} run schema is invalid")
        runs.append(run)
    return session, runs


def finalize_session(
        root: Path, bundle: Path, operator: str, confirmation: str,
        evidence_name: str = "evidence.json") -> Path:
    if confirmation != CONFIRMATION:
        raise SessionError("explicit real-hardware operator confirmation is required")
    if not operator.strip():
        raise SessionError("operator name is required")
    _, runs = load_session(bundle, root)
    captured = _utc_now()
    results = {
        "schema": "avionix.hmi-hardware-results.v1",
        "status": "READY_FOR_ASSEMBLY",
        "operator_confirmed": True,
        "operator": operator.strip(),
        "captured_utc": captured,
        "runs": runs,
    }
    errors = validate_results(results, bundle)
    if errors:
        raise SessionError("hardware session is incomplete:\n  " + "\n  ".join(errors))
    results_path = bundle / "results-ready.json"
    _write_new_json(results_path, results)
    evidence_path = _safe_relative(bundle, evidence_name)
    errors = assemble(root, results_path, evidence_path)
    if errors:
        results_path.unlink(missing_ok=True)
        raise SessionError("evidence assembly failed:\n  " + "\n  ".join(errors))
    return evidence_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[2])
    subparsers = parser.add_subparsers(dest="command", required=True)
    init_parser = subparsers.add_parser("init")
    init_parser.add_argument("--output", required=True, type=Path)
    init_parser.add_argument("--source-commit", required=True)
    finalize_parser = subparsers.add_parser("finalize")
    finalize_parser.add_argument("--bundle", required=True, type=Path)
    finalize_parser.add_argument("--operator", required=True)
    finalize_parser.add_argument("--confirmation", required=True)
    finalize_parser.add_argument("--evidence-name", default="evidence.json")
    args = parser.parse_args()
    try:
        if args.command == "init":
            path = init_session(args.root, args.output, args.source_commit)
            print(f"AVIONIX HMI session PREPARED-SW/NOT-RUN: {path}")
        else:
            path = finalize_session(
                args.root, args.bundle, args.operator, args.confirmation,
                args.evidence_name)
            print(f"AVIONIX HMI hardware evidence PASS: {path}")
        return 0
    except (OSError, SessionError) as exc:
        print(f"AVIONIX HMI session FAILED: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
