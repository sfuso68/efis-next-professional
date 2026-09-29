#!/usr/bin/env python3
"""Prepare and freeze CCB decisions without authorizing implementation."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

try:
    from .manage_product_evidence_session import _atomic_json, _safe_local
    from .manage_product_change_control import load as load_change, validate_record as validate_request_record
    from .manage_product_gate_disposition import load as load_disposition
    from .validate_product_independent_review import load as load_review
except ImportError:
    from manage_product_evidence_session import _atomic_json, _safe_local
    from manage_product_change_control import load as load_change, validate_record as validate_request_record
    from manage_product_gate_disposition import load as load_disposition
    from validate_product_independent_review import load as load_review


ROOT = Path(__file__).resolve().parents[2]
DIGEST = re.compile(r"[0-9a-f]{64}")
MEETING_ID = re.compile(r"CCB-[A-Z0-9][A-Z0-9._-]{5,63}")
ROOT_KEYS = {
    "schema", "domain", "status", "meeting_id", "chair", "decided_utc",
    "change_control_template_path", "change_control_template_sha256",
    "request_record_path", "request_record_sha256", "required_board_roles",
    "board_members", "ccb_decision", "rationale", "conditions",
    "residual_risks", "quorum_complete", "implementation_plan_required",
    "implementation_plan_id", "implementation_authorized",
    "deployment_allowed", "flight_approval", "decision",
}
MEMBER_KEYS = {"role", "name", "voted_utc", "vote", "signature_artifact", "signature_sha256"}
RECORD_KEYS = {
    "schema", "domain", "status", "decision_path", "decision_sha256",
    "request_record_path", "request_record_sha256", "meeting_id",
    "ccb_decision", "rationale", "conditions", "residual_risks", "members",
    "implementation_plan_required", "implementation_plan_id",
    "implementation_authorized", "deployment_allowed", "flight_approval",
    "next_action", "decision",
}
CCB_SPECS = {
    "engine": {
        "template": "config/product/engine-source-ccb-decision-not-run-v1.json",
        "request": "config/product/engine-source-change-control-request-not-run-v1.json",
        "roles": ["ccb-chair", "safety-board-member", "configuration-board-member", "verification-board-member"],
    },
    "sku": {
        "template": "config/product/sku-release-ccb-decision-not-run-v1.json",
        "request": "config/product/sku-release-change-control-request-not-run-v1.json",
        "roles": ["ccb-chair", "product-board-member", "safety-board-member", "release-board-member"],
    },
    "fpv": {
        "template": "config/video/fpv-benchmark-ccb-decision-not-run-v1.json",
        "request": "config/video/fpv-benchmark-change-control-request-not-run-v1.json",
        "roles": ["ccb-chair", "safety-board-member", "network-board-member", "verification-board-member"],
    },
    "voice": {
        "template": "config/voice/voice-cabin-ccb-decision-not-run-v1.json",
        "request": "config/voice/voice-cabin-change-control-request-not-run-v1.json",
        "roles": ["ccb-chair", "safety-board-member", "operations-board-member", "human-factors-board-member"],
    },
    "hil": {
        "template": "config/hil/hil-execution-ccb-decision-not-run-v1.json",
        "request": "config/hil/hil-execution-change-control-request-not-run-v1.json",
        "roles": ["ccb-chair", "safety-board-member", "test-board-member", "configuration-board-member"],
    },
}
DECISION_VOTE = {
    "APPROVE_IMPLEMENTATION_PLANNING": "APPROVE",
    "REJECT": "REJECT",
    "DEFER": "DEFER",
}


class CcbDecisionError(ValueError):
    """The CCB record is incomplete, stale, conflicted, or over-authoritative."""


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise CcbDecisionError(f"{control}: {message}")


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
        raise CcbDecisionError("CCB decision root must be an object")
    return value


def _validate_request_link(data: dict, root: Path, session_dir: Path, control: str) -> dict:
    require(data.get("request_record_path") == "change-control-request-record.json", control, "unexpected request record path")
    path = _safe_local(session_dir, data.get("request_record_path"))
    require(path is not None, control, "change-control request record is missing or unsafe")
    digest = str(data.get("request_record_sha256", ""))
    require(DIGEST.fullmatch(digest) is not None and digest == hashlib.sha256(path.read_bytes()).hexdigest(), control, "change-control request record digest is malformed or stale")
    record = validate_request_record(root, session_dir)
    require(record.get("status") == "REQUEST_RECORDED_PENDING_CCB_DECISION" and record.get("decision") == "NO_GO_IMPLEMENTATION_NOT_AUTHORIZED", control, "request is not eligible for CCB review")
    return record


def _prior_people(session_dir: Path, request_record: dict) -> set[str]:
    request_path = _safe_local(session_dir, request_record.get("request_path"))
    require(request_path is not None, "PCB-22", "source request is unavailable")
    request = load_change(request_path)
    names = {
        row["name"].strip().casefold()
        for row in request.get("intake_authorities", [])
        if isinstance(row, dict) and isinstance(row.get("name"), str)
    }
    disposition_record_path = _safe_local(session_dir, request.get("disposition_record_path"))
    require(disposition_record_path is not None, "PCB-22", "disposition record is unavailable")
    disposition_record = load_disposition(disposition_record_path)
    disposition_path = _safe_local(session_dir, disposition_record.get("disposition_path"))
    require(disposition_path is not None, "PCB-22", "source disposition is unavailable")
    disposition = load_disposition(disposition_path)
    names.update(
        row["name"].strip().casefold()
        for row in disposition.get("decision_authorities", [])
        if isinstance(row, dict) and isinstance(row.get("name"), str)
    )
    outcome_path = _safe_local(session_dir, disposition.get("outcome_path"))
    require(outcome_path is not None, "PCB-22", "review outcome is unavailable")
    outcome = load(outcome_path)
    review_path = _safe_local(session_dir, outcome.get("review_path"))
    require(review_path is not None, "PCB-22", "independent review is unavailable")
    review = load_review(review_path)
    names.update(
        row["name"].strip().casefold()
        for row in review.get("reviewers", [])
        if isinstance(row, dict) and isinstance(row.get("name"), str)
    )
    return names


def validate_decision(data: dict, root: Path, session_dir: Path, allow_not_run: bool = False) -> dict:
    require(data.get("schema") == "avionix-next.product-ccb-decision.v1", "PCB-01", "wrong CCB schema")
    require(set(data) == ROOT_KEYS, "PCB-02", "CCB properties differ from the closed schema")
    domain = data.get("domain")
    require(domain in CCB_SPECS, "PCB-03", "unknown product domain")
    spec = CCB_SPECS[domain]
    request_template = _repo_file(root, data.get("change_control_template_path"))
    require(data.get("change_control_template_path") == spec["request"] and request_template is not None, "PCB-04", "request template path is unsafe or unexpected")
    template_digest = str(data.get("change_control_template_sha256", ""))
    require(DIGEST.fullmatch(template_digest) is not None and template_digest == hashlib.sha256(request_template.read_bytes()).hexdigest(), "PCB-05", "request template digest is malformed or stale")
    require(data.get("required_board_roles") == spec["roles"], "PCB-06", "board roles changed")
    require(data.get("implementation_plan_required") is True and data.get("implementation_plan_id") is None, "PCB-07", "separate implementation-plan boundary was bypassed")
    require(data.get("implementation_authorized") is False and data.get("deployment_allowed") is False and data.get("flight_approval") is False, "PCB-08", "CCB record claims forbidden operational authority")
    for key in ("conditions", "residual_risks"):
        require(isinstance(data.get(key), list) and all(isinstance(item, str) and item.strip() for item in data[key]), "PCB-09", f"{key} are invalid")

    if data.get("status") == "NOT_RUN" and allow_not_run:
        require(data.get("meeting_id") is None and data.get("chair") is None and data.get("decided_utc") is None, "PCB-10", "NOT_RUN CCB must be unattributed")
        require(data.get("request_record_path") is None and data.get("request_record_sha256") == "UNRESOLVED", "PCB-11", "NOT_RUN CCB must not claim a request")
        require(data.get("board_members") == [] and data.get("ccb_decision") == "UNRESOLVED" and data.get("rationale") is None, "PCB-12", "NOT_RUN CCB contains a decision")
        require(data.get("conditions") == [] and data.get("residual_risks") == [] and data.get("quorum_complete") is False and data.get("decision") == "NO_GO_NOT_RUN", "PCB-13", "NOT_RUN CCB is not fail-closed")
        return {"schema": "avionix-next.product-ccb-validation.v1", "status": "PASS", "domain": domain, "execution_status": "NOT_RUN", "decision": "NO_GO_NOT_RUN", "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}

    if data.get("status") == "PREPARED_NOT_RUN" and allow_not_run:
        request_record = _validate_request_link(data, root, session_dir, "PCB-11")
        require(request_record.get("domain") == domain, "PCB-12", "request belongs to another domain")
        require(data.get("meeting_id") is None and data.get("chair") is None and data.get("decided_utc") is None, "PCB-13", "prepared CCB must remain unattributed")
        require(data.get("board_members") == [] and data.get("ccb_decision") == "UNRESOLVED" and data.get("rationale") is None, "PCB-14", "prepared CCB contains a decision")
        require(data.get("conditions") == [] and data.get("residual_risks") == [] and data.get("quorum_complete") is False, "PCB-15", "prepared CCB contains premature claims")
        require(data.get("decision") == "NO_GO_PENDING_CCB_DECISION", "PCB-16", "prepared CCB is not fail-closed")
        return {"schema": "avionix-next.product-ccb-validation.v1", "status": "PASS", "domain": domain, "execution_status": "PREPARED_NOT_RUN", "decision": "NO_GO_PENDING_CCB_DECISION", "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}

    require(data.get("status") == "READY_FOR_CCB_VALIDATION", "PCB-10", "CCB decision is not explicitly ready")
    request_record = _validate_request_link(data, root, session_dir, "PCB-11")
    require(request_record.get("domain") == domain, "PCB-12", "request belongs to another domain")
    require(MEETING_ID.fullmatch(str(data.get("meeting_id", ""))) is not None, "PCB-13", "meeting ID is invalid")
    require(isinstance(data.get("chair"), str) and data["chair"].strip() and _utc(data.get("decided_utc")), "PCB-14", "chair or decision timestamp is invalid")
    decision = data.get("ccb_decision")
    require(decision in DECISION_VOTE, "PCB-15", "CCB decision is invalid")
    require(isinstance(data.get("rationale"), str) and data["rationale"].strip(), "PCB-16", "CCB rationale is missing")
    require(len(data["residual_risks"]) > 0, "PCB-17", "residual risks require an explicit statement")
    members = data.get("board_members")
    require(isinstance(members, list) and [row.get("role") for row in members if isinstance(row, dict)] == spec["roles"], "PCB-18", "board membership is incomplete or out of order")
    names: list[str] = []
    for member in members:
        require(isinstance(member, dict) and set(member) == MEMBER_KEYS, "PCB-19", "board member properties differ from the closed schema")
        require(isinstance(member.get("name"), str) and member["name"].strip() and _utc(member.get("voted_utc")), "PCB-20", "board identity or timestamp is invalid")
        require(member.get("vote") == DECISION_VOTE[decision], "PCB-21", "V1 requires unanimous votes matching the decision")
        names.append(member["name"].strip().casefold())
        signature = _safe_local(session_dir, member.get("signature_artifact"))
        require(signature is not None, "PCB-22", f"{member.get('role')} signature is missing or unsafe")
        signature_digest = str(member.get("signature_sha256", ""))
        require(DIGEST.fullmatch(signature_digest) is not None and signature_digest == hashlib.sha256(signature.read_bytes()).hexdigest(), "PCB-22", f"{member['role']} signature digest is malformed or stale")
    require(len(set(names)) == len(spec["roles"]), "PCB-23", "board members must have distinct identities")
    require(data["chair"].strip().casefold() == names[0], "PCB-24", "named chair must match the CCB chair member")
    require(not _prior_people(session_dir, request_record).intersection(names), "PCB-25", "board must be independent from prior reviewers and authorities")
    require(data.get("quorum_complete") is True, "PCB-26", "CCB quorum is incomplete")
    expected = {
        "APPROVE_IMPLEMENTATION_PLANNING": "ELIGIBLE_FOR_SEPARATE_IMPLEMENTATION_PLAN",
        "REJECT": "NO_GO_CCB_REJECTED",
        "DEFER": "NO_GO_CCB_DEFERRED",
    }[decision]
    require(data.get("decision") == expected, "PCB-27", "outcome does not match the CCB decision")
    return {"schema": "avionix-next.product-ccb-validation.v1", "status": "PASS", "domain": domain, "execution_status": "CCB_COMPLETE", "meeting_id": data["meeting_id"], "members": len(members), "ccb_decision": decision, "decision": expected, "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}


def validate_templates(root: Path = ROOT) -> dict:
    for domain, spec in CCB_SPECS.items():
        path = root / spec["template"]
        report = validate_decision(load(path), root, path.parent, allow_not_run=True)
        require(report["domain"] == domain, "PCB-28", "template domain mismatch")
    return {"schema": "avionix-next.product-ccb-templates-validation.v1", "status": "PASS", "templates": 5, "execution_status": "NOT_RUN", "decision": "NO_GO_PENDING_CCB_DECISION", "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}


def init_decision(root: Path, session_dir: Path) -> dict:
    request_record = validate_request_record(root, session_dir)
    require(request_record.get("status") == "REQUEST_RECORDED_PENDING_CCB_DECISION" and request_record.get("decision") == "NO_GO_IMPLEMENTATION_NOT_AUTHORIZED", "PCB-29", "request does not permit opening a CCB decision")
    domain = request_record["domain"]
    output = session_dir / "ccb-decision.json"
    if output.exists():
        raise CcbDecisionError("CCB decision already exists; decisions are never overwritten")
    data = load(root / CCB_SPECS[domain]["template"])
    data["status"] = "PREPARED_NOT_RUN"
    data["request_record_path"] = "change-control-request-record.json"
    data["request_record_sha256"] = hashlib.sha256((session_dir / "change-control-request-record.json").read_bytes()).hexdigest()
    data["decision"] = "NO_GO_PENDING_CCB_DECISION"
    _atomic_json(output, data)
    return validate_decision(data, root, session_dir, allow_not_run=True)


def finalize_decision(root: Path, session_dir: Path) -> dict:
    decision_path = session_dir / "ccb-decision.json"
    data = load(decision_path)
    report = validate_decision(data, root, session_dir)
    output = session_dir / "ccb-decision-record.json"
    if output.exists():
        raise CcbDecisionError("CCB decision record already exists; records are immutable")
    next_action, final_decision = {
        "APPROVE_IMPLEMENTATION_PLANNING": ("CREATE_SEPARATE_IMPLEMENTATION_PLAN", "NO_GO_IMPLEMENTATION_PLAN_NOT_CREATED"),
        "REJECT": ("REMEDIATE_CHANGE_CONTROL_REQUEST", "NO_GO_CCB_REJECTED"),
        "DEFER": ("RESOLVE_CCB_DEFERRAL", "NO_GO_CCB_DEFERRED"),
    }[report["ccb_decision"]]
    planning_approved = report["ccb_decision"] == "APPROVE_IMPLEMENTATION_PLANNING"
    record = {
        "schema": "avionix-next.product-ccb-decision-record.v1",
        "domain": report["domain"],
        "status": "CCB_DECISION_RECORDED_IMPLEMENTATION_PLAN_REQUIRED" if planning_approved else "CCB_DECISION_RECORDED_NO_GO",
        "decision_path": decision_path.name,
        "decision_sha256": hashlib.sha256(decision_path.read_bytes()).hexdigest(),
        "request_record_path": data["request_record_path"],
        "request_record_sha256": data["request_record_sha256"],
        "meeting_id": data["meeting_id"],
        "ccb_decision": report["ccb_decision"],
        "rationale": data["rationale"],
        "conditions": data["conditions"],
        "residual_risks": data["residual_risks"],
        "members": report["members"],
        "implementation_plan_required": planning_approved,
        "implementation_plan_id": None,
        "implementation_authorized": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "next_action": next_action,
        "decision": final_decision,
    }
    _atomic_json(output, record)
    return validate_record(root, session_dir, record)


def validate_record(root: Path, session_dir: Path, record: dict | None = None) -> dict:
    data = record if record is not None else load(session_dir / "ccb-decision-record.json")
    require(set(data) == RECORD_KEYS and data.get("schema") == "avionix-next.product-ccb-decision-record.v1", "PCB-30", "record properties or schema are invalid")
    decision_path = _safe_local(session_dir, data.get("decision_path"))
    require(decision_path is not None and data.get("decision_sha256") == hashlib.sha256(decision_path.read_bytes()).hexdigest(), "PCB-31", "record decision source is missing, unsafe or modified")
    decision = load(decision_path)
    report = validate_decision(decision, root, session_dir)
    expected_next, expected_decision = {
        "APPROVE_IMPLEMENTATION_PLANNING": ("CREATE_SEPARATE_IMPLEMENTATION_PLAN", "NO_GO_IMPLEMENTATION_PLAN_NOT_CREATED"),
        "REJECT": ("REMEDIATE_CHANGE_CONTROL_REQUEST", "NO_GO_CCB_REJECTED"),
        "DEFER": ("RESOLVE_CCB_DEFERRAL", "NO_GO_CCB_DEFERRED"),
    }[report["ccb_decision"]]
    planning_approved = report["ccb_decision"] == "APPROVE_IMPLEMENTATION_PLANNING"
    checks = (
        data.get("domain") == report["domain"],
        data.get("status") == ("CCB_DECISION_RECORDED_IMPLEMENTATION_PLAN_REQUIRED" if planning_approved else "CCB_DECISION_RECORDED_NO_GO"),
        data.get("request_record_path") == decision["request_record_path"],
        data.get("request_record_sha256") == decision["request_record_sha256"],
        data.get("meeting_id") == decision["meeting_id"],
        data.get("ccb_decision") == report["ccb_decision"],
        data.get("rationale") == decision["rationale"],
        data.get("conditions") == decision["conditions"],
        data.get("residual_risks") == decision["residual_risks"],
        data.get("members") == report["members"],
        data.get("implementation_plan_required") is planning_approved,
        data.get("implementation_plan_id") is None,
        data.get("implementation_authorized") is False,
        data.get("deployment_allowed") is False,
        data.get("flight_approval") is False,
        data.get("next_action") == expected_next,
        data.get("decision") == expected_decision,
    )
    require(all(checks), "PCB-32", "record is inconsistent or claims excess authority")
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
            report = init_decision(args.root, args.session_dir)
        elif args.command == "validate":
            report = validate_decision(load(args.session_dir / "ccb-decision.json"), args.root, args.session_dir, allow_not_run=True)
        elif args.command == "finalize":
            report = finalize_decision(args.root, args.session_dir)
        else:
            report = validate_record(args.root, args.session_dir)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        print(f"AVIONIX product CCB decision: FAIL: {exc}")
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
