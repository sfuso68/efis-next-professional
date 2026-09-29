#!/usr/bin/env python3
"""Guide product evidence capture and review without touching hardware."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import tempfile

try:
    from .package_product_evidence import DOMAIN_SPECS, _artifact, assemble, load_results, validate_results
    from .validate_product_evidence_bundle import validate_file as validate_bundle_file
    from .validate_product_independent_review import REVIEW_SPECS, load as load_review, validate_review
except ImportError:
    from package_product_evidence import DOMAIN_SPECS, _artifact, assemble, load_results, validate_results
    from validate_product_evidence_bundle import validate_file as validate_bundle_file
    from validate_product_independent_review import REVIEW_SPECS, load as load_review, validate_review


ROOT = Path(__file__).resolve().parents[2]
RESULT_TEMPLATES = {
    "engine": "config/product/engine-source-review-results-not-run-v1.json",
    "sku": "config/product/sku-release-review-results-not-run-v1.json",
    "fpv": "config/video/fpv-benchmark-review-results-not-run-v1.json",
    "voice": "config/voice/voice-cabin-review-results-not-run-v1.json",
    "hil": "config/hil/hil-execution-review-results-not-run-v1.json",
}
OUTCOME_KEYS = {"schema", "domain", "status", "review_path", "review_sha256", "evidence_bundle_path", "evidence_bundle_sha256", "review_decision", "reviewers", "findings", "next_gate_eligible", "hardware_verified", "deployment_allowed", "flight_approval", "decision"}


def _atomic_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent, delete=False) as handle:
        temporary = Path(handle.name)
        json.dump(data, handle, indent=2)
        handle.write("\n")
    temporary.replace(path)


def _safe_local(base: Path, raw: object) -> Path | None:
    if not isinstance(raw, str) or not raw.strip():
        return None
    relative = Path(raw)
    if relative.is_absolute() or ".." in relative.parts:
        return None
    candidate = base / relative
    resolved = candidate.resolve()
    if base.resolve() not in resolved.parents or candidate.is_symlink() or not resolved.is_file() or resolved.stat().st_size == 0:
        return None
    return resolved


def init_session(root: Path, domain: str, session_dir: Path) -> dict:
    if domain not in RESULT_TEMPLATES:
        raise ValueError("unknown product domain")
    if session_dir.exists():
        raise ValueError("session directory already exists; capture sessions are never overwritten")
    template = root / RESULT_TEMPLATES[domain]
    data = load_results(template)
    session_dir.mkdir(parents=True)
    (session_dir / "artifacts").mkdir()
    _atomic_json(session_dir / "results.json", data)
    return {
        "schema": "avionix-next.product-evidence-session.v1",
        "status": "NOT_RUN",
        "domain": domain,
        "session_dir": str(session_dir.resolve()),
        "results": "results.json",
        "artifact_directory": "artifacts",
        "hardware_verified": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "decision": "NO_GO_NOT_RUN",
    }


def session_status(root: Path, session_dir: Path) -> dict:
    results_path = session_dir / "results.json"
    data = load_results(results_path)
    domain = data.get("domain")
    if domain not in DOMAIN_SPECS:
        raise ValueError("results contain an unknown domain")
    spec = DOMAIN_SPECS[domain]
    probe = copy.deepcopy(data)
    probe["status"] = "NOT_RUN"
    probe["operator_confirmed"] = False
    probe["operator"] = None
    probe["captured_utc"] = None
    probe["decision"] = "NO_GO_NOT_RUN"
    if isinstance(probe.get("subjects"), list):
        for row in probe["subjects"]:
            if isinstance(row, dict):
                row["artifacts"] = {}
    probe["global_artifacts"] = {}
    contract_errors = validate_results(probe, root, session_dir, allow_not_run=True)
    missing: list[str] = []
    unsafe: list[str] = []
    subjects = data.get("subjects") if isinstance(data.get("subjects"), list) else []
    by_id = {row.get("id"): row for row in subjects if isinstance(row, dict)}
    for subject_id in spec["subjects"]:
        artifacts = by_id.get(subject_id, {}).get("artifacts", {})
        artifacts = artifacts if isinstance(artifacts, dict) else {}
        for kind in spec["artifacts"]:
            if kind not in artifacts:
                missing.append(f"{subject_id}/{kind}")
            elif _artifact(session_dir, artifacts[kind]) is None:
                unsafe.append(f"{subject_id}/{kind}")
    global_artifacts = data.get("global_artifacts") if isinstance(data.get("global_artifacts"), dict) else {}
    for kind in spec["global"]:
        if kind not in global_artifacts:
            missing.append(f"global/{kind}")
        elif _artifact(session_dir, global_artifacts[kind]) is None:
            unsafe.append(f"global/{kind}")
    ready_errors = validate_results(data, root, session_dir) if data.get("status") == "READY_FOR_PACKAGING" else []
    ready = not contract_errors and not missing and not unsafe and data.get("status") == "READY_FOR_PACKAGING" and not ready_errors
    if ready:
        state = "READY_FOR_PACKAGING"
        decision = "NO_GO_PENDING_INDEPENDENT_REVIEW"
    elif not missing and not unsafe and not contract_errors:
        state = "AWAITING_OPERATOR_CONFIRMATION"
        decision = "NO_GO_AWAITING_OPERATOR_CONFIRMATION"
    elif data.get("status") == "NOT_RUN" and all(not row.get("artifacts") for row in subjects if isinstance(row, dict)) and not global_artifacts:
        state = "NOT_RUN"
        decision = "NO_GO_NOT_RUN"
    else:
        state = "CAPTURING"
        decision = "NO_GO_SESSION_INCOMPLETE"
    return {
        "schema": "avionix-next.product-evidence-session-status.v1",
        "status": state,
        "domain": domain,
        "missing_artifacts": missing,
        "unsafe_artifacts": unsafe,
        "contract_errors": contract_errors,
        "ready_errors": ready_errors,
        "ready_for_packaging": ready,
        "hardware_verified": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "decision": decision,
    }


def finalize_evidence(root: Path, session_dir: Path) -> dict:
    status = session_status(root, session_dir)
    if not status["ready_for_packaging"]:
        raise ValueError("session is not ready for packaging")
    output = session_dir / "evidence-bundle.json"
    errors = assemble(root, session_dir / "results.json", output)
    if errors:
        raise ValueError("; ".join(errors))
    report = validate_bundle_file(root, output)
    return {"schema": "avionix-next.product-evidence-session-finalization.v1", "status": "PACKAGED_PENDING_REVIEW", "domain": report["domain"], "bundle": output.name, "bundle_sha256": hashlib.sha256(output.read_bytes()).hexdigest(), "hardware_verified": False, "deployment_allowed": False, "flight_approval": False, "decision": "NO_GO_PENDING_INDEPENDENT_REVIEW"}


def init_review(root: Path, session_dir: Path) -> dict:
    bundle = session_dir / "evidence-bundle.json"
    report = validate_bundle_file(root, bundle)
    domain = report["domain"]
    output = session_dir / "independent-review.json"
    if output.exists():
        raise ValueError("independent review already exists; review sessions are never overwritten")
    template = root / REVIEW_SPECS[domain]["template"]
    data = load_review(template)
    data["status"] = "PREPARED_NOT_RUN"
    data["evidence_bundle_path"] = bundle.name
    data["evidence_bundle_sha256"] = hashlib.sha256(bundle.read_bytes()).hexdigest()
    data["decision"] = "NO_GO_PENDING_INDEPENDENT_REVIEW"
    _atomic_json(output, data)
    prepared = validate_review(data, root, session_dir, allow_not_run=True)
    return {"schema": "avionix-next.product-review-session.v1", "status": prepared["execution_status"], "domain": domain, "review": output.name, "hardware_verified": False, "deployment_allowed": False, "flight_approval": False, "decision": prepared["decision"]}


def finalize_review(root: Path, session_dir: Path) -> dict:
    review_path = session_dir / "independent-review.json"
    data = load_review(review_path)
    report = validate_review(data, root, session_dir)
    output = session_dir / "review-outcome.json"
    if output.exists():
        raise ValueError("review outcome already exists; outcomes are immutable")
    outcome = {
        "schema": "avionix-next.product-review-outcome.v1",
        "domain": report["domain"],
        "status": "REVIEWED_PENDING_GATE_DISPOSITION",
        "review_path": review_path.name,
        "review_sha256": hashlib.sha256(review_path.read_bytes()).hexdigest(),
        "evidence_bundle_path": data["evidence_bundle_path"],
        "evidence_bundle_sha256": data["evidence_bundle_sha256"],
        "review_decision": report["decision"],
        "reviewers": report["reviewers"],
        "findings": report["findings"],
        "next_gate_eligible": report["decision"] == "EVIDENCE_ACCEPTED_FOR_NEXT_GATE",
        "hardware_verified": False,
        "deployment_allowed": False,
        "flight_approval": False,
        "decision": "NO_GO_PENDING_GATE_DISPOSITION" if report["decision"] == "EVIDENCE_ACCEPTED_FOR_NEXT_GATE" else "NO_GO_REVIEW_FINDINGS",
    }
    _atomic_json(output, outcome)
    validate_outcome(root, session_dir, outcome)
    return outcome


def validate_outcome(root: Path, session_dir: Path, data: dict | None = None) -> dict:
    outcome_path = session_dir / "review-outcome.json"
    outcome = data if data is not None else json.loads(outcome_path.read_text(encoding="utf-8"))
    if not isinstance(outcome, dict) or set(outcome) != OUTCOME_KEYS:
        raise ValueError("review outcome properties differ from the closed schema")
    review_path = _safe_local(session_dir, outcome.get("review_path"))
    if review_path is None or outcome.get("review_sha256") != hashlib.sha256(review_path.read_bytes()).hexdigest():
        raise ValueError("review outcome source is missing, unsafe or modified")
    review = load_review(review_path)
    report = validate_review(review, root, session_dir)
    expected_eligible = report["decision"] == "EVIDENCE_ACCEPTED_FOR_NEXT_GATE"
    expected_decision = "NO_GO_PENDING_GATE_DISPOSITION" if expected_eligible else "NO_GO_REVIEW_FINDINGS"
    checks = (
        outcome.get("schema") == "avionix-next.product-review-outcome.v1",
        outcome.get("domain") == report["domain"],
        outcome.get("status") == "REVIEWED_PENDING_GATE_DISPOSITION",
        outcome.get("evidence_bundle_path") == review["evidence_bundle_path"],
        outcome.get("evidence_bundle_sha256") == review["evidence_bundle_sha256"],
        outcome.get("review_decision") == report["decision"],
        outcome.get("reviewers") == report["reviewers"],
        outcome.get("findings") == report["findings"],
        outcome.get("next_gate_eligible") is expected_eligible,
        outcome.get("hardware_verified") is False,
        outcome.get("deployment_allowed") is False,
        outcome.get("flight_approval") is False,
        outcome.get("decision") == expected_decision,
    )
    if not all(checks):
        raise ValueError("review outcome is inconsistent or claims excess authority")
    return outcome


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    subparsers = parser.add_subparsers(dest="command", required=True)
    init_parser = subparsers.add_parser("init")
    init_parser.add_argument("--domain", required=True, choices=sorted(RESULT_TEMPLATES))
    init_parser.add_argument("--session-dir", required=True, type=Path)
    for command in ("status", "finalize-evidence", "init-review", "finalize-review", "verify-review-outcome"):
        item = subparsers.add_parser(command)
        item.add_argument("--session-dir", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.command == "init":
            report = init_session(args.root, args.domain, args.session_dir)
        elif args.command == "status":
            report = session_status(args.root, args.session_dir)
        elif args.command == "finalize-evidence":
            report = finalize_evidence(args.root, args.session_dir)
        elif args.command == "init-review":
            report = init_review(args.root, args.session_dir)
        elif args.command == "finalize-review":
            report = finalize_review(args.root, args.session_dir)
        else:
            report = validate_outcome(args.root, args.session_dir)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        print(f"AVIONIX product evidence session: FAIL: {exc}")
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
