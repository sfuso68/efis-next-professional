#!/usr/bin/env python3
"""Prepare and freeze implementation plans without authorizing implementation."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

try:
    from .manage_product_evidence_session import _atomic_json, _safe_local
    from .manage_product_ccb_decision import _prior_people, load as load_ccb, validate_record as validate_ccb_record
except ImportError:
    from manage_product_evidence_session import _atomic_json, _safe_local
    from manage_product_ccb_decision import _prior_people, load as load_ccb, validate_record as validate_ccb_record


ROOT = Path(__file__).resolve().parents[2]
DIGEST = re.compile(r"[0-9a-f]{64}")
PLAN_ID = re.compile(r"IMP-[A-Z0-9][A-Z0-9._-]{5,63}")
COMMIT = re.compile(r"[0-9a-f]{40}")
ROOT_KEYS = {
    "schema", "domain", "status", "plan_id", "owner", "prepared_utc",
    "ccb_template_path", "ccb_template_sha256", "ccb_record_path",
    "ccb_record_sha256", "baseline_branch", "baseline_commit", "objective",
    "in_scope_requirements", "in_scope_artifacts", "in_scope_nodes",
    "out_of_scope", "work_packages", "hazard_controls", "verification_plan",
    "rollback_plan", "deployment_plan", "required_plan_roles",
    "plan_reviewers", "plan_complete", "implementation_authorized",
    "deployment_allowed", "flight_approval", "decision",
}
WORK_KEYS = {"id", "description", "artifacts", "verification", "evidence", "exit_criteria"}
REVIEWER_KEYS = {"role", "name", "reviewed_utc", "signature_artifact", "signature_sha256"}
RECORD_KEYS = {
    "schema", "domain", "status", "plan_path", "plan_sha256",
    "ccb_record_path", "ccb_record_sha256", "plan_id", "baseline_branch",
    "baseline_commit", "objective", "in_scope_requirements",
    "in_scope_artifacts", "in_scope_nodes", "work_packages", "hazard_controls",
    "verification_plan", "rollback_plan", "deployment_plan", "reviewers",
    "implementation_authorized", "deployment_allowed", "flight_approval",
    "next_action", "decision",
}
PLAN_SPECS = {
    "engine": {
        "template": "config/product/engine-source-implementation-plan-not-run-v1.json",
        "ccb": "config/product/engine-source-ccb-decision-not-run-v1.json",
        "requirement": "EMS-004",
        "roles": ["implementation-lead", "safety-plan-reviewer", "verification-plan-reviewer"],
    },
    "sku": {
        "template": "config/product/sku-release-implementation-plan-not-run-v1.json",
        "ccb": "config/product/sku-release-ccb-decision-not-run-v1.json",
        "requirement": "PROD-001",
        "roles": ["implementation-lead", "safety-plan-reviewer", "release-plan-reviewer"],
    },
    "fpv": {
        "template": "config/video/fpv-benchmark-implementation-plan-not-run-v1.json",
        "ccb": "config/video/fpv-benchmark-ccb-decision-not-run-v1.json",
        "requirement": "FPV-002",
        "roles": ["implementation-lead", "safety-plan-reviewer", "network-plan-reviewer"],
    },
    "voice": {
        "template": "config/voice/voice-cabin-implementation-plan-not-run-v1.json",
        "ccb": "config/voice/voice-cabin-ccb-decision-not-run-v1.json",
        "requirement": "VOI-003",
        "roles": ["implementation-lead", "safety-plan-reviewer", "human-factors-plan-reviewer"],
    },
    "hil": {
        "template": "config/hil/hil-execution-implementation-plan-not-run-v1.json",
        "ccb": "config/hil/hil-execution-ccb-decision-not-run-v1.json",
        "requirement": "VER-002",
        "roles": ["implementation-lead", "safety-plan-reviewer", "test-plan-reviewer"],
    },
}


class ImplementationPlanError(ValueError):
    """The implementation plan is incomplete, stale, conflicted, or unsafe."""


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise ImplementationPlanError(f"{control}: {message}")


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
        raise ImplementationPlanError("implementation-plan root must be an object")
    return value


def _validate_ccb_link(data: dict, root: Path, session_dir: Path, control: str) -> dict:
    require(data.get("ccb_record_path") == "ccb-decision-record.json", control, "unexpected CCB record path")
    path = _safe_local(session_dir, data.get("ccb_record_path"))
    require(path is not None, control, "CCB record is missing or unsafe")
    digest = str(data.get("ccb_record_sha256", ""))
    require(DIGEST.fullmatch(digest) is not None and digest == hashlib.sha256(path.read_bytes()).hexdigest(), control, "CCB record digest is malformed or stale")
    record = validate_ccb_record(root, session_dir)
    require(record.get("ccb_decision") == "APPROVE_IMPLEMENTATION_PLANNING" and record.get("decision") == "NO_GO_IMPLEMENTATION_PLAN_NOT_CREATED", control, "CCB decision does not permit implementation planning")
    return record


def _all_prior_people(session_dir: Path, ccb_record: dict) -> set[str]:
    decision_path = _safe_local(session_dir, ccb_record.get("decision_path"))
    require(decision_path is not None, "PIP-26", "source CCB decision is unavailable")
    decision = load_ccb(decision_path)
    request_record_path = _safe_local(session_dir, decision.get("request_record_path"))
    require(request_record_path is not None, "PIP-26", "request record is unavailable")
    request_record = load(request_record_path)
    names = _prior_people(session_dir, request_record)
    names.update(
        row["name"].strip().casefold()
        for row in decision.get("board_members", [])
        if isinstance(row, dict) and isinstance(row.get("name"), str)
    )
    return names


def validate_plan(data: dict, root: Path, session_dir: Path, allow_not_run: bool = False) -> dict:
    require(data.get("schema") == "avionix-next.product-implementation-plan.v1", "PIP-01", "wrong plan schema")
    require(set(data) == ROOT_KEYS, "PIP-02", "plan properties differ from the closed schema")
    domain = data.get("domain")
    require(domain in PLAN_SPECS, "PIP-03", "unknown product domain")
    spec = PLAN_SPECS[domain]
    ccb_template = _repo_file(root, data.get("ccb_template_path"))
    require(data.get("ccb_template_path") == spec["ccb"] and ccb_template is not None, "PIP-04", "CCB template path is unsafe or unexpected")
    template_digest = str(data.get("ccb_template_sha256", ""))
    require(DIGEST.fullmatch(template_digest) is not None and template_digest == hashlib.sha256(ccb_template.read_bytes()).hexdigest(), "PIP-05", "CCB template digest is malformed or stale")
    require(data.get("required_plan_roles") == spec["roles"], "PIP-06", "plan reviewer roles changed")
    require(data.get("implementation_authorized") is False and data.get("deployment_allowed") is False and data.get("flight_approval") is False, "PIP-07", "plan claims forbidden operational authority")

    empty_scalar = ("plan_id", "owner", "prepared_utc", "baseline_branch", "baseline_commit", "objective", "verification_plan", "rollback_plan", "deployment_plan")
    empty_list = ("in_scope_requirements", "in_scope_artifacts", "in_scope_nodes", "out_of_scope", "work_packages", "hazard_controls", "plan_reviewers")
    if data.get("status") == "NOT_RUN" and allow_not_run:
        require(all(data.get(key) is None for key in empty_scalar), "PIP-08", "NOT_RUN plan contains scalar claims")
        require(data.get("ccb_record_path") is None and data.get("ccb_record_sha256") == "UNRESOLVED", "PIP-09", "NOT_RUN plan claims a CCB record")
        require(all(data.get(key) == [] for key in empty_list), "PIP-10", "NOT_RUN plan contains scope or reviewers")
        require(data.get("plan_complete") is False and data.get("decision") == "NO_GO_NOT_RUN", "PIP-11", "NOT_RUN plan is not fail-closed")
        return {"schema": "avionix-next.product-implementation-plan-validation.v1", "status": "PASS", "domain": domain, "execution_status": "NOT_RUN", "decision": "NO_GO_NOT_RUN", "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}

    if data.get("status") == "PREPARED_NOT_RUN" and allow_not_run:
        ccb_record = _validate_ccb_link(data, root, session_dir, "PIP-09")
        require(ccb_record.get("domain") == domain, "PIP-10", "CCB decision belongs to another domain")
        require(all(data.get(key) is None for key in empty_scalar), "PIP-11", "prepared plan contains scalar claims")
        require(all(data.get(key) == [] for key in empty_list), "PIP-12", "prepared plan contains scope or reviewers")
        require(data.get("plan_complete") is False and data.get("decision") == "NO_GO_PENDING_IMPLEMENTATION_PLAN", "PIP-13", "prepared plan is not fail-closed")
        return {"schema": "avionix-next.product-implementation-plan-validation.v1", "status": "PASS", "domain": domain, "execution_status": "PREPARED_NOT_RUN", "decision": "NO_GO_PENDING_IMPLEMENTATION_PLAN", "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}

    require(data.get("status") == "READY_FOR_PLAN_VALIDATION", "PIP-08", "plan is not explicitly ready")
    ccb_record = _validate_ccb_link(data, root, session_dir, "PIP-09")
    require(ccb_record.get("domain") == domain, "PIP-10", "CCB decision belongs to another domain")
    require(PLAN_ID.fullmatch(str(data.get("plan_id", ""))) is not None, "PIP-11", "plan ID is invalid")
    require(isinstance(data.get("owner"), str) and data["owner"].strip() and _utc(data.get("prepared_utc")), "PIP-12", "owner or preparation timestamp is invalid")
    require(isinstance(data.get("baseline_branch"), str) and data["baseline_branch"].strip() and COMMIT.fullmatch(str(data.get("baseline_commit", ""))) is not None, "PIP-13", "baseline branch or commit is invalid")
    for key in ("objective", "verification_plan", "rollback_plan", "deployment_plan"):
        require(isinstance(data.get(key), str) and data[key].strip(), "PIP-14", f"{key} is missing")
    requirements = data.get("in_scope_requirements")
    require(isinstance(requirements, list) and spec["requirement"] in requirements and len(requirements) == len(set(requirements)), "PIP-15", "in-scope requirements are incomplete or duplicated")
    require(all(isinstance(item, str) and re.fullmatch(r"[A-Z][A-Z0-9-]*-[0-9]{3}", item) for item in requirements), "PIP-16", "in-scope requirement ID is malformed")
    artifacts = data.get("in_scope_artifacts")
    require(isinstance(artifacts, list) and artifacts and len(artifacts) == len(set(artifacts)) and all(_safe_declared_path(item) for item in artifacts), "PIP-17", "in-scope artifacts are empty, duplicated or unsafe")
    for key in ("in_scope_nodes", "out_of_scope", "hazard_controls"):
        values = data.get(key)
        require(isinstance(values, list) and values and all(isinstance(item, str) and item.strip() for item in values), "PIP-18", f"{key} are empty or invalid")
    work_packages = data.get("work_packages")
    require(isinstance(work_packages, list) and work_packages, "PIP-19", "work packages are missing")
    work_ids: list[str] = []
    for package in work_packages:
        require(isinstance(package, dict) and set(package) == WORK_KEYS, "PIP-20", "work-package properties differ from the closed schema")
        require(isinstance(package.get("id"), str) and re.fullmatch(r"WP-[A-Z0-9][A-Z0-9._-]{2,31}", package["id"]), "PIP-21", "work-package ID is invalid")
        work_ids.append(package["id"])
        require(isinstance(package.get("description"), str) and package["description"].strip(), "PIP-22", "work-package description is missing")
        require(isinstance(package.get("artifacts"), list) and package["artifacts"] and all(_safe_declared_path(item) for item in package["artifacts"]), "PIP-23", "work-package artifacts are empty or unsafe")
        for key in ("verification", "evidence", "exit_criteria"):
            require(isinstance(package.get(key), str) and package[key].strip(), "PIP-24", f"work-package {key} is missing")
    require(len(work_ids) == len(set(work_ids)), "PIP-25", "work-package IDs are duplicated")
    reviewers = data.get("plan_reviewers")
    require(isinstance(reviewers, list) and [row.get("role") for row in reviewers if isinstance(row, dict)] == spec["roles"], "PIP-26", "plan reviewers are incomplete or out of order")
    names: list[str] = []
    for reviewer in reviewers:
        require(isinstance(reviewer, dict) and set(reviewer) == REVIEWER_KEYS, "PIP-27", "reviewer properties differ from the closed schema")
        require(isinstance(reviewer.get("name"), str) and reviewer["name"].strip() and _utc(reviewer.get("reviewed_utc")), "PIP-28", "reviewer identity or timestamp is invalid")
        names.append(reviewer["name"].strip().casefold())
        signature = _safe_local(session_dir, reviewer.get("signature_artifact"))
        require(signature is not None, "PIP-29", f"{reviewer.get('role')} signature is missing or unsafe")
        signature_digest = str(reviewer.get("signature_sha256", ""))
        require(DIGEST.fullmatch(signature_digest) is not None and signature_digest == hashlib.sha256(signature.read_bytes()).hexdigest(), "PIP-29", f"{reviewer['role']} signature digest is malformed or stale")
    require(len(set(names)) == len(spec["roles"]), "PIP-30", "plan reviewers must have distinct identities")
    require(not _all_prior_people(session_dir, ccb_record).intersection(names), "PIP-31", "plan reviewers must be independent from all prior authorities")
    require(data.get("plan_complete") is True and data.get("decision") == "IMPLEMENTATION_PLAN_RECORDED_PENDING_AUTHORIZATION", "PIP-32", "plan completion or decision is invalid")
    return {"schema": "avionix-next.product-implementation-plan-validation.v1", "status": "PASS", "domain": domain, "execution_status": "PLAN_COMPLETE", "plan_id": data["plan_id"], "work_packages": len(work_packages), "reviewers": len(reviewers), "decision": data["decision"], "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}


def validate_templates(root: Path = ROOT) -> dict:
    for domain, spec in PLAN_SPECS.items():
        path = root / spec["template"]
        report = validate_plan(load(path), root, path.parent, allow_not_run=True)
        require(report["domain"] == domain, "PIP-33", "template domain mismatch")
    return {"schema": "avionix-next.product-implementation-plan-templates-validation.v1", "status": "PASS", "templates": 5, "execution_status": "NOT_RUN", "decision": "NO_GO_PENDING_IMPLEMENTATION_PLAN", "implementation_authorized": False, "deployment_allowed": False, "flight_approval": False}


def init_plan(root: Path, session_dir: Path) -> dict:
    ccb_record = validate_ccb_record(root, session_dir)
    require(ccb_record.get("ccb_decision") == "APPROVE_IMPLEMENTATION_PLANNING" and ccb_record.get("decision") == "NO_GO_IMPLEMENTATION_PLAN_NOT_CREATED", "PIP-34", "CCB record does not permit implementation planning")
    domain = ccb_record["domain"]
    output = session_dir / "implementation-plan.json"
    if output.exists():
        raise ImplementationPlanError("implementation plan already exists; plans are never overwritten")
    data = load(root / PLAN_SPECS[domain]["template"])
    data["status"] = "PREPARED_NOT_RUN"
    data["ccb_record_path"] = "ccb-decision-record.json"
    data["ccb_record_sha256"] = hashlib.sha256((session_dir / "ccb-decision-record.json").read_bytes()).hexdigest()
    data["decision"] = "NO_GO_PENDING_IMPLEMENTATION_PLAN"
    _atomic_json(output, data)
    return validate_plan(data, root, session_dir, allow_not_run=True)


def finalize_plan(root: Path, session_dir: Path) -> dict:
    plan_path = session_dir / "implementation-plan.json"
    data = load(plan_path)
    report = validate_plan(data, root, session_dir)
    output = session_dir / "implementation-plan-record.json"
    if output.exists():
        raise ImplementationPlanError("implementation plan record already exists; records are immutable")
    record = {
        "schema": "avionix-next.product-implementation-plan-record.v1",
        "domain": report["domain"],
        "status": "PLAN_RECORDED_AUTHORIZATION_REQUIRED",
        "plan_path": plan_path.name,
        "plan_sha256": hashlib.sha256(plan_path.read_bytes()).hexdigest(),
        "ccb_record_path": data["ccb_record_path"],
        "ccb_record_sha256": data["ccb_record_sha256"],
        "plan_id": data["plan_id"],
        "baseline_branch": data["baseline_branch"],
        "baseline_commit": data["baseline_commit"],
        "objective": data["objective"],
        "in_scope_requirements": data["in_scope_requirements"],
        "in_scope_artifacts": data["in_scope_artifacts"],
        "in_scope_nodes": data["in_scope_nodes"],
        "work_packages": data["work_packages"],
        "hazard_controls": data["hazard_controls"],
        "verification_plan": data["verification_plan"],
        "rollback_plan": data["rollback_plan"],
        "deployment_plan": data["deployment_plan"],
        "reviewers": report["reviewers"],
        "implementation_authorized": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "next_action": "CONVENE_SEPARATE_IMPLEMENTATION_AUTHORIZATION_REVIEW",
        "decision": "NO_GO_IMPLEMENTATION_AUTHORIZATION_NOT_CREATED",
    }
    _atomic_json(output, record)
    return validate_record(root, session_dir, record)


def validate_record(root: Path, session_dir: Path, record: dict | None = None) -> dict:
    data = record if record is not None else load(session_dir / "implementation-plan-record.json")
    require(set(data) == RECORD_KEYS and data.get("schema") == "avionix-next.product-implementation-plan-record.v1", "PIP-35", "record properties or schema are invalid")
    plan_path = _safe_local(session_dir, data.get("plan_path"))
    require(plan_path is not None and data.get("plan_sha256") == hashlib.sha256(plan_path.read_bytes()).hexdigest(), "PIP-36", "record plan source is missing, unsafe or modified")
    plan = load(plan_path)
    report = validate_plan(plan, root, session_dir)
    checks = (
        data.get("domain") == report["domain"],
        data.get("status") == "PLAN_RECORDED_AUTHORIZATION_REQUIRED",
        data.get("ccb_record_path") == plan["ccb_record_path"],
        data.get("ccb_record_sha256") == plan["ccb_record_sha256"],
        data.get("plan_id") == plan["plan_id"],
        data.get("baseline_branch") == plan["baseline_branch"],
        data.get("baseline_commit") == plan["baseline_commit"],
        data.get("objective") == plan["objective"],
        data.get("in_scope_requirements") == plan["in_scope_requirements"],
        data.get("in_scope_artifacts") == plan["in_scope_artifacts"],
        data.get("in_scope_nodes") == plan["in_scope_nodes"],
        data.get("work_packages") == plan["work_packages"],
        data.get("hazard_controls") == plan["hazard_controls"],
        data.get("verification_plan") == plan["verification_plan"],
        data.get("rollback_plan") == plan["rollback_plan"],
        data.get("deployment_plan") == plan["deployment_plan"],
        data.get("reviewers") == report["reviewers"],
        data.get("implementation_authorized") is False,
        data.get("deployment_allowed") is False,
        data.get("flight_approval") is False,
        data.get("next_action") == "CONVENE_SEPARATE_IMPLEMENTATION_AUTHORIZATION_REVIEW",
        data.get("decision") == "NO_GO_IMPLEMENTATION_AUTHORIZATION_NOT_CREATED",
    )
    require(all(checks), "PIP-37", "record is inconsistent or claims excess authority")
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
            report = init_plan(args.root, args.session_dir)
        elif args.command == "validate":
            report = validate_plan(load(args.session_dir / "implementation-plan.json"), args.root, args.session_dir, allow_not_run=True)
        elif args.command == "finalize":
            report = finalize_plan(args.root, args.session_dir)
        else:
            report = validate_record(args.root, args.session_dir)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        print(f"AVIONIX product implementation plan: FAIL: {exc}")
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
