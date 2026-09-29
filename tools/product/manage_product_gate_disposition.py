#!/usr/bin/env python3
"""Prepare and freeze product gate dispositions without authorizing deployment."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

try:
    from .manage_product_evidence_session import _atomic_json, _safe_local, validate_outcome
    from .validate_product_independent_review import load as load_review
except ImportError:
    from manage_product_evidence_session import _atomic_json, _safe_local, validate_outcome
    from validate_product_independent_review import load as load_review


ROOT = Path(__file__).resolve().parents[2]
DIGEST = re.compile(r"[0-9a-f]{64}")
ROOT_KEYS = {"schema", "domain", "status", "coordinator", "captured_utc", "outcome_path", "outcome_sha256", "review_gate_path", "review_gate_sha256", "required_authority_roles", "decision_authorities", "disposition", "justification", "conditions", "residual_risks", "change_control_required", "change_control_id", "disposition_complete", "hardware_verified", "deployment_allowed", "flight_approval", "decision"}
AUTHORITY_KEYS = {"role", "name", "decided_utc", "signature_artifact", "signature_sha256"}
RECORD_KEYS = {"schema", "domain", "status", "disposition_path", "disposition_sha256", "outcome_path", "outcome_sha256", "disposition", "justification", "conditions", "residual_risks", "authorities", "change_control_required", "change_control_id", "next_action", "hardware_verified", "deployment_allowed", "flight_approval", "decision"}
DISPOSITION_SPECS = {
    "engine": {"template": "config/product/engine-source-gate-disposition-not-run-v1.json", "gate": "config/product/engine-source-review-gate-v1.json", "roles": ["product-authority", "safety-authority", "configuration-authority"]},
    "sku": {"template": "config/product/sku-release-gate-disposition-not-run-v1.json", "gate": "config/product/sku-release-review-gate-v1.json", "roles": ["product-authority", "safety-authority", "release-authority"]},
    "fpv": {"template": "config/video/fpv-benchmark-gate-disposition-not-run-v1.json", "gate": "config/video/fpv-benchmark-review-gate-v1.json", "roles": ["product-authority", "safety-authority", "network-authority"]},
    "voice": {"template": "config/voice/voice-cabin-gate-disposition-not-run-v1.json", "gate": "config/voice/voice-cabin-review-gate-v1.json", "roles": ["product-authority", "safety-authority", "operations-authority"]},
    "hil": {"template": "config/hil/hil-execution-gate-disposition-not-run-v1.json", "gate": "config/hil/hil-execution-review-gate-v1.json", "roles": ["test-authority", "safety-authority", "configuration-authority"]},
}


class GateDispositionError(ValueError):
    """The disposition is incomplete, stale, unsafe, or over-authoritative."""


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise GateDispositionError(f"{control}: {message}")


def _utc(value: object) -> bool:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        return parsed.utcoffset() is not None and parsed.utcoffset().total_seconds() == 0
    except (TypeError, ValueError):
        return False


def _repo_file(root: Path, raw: object) -> Path | None:
    if not isinstance(raw, str):
        return None
    relative = PurePosixPath(raw)
    if relative.is_absolute() or ".." in relative.parts:
        return None
    candidate = root.resolve().joinpath(*relative.parts)
    resolved = candidate.resolve()
    if root.resolve() not in resolved.parents or candidate.is_symlink() or not resolved.is_file():
        return None
    return resolved


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise GateDispositionError("disposition root must be an object")
    return value


def _validate_outcome_link(data: dict, root: Path, session_dir: Path, control: str) -> dict:
    outcome_path = _safe_local(session_dir, data.get("outcome_path"))
    require(outcome_path is not None, control, "review outcome is missing, empty or unsafe")
    require(DIGEST.fullmatch(str(data.get("outcome_sha256", ""))) is not None and data["outcome_sha256"] == hashlib.sha256(outcome_path.read_bytes()).hexdigest(), control, "review outcome digest is malformed or stale")
    return validate_outcome(root, session_dir)


def validate_disposition(data: dict, root: Path, session_dir: Path, allow_not_run: bool = False) -> dict:
    require(data.get("schema") == "avionix-next.product-gate-disposition.v1", "PGD-01", "wrong disposition schema")
    require(set(data) == ROOT_KEYS, "PGD-02", "disposition properties differ from the closed schema")
    domain = data.get("domain")
    require(domain in DISPOSITION_SPECS, "PGD-03", "unknown product domain")
    spec = DISPOSITION_SPECS[domain]
    gate = _repo_file(root, data.get("review_gate_path"))
    require(data.get("review_gate_path") == spec["gate"] and gate is not None, "PGD-04", "review gate path is missing, unsafe or unexpected")
    require(DIGEST.fullmatch(str(data.get("review_gate_sha256", ""))) is not None and data["review_gate_sha256"] == hashlib.sha256(gate.read_bytes()).hexdigest(), "PGD-05", "review gate digest is malformed or stale")
    gate_data = json.loads(gate.read_text(encoding="utf-8"))
    require(gate_data.get("status") == "PREPARED_SW_NOT_RUN" and gate_data.get("flight_approval") is False, "PGD-06", "review gate is not fail-closed")
    require(data.get("required_authority_roles") == spec["roles"], "PGD-07", "decision authority roles changed")
    require(data.get("change_control_required") is True and data.get("change_control_id") is None, "PGD-08", "separate change control boundary was bypassed")
    require(data.get("hardware_verified") is False and data.get("deployment_allowed") is False and data.get("flight_approval") is False, "PGD-09", "disposition claims forbidden operational authority")
    for key in ("conditions", "residual_risks"):
        require(isinstance(data.get(key), list) and all(isinstance(item, str) and item.strip() for item in data[key]), "PGD-10", f"{key} are invalid")

    if data.get("status") == "NOT_RUN" and allow_not_run:
        require(data.get("coordinator") is None and data.get("captured_utc") is None, "PGD-11", "NOT_RUN disposition must be unattributed")
        require(data.get("outcome_path") is None and data.get("outcome_sha256") == "UNRESOLVED", "PGD-12", "NOT_RUN disposition must not claim an outcome")
        require(data.get("decision_authorities") == [] and data.get("disposition") == "UNRESOLVED" and data.get("justification") is None, "PGD-13", "NOT_RUN disposition contains a decision")
        require(data.get("conditions") == [] and data.get("residual_risks") == [] and data.get("disposition_complete") is False and data.get("decision") == "NO_GO_NOT_RUN", "PGD-14", "NOT_RUN disposition is not fail-closed")
        return {"schema": "avionix-next.product-gate-disposition-validation.v1", "status": "PASS", "domain": domain, "execution_status": "NOT_RUN", "decision": "NO_GO_NOT_RUN", "deployment_allowed": False, "flight_approval": False}

    if data.get("status") == "PREPARED_NOT_RUN" and allow_not_run:
        require(data.get("coordinator") is None and data.get("captured_utc") is None, "PGD-11", "prepared disposition must remain unattributed")
        outcome = _validate_outcome_link(data, root, session_dir, "PGD-12")
        require(outcome.get("domain") == domain, "PGD-13", "review outcome belongs to another domain")
        require(data.get("decision_authorities") == [] and data.get("disposition") == "UNRESOLVED" and data.get("justification") is None, "PGD-14", "prepared disposition contains a decision")
        require(data.get("conditions") == [] and data.get("residual_risks") == [] and data.get("disposition_complete") is False and data.get("decision") == "NO_GO_PENDING_GATE_DISPOSITION", "PGD-15", "prepared disposition is not fail-closed")
        return {"schema": "avionix-next.product-gate-disposition-validation.v1", "status": "PASS", "domain": domain, "execution_status": "PREPARED_NOT_RUN", "decision": "NO_GO_PENDING_GATE_DISPOSITION", "deployment_allowed": False, "flight_approval": False}

    require(data.get("status") == "READY_FOR_DISPOSITION_VALIDATION", "PGD-11", "disposition is not explicitly ready")
    require(isinstance(data.get("coordinator"), str) and data["coordinator"].strip() and _utc(data.get("captured_utc")), "PGD-12", "coordinator or UTC timestamp is invalid")
    outcome = _validate_outcome_link(data, root, session_dir, "PGD-13")
    require(outcome.get("domain") == domain, "PGD-14", "review outcome belongs to another domain")
    authorities = data.get("decision_authorities")
    require(isinstance(authorities, list) and [row.get("role") for row in authorities if isinstance(row, dict)] == spec["roles"], "PGD-15", "decision authorities are incomplete or out of order")
    authority_names: list[str] = []
    for authority in authorities:
        require(isinstance(authority, dict) and set(authority) == AUTHORITY_KEYS, "PGD-16", "authority properties differ from the closed schema")
        require(isinstance(authority.get("name"), str) and authority["name"].strip() and _utc(authority.get("decided_utc")), "PGD-17", "authority identity or timestamp is invalid")
        authority_names.append(authority["name"].strip().casefold())
        signature = _safe_local(session_dir, authority.get("signature_artifact"))
        require(signature is not None, "PGD-18", f"{authority.get('role')} signature is missing or unsafe")
        require(DIGEST.fullmatch(str(authority.get("signature_sha256", ""))) is not None and authority["signature_sha256"] == hashlib.sha256(signature.read_bytes()).hexdigest(), "PGD-19", f"{authority['role']} signature digest is malformed or stale")
    require(len(set(authority_names)) == len(spec["roles"]), "PGD-20", "decision authorities must have distinct identities")
    review_path = _safe_local(session_dir, outcome.get("review_path"))
    require(review_path is not None, "PGD-21", "linked independent review is unavailable")
    review = load_review(review_path)
    reviewer_names = {row["name"].strip().casefold() for row in review.get("reviewers", []) if isinstance(row, dict) and isinstance(row.get("name"), str)}
    require(not reviewer_names.intersection(authority_names), "PGD-22", "decision authorities must be independent from evidence reviewers")
    require(data.get("disposition") in ("ACCEPT_FOR_CHANGE_CONTROL", "REJECT"), "PGD-23", "disposition is invalid")
    require(isinstance(data.get("justification"), str) and data["justification"].strip(), "PGD-24", "disposition justification is missing")
    require(len(data["residual_risks"]) > 0, "PGD-25", "residual risks require an explicit statement")
    require(data.get("disposition_complete") is True, "PGD-26", "disposition is not complete")
    accepted = data["disposition"] == "ACCEPT_FOR_CHANGE_CONTROL"
    if accepted:
        require(outcome.get("next_gate_eligible") is True and outcome.get("review_decision") == "EVIDENCE_ACCEPTED_FOR_NEXT_GATE", "PGD-27", "review outcome is not eligible for acceptance")
    expected = "ELIGIBLE_FOR_SEPARATE_CHANGE_CONTROL_REQUEST" if accepted else "NO_GO_GATE_REJECTED"
    require(data.get("decision") == expected, "PGD-28", "decision does not match disposition")
    return {"schema": "avionix-next.product-gate-disposition-validation.v1", "status": "PASS", "domain": domain, "execution_status": "DISPOSITION_COMPLETE", "authorities": len(authorities), "disposition": data["disposition"], "decision": expected, "hardware_verified": False, "deployment_allowed": False, "flight_approval": False}


def validate_templates(root: Path = ROOT) -> dict:
    for domain, spec in DISPOSITION_SPECS.items():
        path = root / spec["template"]
        report = validate_disposition(load(path), root, path.parent, allow_not_run=True)
        require(report["domain"] == domain, "PGD-29", "template domain mismatch")
    return {"schema": "avionix-next.product-gate-disposition-templates-validation.v1", "status": "PASS", "templates": 5, "execution_status": "NOT_RUN", "deployment_allowed": False, "flight_approval": False, "decision": "NO_GO_PENDING_GATE_DISPOSITION"}


def init_disposition(root: Path, session_dir: Path) -> dict:
    outcome = validate_outcome(root, session_dir)
    domain = outcome["domain"]
    output = session_dir / "gate-disposition.json"
    if output.exists():
        raise GateDispositionError("gate disposition already exists; dispositions are never overwritten")
    data = load(root / DISPOSITION_SPECS[domain]["template"])
    data["status"] = "PREPARED_NOT_RUN"
    data["outcome_path"] = "review-outcome.json"
    data["outcome_sha256"] = hashlib.sha256((session_dir / "review-outcome.json").read_bytes()).hexdigest()
    data["decision"] = "NO_GO_PENDING_GATE_DISPOSITION"
    _atomic_json(output, data)
    return validate_disposition(data, root, session_dir, allow_not_run=True)


def finalize_disposition(root: Path, session_dir: Path) -> dict:
    disposition_path = session_dir / "gate-disposition.json"
    data = load(disposition_path)
    report = validate_disposition(data, root, session_dir)
    output = session_dir / "gate-disposition-record.json"
    if output.exists():
        raise GateDispositionError("gate disposition record already exists; records are immutable")
    accepted = report["disposition"] == "ACCEPT_FOR_CHANGE_CONTROL"
    record = {
        "schema": "avionix-next.product-gate-disposition-record.v1",
        "domain": report["domain"],
        "status": "DISPOSITION_RECORDED_CHANGE_CONTROL_REQUIRED",
        "disposition_path": disposition_path.name,
        "disposition_sha256": hashlib.sha256(disposition_path.read_bytes()).hexdigest(),
        "outcome_path": data["outcome_path"],
        "outcome_sha256": data["outcome_sha256"],
        "disposition": report["disposition"],
        "justification": data["justification"],
        "conditions": data["conditions"],
        "residual_risks": data["residual_risks"],
        "authorities": report["authorities"],
        "change_control_required": True,
        "change_control_id": None,
        "next_action": "CREATE_SEPARATE_CHANGE_CONTROL_REQUEST" if accepted else "REMEDIATE_REVIEW_FINDINGS",
        "hardware_verified": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "decision": "NO_GO_CHANGE_CONTROL_NOT_CREATED" if accepted else "NO_GO_GATE_REJECTED",
    }
    _atomic_json(output, record)
    return validate_record(root, session_dir, record)


def validate_record(root: Path, session_dir: Path, record: dict | None = None) -> dict:
    path = session_dir / "gate-disposition-record.json"
    data = record if record is not None else load(path)
    require(set(data) == RECORD_KEYS and data.get("schema") == "avionix-next.product-gate-disposition-record.v1", "PGD-30", "record properties or schema are invalid")
    disposition_path = _safe_local(session_dir, data.get("disposition_path"))
    require(disposition_path is not None and data.get("disposition_sha256") == hashlib.sha256(disposition_path.read_bytes()).hexdigest(), "PGD-31", "record disposition source is missing, unsafe or modified")
    disposition = load(disposition_path)
    report = validate_disposition(disposition, root, session_dir)
    accepted = report["disposition"] == "ACCEPT_FOR_CHANGE_CONTROL"
    expected_next = "CREATE_SEPARATE_CHANGE_CONTROL_REQUEST" if accepted else "REMEDIATE_REVIEW_FINDINGS"
    expected_decision = "NO_GO_CHANGE_CONTROL_NOT_CREATED" if accepted else "NO_GO_GATE_REJECTED"
    checks = (
        data.get("domain") == report["domain"],
        data.get("status") == "DISPOSITION_RECORDED_CHANGE_CONTROL_REQUIRED",
        data.get("outcome_path") == disposition["outcome_path"],
        data.get("outcome_sha256") == disposition["outcome_sha256"],
        data.get("disposition") == report["disposition"],
        data.get("justification") == disposition["justification"],
        data.get("conditions") == disposition["conditions"],
        data.get("residual_risks") == disposition["residual_risks"],
        data.get("authorities") == report["authorities"],
        data.get("change_control_required") is True,
        data.get("change_control_id") is None,
        data.get("next_action") == expected_next,
        data.get("hardware_verified") is False,
        data.get("deployment_allowed") is False,
        data.get("flight_approval") is False,
        data.get("decision") == expected_decision,
    )
    require(all(checks), "PGD-32", "record is inconsistent or claims excess authority")
    return data


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("validate-templates")
    for command in ("init", "validate", "finalize", "verify-record"):
        item = subparsers.add_parser(command)
        item.add_argument("--session-dir", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.command == "validate-templates":
            report = validate_templates(args.root)
        elif args.command == "init":
            report = init_disposition(args.root, args.session_dir)
        elif args.command == "validate":
            path = args.session_dir / "gate-disposition.json"
            report = validate_disposition(load(path), args.root, args.session_dir, allow_not_run=True)
        elif args.command == "finalize":
            report = finalize_disposition(args.root, args.session_dir)
        else:
            report = validate_record(args.root, args.session_dir)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        print(f"AVIONIX product gate disposition: FAIL: {exc}")
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
