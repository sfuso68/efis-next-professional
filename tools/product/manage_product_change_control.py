#!/usr/bin/env python3
"""Prepare and freeze change-control requests without authorizing implementation."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

try:
    from .manage_product_evidence_session import _atomic_json, _safe_local
    from .manage_product_gate_disposition import load as load_disposition, validate_record as validate_disposition_record
    from .validate_product_independent_review import load as load_review
except ImportError:
    from manage_product_evidence_session import _atomic_json, _safe_local
    from manage_product_gate_disposition import load as load_disposition, validate_record as validate_disposition_record
    from validate_product_independent_review import load as load_review


ROOT = Path(__file__).resolve().parents[2]
DIGEST = re.compile(r"[0-9a-f]{64}")
REQUEST_ID = re.compile(r"CCR-[A-Z0-9][A-Z0-9._-]{5,63}")
ROOT_KEYS = {
    "schema", "domain", "status", "request_id", "proposer", "submitted_utc",
    "gate_disposition_template_path", "gate_disposition_template_sha256",
    "disposition_record_path", "disposition_record_sha256", "title",
    "change_summary", "affected_requirements", "affected_artifacts",
    "affected_nodes", "exclusions", "impact_analysis", "rollback_strategy",
    "verification_plan", "required_intake_roles", "intake_authorities",
    "request_complete", "implementation_authorized", "deployment_allowed",
    "flight_approval", "decision",
}
IMPACT_KEYS = {
    "safety", "architecture", "configuration", "cybersecurity",
    "human_factors", "operations", "verification",
}
AUTHORITY_KEYS = {"role", "name", "reviewed_utc", "signature_artifact", "signature_sha256"}
RECORD_KEYS = {
    "schema", "domain", "status", "request_path", "request_sha256",
    "disposition_record_path", "disposition_record_sha256", "request_id",
    "title", "change_summary", "affected_requirements", "affected_artifacts",
    "affected_nodes", "impact_analysis", "rollback_strategy", "verification_plan",
    "authorities", "implementation_authorized", "deployment_allowed",
    "flight_approval", "next_action", "decision",
}
CHANGE_SPECS = {
    "engine": {
        "template": "config/product/engine-source-change-control-request-not-run-v1.json",
        "disposition": "config/product/engine-source-gate-disposition-not-run-v1.json",
        "requirement": "EMS-004",
        "roles": ["change-control-chair", "safety-intake-authority", "verification-intake-authority"],
    },
    "sku": {
        "template": "config/product/sku-release-change-control-request-not-run-v1.json",
        "disposition": "config/product/sku-release-gate-disposition-not-run-v1.json",
        "requirement": "PROD-001",
        "roles": ["change-control-chair", "safety-intake-authority", "release-intake-authority"],
    },
    "fpv": {
        "template": "config/video/fpv-benchmark-change-control-request-not-run-v1.json",
        "disposition": "config/video/fpv-benchmark-gate-disposition-not-run-v1.json",
        "requirement": "FPV-002",
        "roles": ["change-control-chair", "safety-intake-authority", "network-intake-authority"],
    },
    "voice": {
        "template": "config/voice/voice-cabin-change-control-request-not-run-v1.json",
        "disposition": "config/voice/voice-cabin-gate-disposition-not-run-v1.json",
        "requirement": "VOI-003",
        "roles": ["change-control-chair", "safety-intake-authority", "operations-intake-authority"],
    },
    "hil": {
        "template": "config/hil/hil-execution-change-control-request-not-run-v1.json",
        "disposition": "config/hil/hil-execution-gate-disposition-not-run-v1.json",
        "requirement": "VER-002",
        "roles": ["change-control-chair", "safety-intake-authority", "test-intake-authority"],
    },
}


class ChangeControlError(ValueError):
    """The request is incomplete, stale, unsafe, or over-authoritative."""


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise ChangeControlError(f"{control}: {message}")


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


def _safe_declared_path(raw: object) -> bool:
    if not isinstance(raw, str) or not raw.strip():
        return False
    path = PurePosixPath(raw)
    return not path.is_absolute() and ".." not in path.parts and "\\" not in raw


def load(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ChangeControlError("change-control root must be an object")
    return value


def _validate_disposition_link(data: dict, root: Path, session_dir: Path, control: str) -> dict:
    require(data.get("disposition_record_path") == "gate-disposition-record.json", control, "unexpected gate disposition record path")
    record_path = _safe_local(session_dir, data.get("disposition_record_path"))
    require(record_path is not None, control, "gate disposition record is missing or unsafe")
    digest = str(data.get("disposition_record_sha256", ""))
    require(DIGEST.fullmatch(digest) is not None and digest == hashlib.sha256(record_path.read_bytes()).hexdigest(), control, "gate disposition record digest is malformed or stale")
    record = validate_disposition_record(root, session_dir)
    require(record.get("disposition") == "ACCEPT_FOR_CHANGE_CONTROL" and record.get("decision") == "NO_GO_CHANGE_CONTROL_NOT_CREATED", control, "gate disposition is not eligible for a change-control request")
    return record


def _prior_people(session_dir: Path, disposition_record: dict) -> set[str]:
    disposition_path = _safe_local(session_dir, disposition_record.get("disposition_path"))
    require(disposition_path is not None, "PCC-24", "source disposition is unavailable")
    disposition = load_disposition(disposition_path)
    names = {
        row["name"].strip().casefold()
        for row in disposition.get("decision_authorities", [])
        if isinstance(row, dict) and isinstance(row.get("name"), str)
    }
    outcome_path = _safe_local(session_dir, disposition.get("outcome_path"))
    require(outcome_path is not None, "PCC-24", "review outcome is unavailable")
    outcome = load(outcome_path)
    review_path = _safe_local(session_dir, outcome.get("review_path"))
    require(review_path is not None, "PCC-24", "independent review is unavailable")
    review = load_review(review_path)
    names.update(
        row["name"].strip().casefold()
        for row in review.get("reviewers", [])
        if isinstance(row, dict) and isinstance(row.get("name"), str)
    )
    return names


def validate_request(data: dict, root: Path, session_dir: Path, allow_not_run: bool = False) -> dict:
    require(data.get("schema") == "avionix-next.product-change-control-request.v1", "PCC-01", "wrong request schema")
    require(set(data) == ROOT_KEYS, "PCC-02", "request properties differ from the closed schema")
    domain = data.get("domain")
    require(domain in CHANGE_SPECS, "PCC-03", "unknown product domain")
    spec = CHANGE_SPECS[domain]
    disposition_template = _repo_file(root, data.get("gate_disposition_template_path"))
    require(data.get("gate_disposition_template_path") == spec["disposition"] and disposition_template is not None, "PCC-04", "gate disposition template path is unsafe or unexpected")
    digest = str(data.get("gate_disposition_template_sha256", ""))
    require(DIGEST.fullmatch(digest) is not None and digest == hashlib.sha256(disposition_template.read_bytes()).hexdigest(), "PCC-05", "gate disposition template digest is malformed or stale")
    require(data.get("required_intake_roles") == spec["roles"], "PCC-06", "intake authority roles changed")
    require(data.get("implementation_authorized") is False and data.get("deployment_allowed") is False and data.get("flight_approval") is False, "PCC-07", "request claims forbidden operational authority")
    require(isinstance(data.get("impact_analysis"), dict) and set(data["impact_analysis"]) == IMPACT_KEYS, "PCC-08", "impact-analysis properties differ from the closed schema")

    if data.get("status") == "NOT_RUN" and allow_not_run:
        require(data.get("request_id") is None and data.get("proposer") is None and data.get("submitted_utc") is None, "PCC-09", "NOT_RUN request must be unattributed")
        require(data.get("disposition_record_path") is None and data.get("disposition_record_sha256") == "UNRESOLVED", "PCC-10", "NOT_RUN request must not claim a disposition")
        empty_fields = ("title", "change_summary", "rollback_strategy", "verification_plan")
        require(all(data.get(key) is None for key in empty_fields), "PCC-11", "NOT_RUN request contains narrative")
        require(all(data.get(key) == [] for key in ("affected_requirements", "affected_artifacts", "affected_nodes", "exclusions", "intake_authorities")), "PCC-12", "NOT_RUN request contains scope or authorities")
        require(all(value is None for value in data["impact_analysis"].values()), "PCC-13", "NOT_RUN request contains impact claims")
        require(data.get("request_complete") is False and data.get("decision") == "NO_GO_NOT_RUN", "PCC-14", "NOT_RUN request is not fail-closed")
        return {"schema": "avionix-next.product-change-control-validation.v1", "status": "PASS", "domain": domain, "execution_status": "NOT_RUN", "decision": "NO_GO_NOT_RUN", "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}

    if data.get("status") == "PREPARED_NOT_RUN" and allow_not_run:
        record = _validate_disposition_link(data, root, session_dir, "PCC-10")
        require(record.get("domain") == domain, "PCC-11", "gate disposition belongs to another domain")
        require(data.get("request_id") is None and data.get("proposer") is None and data.get("submitted_utc") is None, "PCC-12", "prepared request must remain unattributed")
        empty_fields = ("title", "change_summary", "rollback_strategy", "verification_plan")
        require(all(data.get(key) is None for key in empty_fields), "PCC-13", "prepared request contains narrative")
        require(all(data.get(key) == [] for key in ("affected_requirements", "affected_artifacts", "affected_nodes", "exclusions", "intake_authorities")), "PCC-14", "prepared request contains scope or authorities")
        require(all(value is None for value in data["impact_analysis"].values()), "PCC-15", "prepared request contains impact claims")
        require(data.get("request_complete") is False and data.get("decision") == "NO_GO_PENDING_CHANGE_CONTROL_REQUEST", "PCC-16", "prepared request is not fail-closed")
        return {"schema": "avionix-next.product-change-control-validation.v1", "status": "PASS", "domain": domain, "execution_status": "PREPARED_NOT_RUN", "decision": "NO_GO_PENDING_CHANGE_CONTROL_REQUEST", "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}

    require(data.get("status") == "READY_FOR_REQUEST_VALIDATION", "PCC-09", "request is not explicitly ready")
    record = _validate_disposition_link(data, root, session_dir, "PCC-10")
    require(record.get("domain") == domain, "PCC-11", "gate disposition belongs to another domain")
    require(REQUEST_ID.fullmatch(str(data.get("request_id", ""))) is not None, "PCC-12", "request ID is invalid")
    require(isinstance(data.get("proposer"), str) and data["proposer"].strip() and _utc(data.get("submitted_utc")), "PCC-13", "proposer or submission timestamp is invalid")
    for key in ("title", "change_summary", "rollback_strategy", "verification_plan"):
        require(isinstance(data.get(key), str) and data[key].strip(), "PCC-14", f"{key} is missing")
    requirements = data.get("affected_requirements")
    require(isinstance(requirements, list) and spec["requirement"] in requirements and len(requirements) == len(set(requirements)), "PCC-15", "affected requirements are incomplete or duplicated")
    require(all(isinstance(item, str) and re.fullmatch(r"[A-Z][A-Z0-9-]*-[0-9]{3}", item) for item in requirements), "PCC-16", "affected requirement ID is malformed")
    artifacts = data.get("affected_artifacts")
    require(isinstance(artifacts, list) and artifacts and all(_safe_declared_path(item) for item in artifacts), "PCC-17", "affected artifacts are empty or unsafe")
    for key in ("affected_nodes", "exclusions"):
        values = data.get(key)
        require(isinstance(values, list) and values and all(isinstance(item, str) and item.strip() for item in values), "PCC-18", f"{key} are empty or invalid")
    require(all(isinstance(value, str) and value.strip() for value in data["impact_analysis"].values()), "PCC-19", "impact analysis is incomplete")
    authorities = data.get("intake_authorities")
    require(isinstance(authorities, list) and [row.get("role") for row in authorities if isinstance(row, dict)] == spec["roles"], "PCC-20", "intake authorities are incomplete or out of order")
    authority_names: list[str] = []
    for authority in authorities:
        require(isinstance(authority, dict) and set(authority) == AUTHORITY_KEYS, "PCC-21", "authority properties differ from the closed schema")
        require(isinstance(authority.get("name"), str) and authority["name"].strip() and _utc(authority.get("reviewed_utc")), "PCC-22", "authority identity or timestamp is invalid")
        authority_names.append(authority["name"].strip().casefold())
        signature = _safe_local(session_dir, authority.get("signature_artifact"))
        require(signature is not None, "PCC-23", f"{authority.get('role')} signature is missing or unsafe")
        signature_digest = str(authority.get("signature_sha256", ""))
        require(DIGEST.fullmatch(signature_digest) is not None and signature_digest == hashlib.sha256(signature.read_bytes()).hexdigest(), "PCC-23", f"{authority['role']} signature digest is malformed or stale")
    require(len(set(authority_names)) == len(spec["roles"]), "PCC-24", "intake authorities must have distinct identities")
    require(not _prior_people(session_dir, record).intersection(authority_names), "PCC-25", "intake authorities must be independent from evidence reviewers and disposition authorities")
    require(data.get("request_complete") is True and data.get("decision") == "CHANGE_CONTROL_REQUEST_RECORDED_PENDING_CCB_DECISION", "PCC-26", "request completion or decision is invalid")
    return {"schema": "avionix-next.product-change-control-validation.v1", "status": "PASS", "domain": domain, "execution_status": "REQUEST_COMPLETE", "request_id": data["request_id"], "authorities": len(authorities), "decision": data["decision"], "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}


def validate_templates(root: Path = ROOT) -> dict:
    for domain, spec in CHANGE_SPECS.items():
        path = root / spec["template"]
        report = validate_request(load(path), root, path.parent, allow_not_run=True)
        require(report["domain"] == domain, "PCC-27", "template domain mismatch")
    return {"schema": "avionix-next.product-change-control-templates-validation.v1", "status": "PASS", "templates": 5, "execution_status": "NOT_RUN", "decision": "NO_GO_PENDING_CHANGE_CONTROL_REQUEST", "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}


def init_request(root: Path, session_dir: Path) -> dict:
    disposition_record = validate_disposition_record(root, session_dir)
    require(disposition_record.get("disposition") == "ACCEPT_FOR_CHANGE_CONTROL" and disposition_record.get("decision") == "NO_GO_CHANGE_CONTROL_NOT_CREATED", "PCC-28", "disposition does not permit creating a request")
    domain = disposition_record["domain"]
    output = session_dir / "change-control-request.json"
    if output.exists():
        raise ChangeControlError("change-control request already exists; requests are never overwritten")
    data = load(root / CHANGE_SPECS[domain]["template"])
    data["status"] = "PREPARED_NOT_RUN"
    data["disposition_record_path"] = "gate-disposition-record.json"
    data["disposition_record_sha256"] = hashlib.sha256((session_dir / "gate-disposition-record.json").read_bytes()).hexdigest()
    data["decision"] = "NO_GO_PENDING_CHANGE_CONTROL_REQUEST"
    _atomic_json(output, data)
    return validate_request(data, root, session_dir, allow_not_run=True)


def finalize_request(root: Path, session_dir: Path) -> dict:
    request_path = session_dir / "change-control-request.json"
    data = load(request_path)
    report = validate_request(data, root, session_dir)
    output = session_dir / "change-control-request-record.json"
    if output.exists():
        raise ChangeControlError("change-control request record already exists; records are immutable")
    record = {
        "schema": "avionix-next.product-change-control-record.v1",
        "domain": report["domain"],
        "status": "REQUEST_RECORDED_PENDING_CCB_DECISION",
        "request_path": request_path.name,
        "request_sha256": hashlib.sha256(request_path.read_bytes()).hexdigest(),
        "disposition_record_path": data["disposition_record_path"],
        "disposition_record_sha256": data["disposition_record_sha256"],
        "request_id": data["request_id"],
        "title": data["title"],
        "change_summary": data["change_summary"],
        "affected_requirements": data["affected_requirements"],
        "affected_artifacts": data["affected_artifacts"],
        "affected_nodes": data["affected_nodes"],
        "impact_analysis": data["impact_analysis"],
        "rollback_strategy": data["rollback_strategy"],
        "verification_plan": data["verification_plan"],
        "authorities": report["authorities"],
        "implementation_authorized": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "next_action": "CONVENE_SEPARATE_CCB_DECISION",
        "decision": "NO_GO_IMPLEMENTATION_NOT_AUTHORIZED",
    }
    _atomic_json(output, record)
    return validate_record(root, session_dir, record)


def validate_record(root: Path, session_dir: Path, record: dict | None = None) -> dict:
    data = record if record is not None else load(session_dir / "change-control-request-record.json")
    require(set(data) == RECORD_KEYS and data.get("schema") == "avionix-next.product-change-control-record.v1", "PCC-29", "record properties or schema are invalid")
    request_path = _safe_local(session_dir, data.get("request_path"))
    require(request_path is not None and data.get("request_sha256") == hashlib.sha256(request_path.read_bytes()).hexdigest(), "PCC-30", "record request source is missing, unsafe or modified")
    request = load(request_path)
    report = validate_request(request, root, session_dir)
    checks = (
        data.get("domain") == report["domain"],
        data.get("status") == "REQUEST_RECORDED_PENDING_CCB_DECISION",
        data.get("disposition_record_path") == request["disposition_record_path"],
        data.get("disposition_record_sha256") == request["disposition_record_sha256"],
        data.get("request_id") == request["request_id"],
        data.get("title") == request["title"],
        data.get("change_summary") == request["change_summary"],
        data.get("affected_requirements") == request["affected_requirements"],
        data.get("affected_artifacts") == request["affected_artifacts"],
        data.get("affected_nodes") == request["affected_nodes"],
        data.get("impact_analysis") == request["impact_analysis"],
        data.get("rollback_strategy") == request["rollback_strategy"],
        data.get("verification_plan") == request["verification_plan"],
        data.get("authorities") == report["authorities"],
        data.get("implementation_authorized") is False,
        data.get("deployment_allowed") is False,
        data.get("flight_approval") is False,
        data.get("next_action") == "CONVENE_SEPARATE_CCB_DECISION",
        data.get("decision") == "NO_GO_IMPLEMENTATION_NOT_AUTHORIZED",
    )
    require(all(checks), "PCC-31", "record is inconsistent or claims excess authority")
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
            report = init_request(args.root, args.session_dir)
        elif args.command == "validate":
            report = validate_request(load(args.session_dir / "change-control-request.json"), args.root, args.session_dir, allow_not_run=True)
        elif args.command == "finalize":
            report = finalize_request(args.root, args.session_dir)
        else:
            report = validate_record(args.root, args.session_dir)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        print(f"AVIONIX product change control: FAIL: {exc}")
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
