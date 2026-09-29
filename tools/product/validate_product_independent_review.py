#!/usr/bin/env python3
"""Validate product evidence review records without granting operational authority."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys

try:
    from .validate_product_evidence_bundle import validate_file as validate_bundle_file
except ImportError:
    from validate_product_evidence_bundle import validate_file as validate_bundle_file


ROOT = Path(__file__).resolve().parents[2]
DIGEST = re.compile(r"[0-9a-f]{64}")
ROOT_KEYS = {"schema", "domain", "status", "coordinator", "captured_utc", "evidence_bundle_path", "evidence_bundle_sha256", "review_gate_path", "review_gate_sha256", "required_review_roles", "required_findings", "reviewers", "findings", "limitations", "evidence_integrity_verified", "review_complete", "hardware_verified", "deployment_allowed", "flight_approval", "decision"}
REVIEWER_KEYS = {"role", "name", "reviewed_utc", "signature_artifact", "signature_sha256"}
FINDING_KEYS = {"result", "note"}
REVIEW_SPECS = {
    "engine": {
        "template": "config/product/engine-source-independent-review-not-run-v1.json",
        "gate": "config/product/engine-source-review-gate-v1.json",
        "roles": ["engine-owner", "engineering-reviewer", "configuration-controller"],
        "findings": ["serial-applicability", "manual-revision", "limit-page-traceability", "ul-uls-separation", "legacy-limit-disposition"],
    },
    "sku": {
        "template": "config/product/sku-release-independent-review-not-run-v1.json",
        "gate": "config/product/sku-release-review-gate-v1.json",
        "roles": ["product-owner", "safety-reviewer", "release-manager"],
        "findings": ["capability-allowlist", "node-allocation", "single-codebase", "unknown-feature-rejection", "inflight-profile-lock"],
    },
    "fpv": {
        "template": "config/video/fpv-benchmark-independent-review-not-run-v1.json",
        "gate": "config/video/fpv-benchmark-review-gate-v1.json",
        "roles": ["video-owner", "network-reviewer", "thermal-reviewer"],
        "findings": ["four-feed-capacity", "latency-and-drops", "decode-thermal-margin", "feed-loss-recovery", "domain-isolation", "telemetry-sidecar-sync"],
    },
    "voice": {
        "template": "config/voice/voice-cabin-independent-review-not-run-v1.json",
        "gate": "config/voice/voice-cabin-review-gate-v1.json",
        "roles": ["phraseology-reviewer", "audio-reviewer", "pilot-reviewer"],
        "findings": ["source-authority", "critical-clip-integrity", "offline-tts-provenance", "cabin-intelligibility", "ptt-isolation", "cas-priority"],
    },
    "hil": {
        "template": "config/hil/hil-execution-independent-review-not-run-v1.json",
        "gate": "config/hil/hil-execution-review-gate-v1.json",
        "roles": ["hil-operator", "safety-reviewer", "evidence-reviewer"],
        "findings": ["fixture-isolation", "input-digest-resolution", "deterministic-replay", "fault-containment", "expected-outcome", "artifact-completeness"],
    },
}


class IndependentReviewError(ValueError):
    """A review record is incomplete, unsafe, stale, or over-authoritative."""


def require(condition: bool, control: str, message: str) -> None:
    if not condition:
        raise IndependentReviewError(f"{control}: {message}")


def _utc(value: object) -> bool:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        return parsed.utcoffset() is not None and parsed.utcoffset().total_seconds() == 0
    except (TypeError, ValueError):
        return False


def _local_file(base: Path, raw: object) -> Path | None:
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
        raise IndependentReviewError("review root must be an object")
    return value


def validate_review(data: dict, root: Path, review_dir: Path, allow_not_run: bool = False) -> dict:
    require(data.get("schema") == "avionix-next.product-independent-review.v1", "PIR-01", "wrong review schema")
    require(set(data) == ROOT_KEYS, "PIR-02", "review properties differ from the closed schema")
    domain = data.get("domain")
    require(domain in REVIEW_SPECS, "PIR-03", "unknown product domain")
    spec = REVIEW_SPECS[domain]
    gate = _repo_file(root, data.get("review_gate_path"))
    require(data.get("review_gate_path") == spec["gate"] and gate is not None, "PIR-04", "review gate path is missing, unsafe or unexpected")
    require(DIGEST.fullmatch(str(data.get("review_gate_sha256", ""))) is not None and data["review_gate_sha256"] == hashlib.sha256(gate.read_bytes()).hexdigest(), "PIR-05", "review gate digest is malformed or stale")
    gate_data = load(gate)
    require(gate_data.get("status") == "PREPARED_SW_NOT_RUN" and gate_data.get("flight_approval") is False, "PIR-06", "review gate is not fail-closed")
    require(data.get("required_review_roles") == spec["roles"] == gate_data.get("required_review_roles"), "PIR-07", "review roles differ from the frozen gate")
    require(data.get("required_findings") == spec["findings"] == gate_data.get("required_findings"), "PIR-08", "required findings differ from the frozen gate")
    limitations = data.get("limitations")
    require(isinstance(limitations, list) and all(isinstance(item, str) and item.strip() for item in limitations), "PIR-09", "limitations are invalid")
    require(data.get("hardware_verified") is False and data.get("deployment_allowed") is False and data.get("flight_approval") is False, "PIR-10", "review claims forbidden operational authority")

    if data.get("status") == "NOT_RUN" and allow_not_run:
        require(data.get("coordinator") is None and data.get("captured_utc") is None, "PIR-11", "NOT_RUN review must be unattributed")
        require(data.get("evidence_bundle_path") is None and data.get("evidence_bundle_sha256") == "UNRESOLVED", "PIR-12", "NOT_RUN review must not claim an evidence bundle")
        require(data.get("reviewers") == [] and data.get("findings") == {}, "PIR-13", "NOT_RUN review must not contain reviewers or findings")
        require(data.get("evidence_integrity_verified") is False and data.get("review_complete") is False and data.get("decision") == "NO_GO_NOT_RUN", "PIR-14", "NOT_RUN review decision is not fail-closed")
        return {"schema": "avionix-next.product-independent-review-validation.v1", "status": "PASS", "domain": domain, "execution_status": "NOT_RUN", "decision": "NO_GO_NOT_RUN", "hardware_verified": False, "deployment_allowed": False, "flight_approval": False}

    if data.get("status") == "PREPARED_NOT_RUN" and allow_not_run:
        require(data.get("coordinator") is None and data.get("captured_utc") is None, "PIR-11", "prepared review must remain unattributed")
        bundle = _local_file(review_dir, data.get("evidence_bundle_path"))
        require(bundle is not None, "PIR-12", "prepared review evidence bundle is missing, empty or unsafe")
        require(DIGEST.fullmatch(str(data.get("evidence_bundle_sha256", ""))) is not None and data["evidence_bundle_sha256"] == hashlib.sha256(bundle.read_bytes()).hexdigest(), "PIR-13", "prepared review bundle digest is malformed or stale")
        bundle_report = validate_bundle_file(root, bundle)
        require(bundle_report.get("domain") == domain, "PIR-14", "prepared review bundle belongs to another domain")
        require(data.get("reviewers") == [] and data.get("findings") == {}, "PIR-15", "prepared review must not contain reviewers or findings")
        require(data.get("evidence_integrity_verified") is False and data.get("review_complete") is False and data.get("decision") == "NO_GO_PENDING_INDEPENDENT_REVIEW", "PIR-16", "prepared review decision is not fail-closed")
        return {"schema": "avionix-next.product-independent-review-validation.v1", "status": "PASS", "domain": domain, "execution_status": "PREPARED_NOT_RUN", "decision": "NO_GO_PENDING_INDEPENDENT_REVIEW", "hardware_verified": False, "deployment_allowed": False, "flight_approval": False}

    require(data.get("status") == "READY_FOR_REVIEW_VALIDATION", "PIR-11", "review is not explicitly ready for validation")
    require(isinstance(data.get("coordinator"), str) and data["coordinator"].strip() and _utc(data.get("captured_utc")), "PIR-12", "coordinator or UTC timestamp is invalid")
    bundle = _local_file(review_dir, data.get("evidence_bundle_path"))
    require(bundle is not None, "PIR-13", "evidence bundle is missing, empty or unsafe")
    require(DIGEST.fullmatch(str(data.get("evidence_bundle_sha256", ""))) is not None and data["evidence_bundle_sha256"] == hashlib.sha256(bundle.read_bytes()).hexdigest(), "PIR-14", "evidence bundle digest is malformed or stale")
    bundle_report = validate_bundle_file(root, bundle)
    require(bundle_report.get("domain") == domain, "PIR-15", "evidence bundle belongs to another domain")
    reviewers = data.get("reviewers")
    require(isinstance(reviewers, list) and [row.get("role") for row in reviewers if isinstance(row, dict)] == spec["roles"], "PIR-16", "reviewer roles are incomplete or out of order")
    names: list[str] = []
    for reviewer in reviewers:
        require(isinstance(reviewer, dict) and set(reviewer) == REVIEWER_KEYS, "PIR-17", "reviewer properties differ from the closed schema")
        require(isinstance(reviewer.get("name"), str) and reviewer["name"].strip() and _utc(reviewer.get("reviewed_utc")), "PIR-18", "reviewer identity or timestamp is invalid")
        names.append(reviewer["name"].strip().casefold())
        signature = _local_file(review_dir, reviewer.get("signature_artifact"))
        require(signature is not None, "PIR-19", f"{reviewer.get('role')} signature artifact is missing or unsafe")
        require(DIGEST.fullmatch(str(reviewer.get("signature_sha256", ""))) is not None and reviewer["signature_sha256"] == hashlib.sha256(signature.read_bytes()).hexdigest(), "PIR-20", f"{reviewer['role']} signature digest is malformed or stale")
    require(len(set(names)) == len(spec["roles"]), "PIR-21", "independent roles require distinct reviewer identities")
    findings = data.get("findings")
    require(isinstance(findings, dict) and list(findings) == spec["findings"], "PIR-22", "findings are incomplete or out of order")
    for finding_id, finding in findings.items():
        require(isinstance(finding, dict) and set(finding) == FINDING_KEYS, "PIR-23", f"{finding_id} properties differ from the closed schema")
        require(finding.get("result") in ("PASS", "FAIL") and isinstance(finding.get("note"), str) and finding["note"].strip(), "PIR-24", f"{finding_id} result or note is invalid")
    all_pass = all(row["result"] == "PASS" for row in findings.values())
    expected_decision = "EVIDENCE_ACCEPTED_FOR_NEXT_GATE" if all_pass else "NO_GO_REVIEW_FINDINGS"
    require(data.get("evidence_integrity_verified") is True and data.get("review_complete") is True, "PIR-25", "review completeness or integrity is not confirmed")
    require(data.get("decision") == expected_decision, "PIR-26", "review decision does not match findings")
    return {"schema": "avionix-next.product-independent-review-validation.v1", "status": "PASS", "domain": domain, "execution_status": "REVIEW_COMPLETE", "reviewers": len(reviewers), "findings": len(findings), "decision": expected_decision, "hardware_verified": False, "deployment_allowed": False, "flight_approval": False}


def validate_templates(root: Path = ROOT) -> dict:
    reports = []
    for domain, spec in REVIEW_SPECS.items():
        path = root / spec["template"]
        report = validate_review(load(path), root, path.parent, allow_not_run=True)
        require(report["domain"] == domain, "PIR-27", "template domain mismatch")
        reports.append(report)
    return {"schema": "avionix-next.product-independent-review-templates-validation.v1", "status": "PASS", "templates": len(reports), "execution_status": "NOT_RUN", "hardware_verified": False, "deployment_allowed": False, "flight_approval": False, "decision": "NO_GO_PENDING_EVIDENCE_AND_REVIEW"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--review", type=Path)
    args = parser.parse_args()
    try:
        report = validate_review(load(args.review), args.root, args.review.resolve().parent) if args.review else validate_templates(args.root)
    except (IndependentReviewError, KeyError, TypeError, OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"AVIONIX product independent review: FAIL: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
